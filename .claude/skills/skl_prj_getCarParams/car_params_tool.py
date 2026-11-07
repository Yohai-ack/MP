"""Car-specific parameters for the A3 solution (TA tool, used by /skl_prj_getCarParams).

Runs on the cart through the ST-LINK (SWD) and its COM port, with the A2 reference solution flashed. Every command
that moves a wheel is run only after Yohai said "ready" (the skill asks). The wheels must be in the air.

  prepare                       flash ass2_sol, task 0 (idle), all four encoders in Encoder Mode TI1 and TI2
  spin N                        motor N (1..4) with its DIR pin = 1 at 30 % for 1 s; prints every counter's change
  cnt                           prints the four encoder counters (TIM1, TIM8, TIM3, TIM4)
  steps X MD ED                 builds a copy of ass2_sol with car X's directions (MD, ED: four signs, e.g. -1,1,1,-1),
                                flashes it and records the 50 % step of each wheel (E.2, tasks 521..524) into
                                assignments/ass3/solution/fullSolution/car_data/carX_step_wK.txt
  write X MD ED CPR [TAUC]      fits K and tau to those steps, designs Kp and Ki (PI zero on the motor pole,
                                closed-loop time constant TAUC, default 0.032 s), computes maxVelCmdNoSat, and
                                writes car X's entry into car_params.c (replacing an old entry of car X; its
                                sensor lines are kept)
  calstart                      flash the A3 solution (ass3_sol) and start task 900: nothing moves but the servo
  servo US                      (task 900) turn the servo to US microseconds (900..2100)
  ir                            (task 900) print the IR pin level, read 6 times over 3 s
  sensors X IR D900,D1500,D2100 write car X's irObstacleLevel (1 high, 0 low) and servoDeg (degrees, + left) into
                                car_params.c
"""
import glob, os, re, shutil, subprocess, sys, time
from pathlib import Path

import numpy as np

REPO = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(REPO / "assignments" / "autotest"))
import assignments_autotest as a                                     # noqa: E402  (Board, symbols, com_port)

ASS2 = REPO / "assignments/ass2/solution/fullSolution/ass2_sol"
ELF2 = ASS2 / "Debug/ass2_sol.elf"
SOL3 = REPO / "assignments/ass3/solution/fullSolution"
DATA = SOL3 / "car_data"
PARAMS_C = SOL3 / "ass3_sol/Core/Src/car_params.c"
ELF3 = SOL3 / "ass3_sol/Debug/ass3_sol.elf"
SENSOR_RE = r"        \.(?:irObstacleLevel|servoUs|servoDeg) .*\n"
SENSOR_DEFAULT = ("        .irObstacleLevel = 1,                          /* NOT YET MEASURED */\n"
                  "        .servoUs         = { 900, 1500, 2100 },\n"
                  "        .servoDeg        = { -60.0f, 0.0f, 60.0f },    /* NOT YET MEASURED */\n")
BUILD = Path(os.environ.get("TEMP", "/tmp")) / "carparams_build"

ENC = [("TIM1", 0x40012C00), ("TIM8", 0x40013400), ("TIM3", 0x40000400), ("TIM4", 0x40000800)]
CCR = [0x40000034, 0x40000038, 0x4000003C, 0x40000040]                     # TIM2 CCR1..4
DIR = [(0x48000800, 2), (0x48000400, 4), (0x48000400, 11), (0x48000400, 13)]  # PC2, PB4, PB11, PB13 (ODR at +0x14)

U_STEP, V_SUPPLY, TS, ULIM = 6.0, 12.0, 0.01, 0.9 * 12.0


def signs(s):
    v = [int(x) for x in s.split(",")]
    assert len(v) == 4 and all(x in (-1, 1) for x in v), "four signs, e.g. -1,1,1,-1"
    return v


def counters(b):
    return [b.read32(base + 0x24) & 0xFFFF for _, base in ENC]


def change(x, y):
    return [((q - p + 32768) % 65536) - 32768 for p, q in zip(x, y)]


# ------------------------------------------------------------------ cart commands
def cmd_prepare(b):
    b.flash(str(ELF2))
    time.sleep(1.5)
    b.write16(a.symbols(str(ELF2))["task"][0], 0)
    time.sleep(0.5)
    for name, base in ENC:
        b.write32(base + 0x08, (b.read32(base + 0x08) & ~7) | 3)          # SMS = 3: TI1 and TI2
    print("ass2_sol flashed, idle, encoders TI1 and TI2:", counters(b))


def cmd_spin(b, n):
    port, pin = DIR[n - 1]
    before = counters(b)
    b.write32(port + 0x14, b.read32(port + 0x14) | (1 << pin))            # DIR pin = 1
    b.write32(CCR[n - 1], 3000)                                            # 30 %
    time.sleep(1.0)
    b.write32(CCR[n - 1], 0)
    time.sleep(0.5)
    print("motor %d, DIR = 1: counter change" % n, dict(zip([e[0] for e in ENC], change(before, counters(b)))))


def toolchain():
    plug = sorted(glob.glob(r"C:\ST\STM32CubeIDE_*\STM32CubeIDE\plugins"))
    if not plug:
        sys.exit("no STM32CubeIDE toolchain found (C:\\ST\\STM32CubeIDE_*): build on the PC that has it")
    gcc = glob.glob(plug[-1] + r"\*gnu-tools-for-stm32*win32*\tools\bin")
    mk = glob.glob(plug[-1] + r"\*externaltools.make.win32*\tools\bin")
    return gcc[0] + os.pathsep + mk[0] + os.pathsep + os.environ["PATH"]


def cmd_steps(b, car, md, ed):
    # a fresh copy of ass2_sol with this car's forward levels and count signs; a CLEAN build (copied object files
    # look newer than the patched source, so a plain make would keep the old table)
    shutil.rmtree(BUILD, ignore_errors=True)
    shutil.copytree(ASS2, BUILD, ignore=shutil.ignore_patterns("*.o", "*.d", "*.elf", "Release"))
    src = BUILD / "Core/Src/ass2_common.c"
    s = src.read_text()
    for i, tim in enumerate(("htim1", "htim8", "htim3", "htim4")):
        pat = r"(DIR%d_Pin, )GPIO_PIN_(?:SET|RESET),(\s*)&%s, [+-]1" % (i + 1, tim)
        lvl = "GPIO_PIN_SET,  " if md[i] == 1 else "GPIO_PIN_RESET,"
        s, n = re.subn(pat, r"\g<1>%s &%s, %+d" % (lvl, tim, ed[i]), s)
        assert n == 1, "wheel table of ass2_common.c not found"
    src.write_text(s)
    env = dict(os.environ, PATH=toolchain())
    r = subprocess.run("make -j8 clean && make -j8 all", shell=True, cwd=BUILD / "Debug", env=env,
                       capture_output=True, text=True)
    if r.returncode != 0 or "warning" in r.stdout + r.stderr:
        sys.exit("build failed:\n" + (r.stdout + r.stderr)[-2000:])
    elf = str(BUILD / "Debug/ass2_sol.elf")
    b.flash(elf)
    time.sleep(1.5)
    task = a.symbols(elf)["task"][0]
    DATA.mkdir(parents=True, exist_ok=True)
    for k in (1, 2, 3, 4):
        m = b.mark()
        b.write16(task, 520 + k)
        if not b.wait_for(m, lambda line: line.startswith("=== task %d done" % (520 + k)), 15):
            print("wheel %d: no end line" % k)
        b.write16(task, 0)
        time.sleep(0.8)
        rows = [re.match(r"E2\.%d: t=([\d.]+) w=(-?[\d.]+)" % k, line) for _, line in b.since(m)]
        rows = [(float(x.group(1)), float(x.group(2))) for x in rows if x]
        (DATA / ("car%d_step_w%d.txt" % (car, k))).write_text("".join("%.2f %.2f\n" % r for r in rows))
        tail = [w for t, w in rows if t > 1.0]
        print("wheel %d: %d samples, mean speed after 1 s %.2f rad/s (must be positive)"
              % (k, len(rows), sum(tail) / max(1, len(tail))))


# ------------------------------------------------------------------ fit and design (no cart)
def fit(t, w):
    """w = K u (1 - exp(-t / tau)); each sample is the mean speed over the 10 ms before t"""
    best = None
    for tau in np.arange(0.02, 0.4, 0.0005):
        f = 1 - (tau / TS) * (np.exp(-(t - TS) / tau) - np.exp(-t / tau))
        K = float(f @ w / (f @ f)) / U_STEP
        e = float(np.sum((w - K * U_STEP * f) ** 2))
        if best is None or e < best[0]:
            best = (e, K, tau)
    return best[1], best[2], (best[0] / len(t)) ** 0.5


def sim(K, tau, Kp, Ki, T=0.5, dt=2e-4):
    """unit step from rest in the real loop: PI every 10 ms, speed over the last 10 ms, output held, no limit.
    Returns overshoot (peak / setpoint) and the peak |u| per rad/s of step."""
    n_c = int(round(TS / dt))
    w = th = thp = integ = u = umax = wmax = 0.0
    for i in range(int(T / dt)):
        if i % n_c == 0:
            e = 1.0 - (th - thp) / TS
            thp = th
            integ += e * TS
            u = Kp * e + Ki * integ
            umax = max(umax, abs(u))
        w += dt * (K * u - w) / tau
        th += dt * w
        wmax = max(wmax, w)
    return wmax, umax


def cmd_write(car, md, ed, cpr, tauc):
    K, tau, rms, Kp, Ki, mv, os_ = [], [], [], [], [], [], []
    for k in (1, 2, 3, 4):
        d = np.loadtxt(DATA / ("car%d_step_w%d.txt" % (car, k)))
        kk, tt, rr = fit(d[:, 0], d[:, 1])
        p = tt / (kk * tauc)
        i = p / tt
        o, um = sim(kk, tt, p, i)
        K.append(kk); tau.append(tt); rms.append(rr); Kp.append(p); Ki.append(i); mv.append(ULIM / um); os_.append(o)
        print("wheel %d: K=%.3f rad/s/V tau=%.4f s (rms %.2f) Kp=%.3f Ki=%.2f overshoot %.3f maxVelCmdNoSat %.2f"
              % (k, kk, tt, rr, p, i, o, ULIM / um))

    def f(v, fmt):
        return "{ " + ", ".join(fmt % x for x in v) + " }"
    entry = (
        "    /* car %d, measured %s (skl_prj_getCarParams). countsPerRev: %s.\n"
        "       Fit rms %.2f to %.2f rad/s (one count in 10 ms is %.2f rad/s).\n"
        "       Simulated with these gains: overshoot %s. */\n"
        "    {\n"
        "        .carId           = %d,\n"
        "        .motorDir        = %s,\n"
        "        .encDir          = %s,\n"
        "        .countsPerRev    = %s,\n"
        "        .K               = %s,\n"
        "        .tau             = %s,\n"
        "        .Kp              = %s,\n"
        "        .Ki              = %s,\n"
        "        .maxVelCmdNoSat  = %s,\n"
        "        .designOvershoot = 1.0f,\n"
        "        .designTauC      = %.3ff,\n"
        "%s"
        "    },\n"
        % (car, time.strftime("%Y-%m-%d"), ", ".join("%g" % c for c in cpr), min(rms), max(rms),
           6.2831853 / min(cpr) / TS, " / ".join("%.3f" % o for o in os_), car,
           f(md, "%+d"), f(ed, "%+d"), f(cpr, "%.1ff"), f(K, "%.3ff"), f(tau, "%.4ff"),
           f(Kp, "%.3ff"), f(Ki, "%.2ff"), f([int(x * 100) / 100 for x in mv], "%.2ff"), tauc, "%s"))
    s = PARAMS_C.read_text()
    old = re.compile(r"    /\* car %d, measured.*?\n    \},\n" % car, re.S)
    m = old.search(s)
    keep = re.findall(SENSOR_RE, m.group(0)) if m else []           # the sensor lines of an old entry stay
    entry = entry % ("".join(keep) if len(keep) == 3 else SENSOR_DEFAULT)
    if m:
        s = old.sub(lambda _: entry, s, count=1)
        print("car %d: entry replaced in %s" % (car, PARAMS_C.name))
    else:
        s = s.replace("\n};\n\nconst CarParams", "\n" + entry + "};\n\nconst CarParams", 1)
        print("car %d: entry added to %s" % (car, PARAMS_C.name))
    PARAMS_C.write_text(s)


# ------------------------------------------------------------------ servo and IR (task 900 of the A3 solution)
def cmd_calstart(b):
    b.flash(str(ELF3))
    time.sleep(2.0)
    b.write16(a.symbols(str(ELF3))["task"][0], 900)
    time.sleep(1.0)
    print("ass3_sol flashed, task 900: the servo at 1500 us")


def cmd_servo(b, us):
    assert 900 <= us <= 2100, "900..2100 us"
    b.write16(a.symbols(str(ELF3))["calServoUs"][0], us)
    time.sleep(1.0)
    print("servo at %d us" % us)


def cmd_ir(b):
    m = b.mark()
    time.sleep(3.2)
    lv = [re.search(r"ir_pin=(\d)", line) for _, line in b.since(m)]
    print("IR pin:", [int(x.group(1)) for x in lv if x] or "no CAL lines: is task 900 running?")


def cmd_sensors(car, ir, deg):
    s = PARAMS_C.read_text()
    m = re.search(r"    /\* car %d, measured.*?\n    \},\n" % car, s, re.S)
    if not m:
        sys.exit("car %d has no entry in car_params.c: measure steps 1 to 4 first" % car)
    e = re.sub(SENSOR_RE, "", m.group(0))
    lines = ("        .irObstacleLevel = %d,                          /* measured %s */\n"
             "        .servoUs         = { 900, 1500, 2100 },\n"
             "        .servoDeg        = { %s },\n"
             % (ir, time.strftime("%Y-%m-%d"), ", ".join("%.1ff" % d for d in deg)))
    e = e[:e.rindex("    },\n")] + lines + "    },\n"
    PARAMS_C.write_text(s[:m.start()] + e + s[m.end():])
    print("car %d: irObstacleLevel %d, servoDeg %s written" % (car, ir, deg))


def main():
    cmd = sys.argv[1]
    if cmd == "write":
        cpr = [float(x) for x in sys.argv[5].split(",")]
        cmd_write(int(sys.argv[2]), signs(sys.argv[3]), signs(sys.argv[4]), cpr,
                  float(sys.argv[6]) if len(sys.argv) > 6 else 0.032)
        return
    if cmd == "sensors":
        deg = [float(x) for x in sys.argv[4].split(",")]
        assert int(sys.argv[3]) in (0, 1) and len(deg) == 3, "IR 0 or 1, three angles"
        cmd_sensors(int(sys.argv[2]), int(sys.argv[3]), deg)
        return
    b = a.Board(a.com_port())
    try:
        if cmd == "prepare":
            cmd_prepare(b)
        elif cmd == "spin":
            cmd_spin(b, int(sys.argv[2]))
        elif cmd == "cnt":
            print(dict(zip([e[0] for e in ENC], counters(b))))
        elif cmd == "calstart":
            cmd_calstart(b)
        elif cmd == "servo":
            cmd_servo(b, int(sys.argv[2]))
        elif cmd == "ir":
            cmd_ir(b)
        elif cmd == "steps":
            cmd_steps(b, int(sys.argv[2]), signs(sys.argv[3]), signs(sys.argv[4]))
        else:
            sys.exit(__doc__)
    finally:
        b.close()


if __name__ == "__main__":
    main()
