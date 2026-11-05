"""
Automatic test of the MP assignments (TA only, never handed to students).
Run through the /skl_prj_checkAss skill.

For each group: builds its CubeIDE project, flashes it to the board, runs every
task that can be checked without a hand on the board by writing the global
`task` over SWD while the program runs, reads the PuTTY lines from the ST-LINK
COM port, and writes a short comment per task into the assignment's sheet of
"assignments gradings.xlsx". The points columns are left for the TA.
Assignment 1: A.1 to A.3, B.1 to B.3, D.1 to D.3 (C and E need a hand: videos).
Assignment 2 (cart on its block, wheels in the air, nothing in front of the sensors): A.1 to A.3, B.1,
B.2/B.3 tests 2 and 3, C.1 to C.5, D.1.2, D.2, E.1 (A.4 and E.2 are report items; B test 1 and D.1.1
need a hand). `task` is a uint16_t there.
Assignments without a reference solution are refused.

Usage (PuTTY closed, no debug session open in CubeIDE):
    python assignments_autotest.py test --ass 1 --submissions <folder with the groups' ZIP files>
    python assignments_autotest.py test --ass 1 --project <folder> --name "first last" [--report <pdf>]
        options: --elf <file> (test a built .elf instead of building), --port COM3
    python assignments_autotest.py note --ass 1 --name "first last" --task A.2 --text "..."
        (adds "report: ..." to that task's comment, for the report review)

Each run adds or updates the group's row; the points the TA typed are kept.
Logs of every line the board printed: logs/assN/<name>.log. Unpacked
submissions: work/assN/<name>/, listed with their report PDFs in
work/assN/summary.json.
"""

import argparse
import glob
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
import threading
import time
import zipfile

import serial
import serial.tools.list_ports
import openpyxl
from openpyxl.styles import Alignment, Font, PatternFill
from openpyxl.utils import get_column_letter

# ---------------------------------------------------------------- tools
CUBEIDE = r"C:\ST\STM32CubeIDE_2.2.0\STM32CubeIDE"
PROGRAMMER = r"C:\Program Files\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe"


def tool_dir(pattern):
    hits = glob.glob(os.path.join(CUBEIDE, "plugins", pattern, "tools", "bin"))
    if not hits:
        sys.exit("not found: " + pattern)
    return hits[0]


GCC_BIN = tool_dir("com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.*")
MAKE_BIN = tool_dir("com.st.stm32cube.ide.mcu.externaltools.make.*")

# ---------------------------------------------------------------- the assignments
HERE = os.path.dirname(os.path.abspath(__file__))
WORKBOOK = os.path.join(HERE, "assignments gradings.xlsx")

# the items of each assignment, one comment and one points column each
TASKS = {1: ["A.1", "A.2", "A.3", "B.1", "B.2", "B.3",
             "C.1", "C.2", "C.3", "C.4", "C.5",
             "D.1", "D.2", "D.3", "E"],
         2: ["A.1", "A.2", "A.3", "A.4", "B.1", "B.2", "B.3",
             "C.1", "C.2", "C.3", "C.4", "C.5",
             "D.1", "D.2", "E.1", "E.2", "E.3"]}

# the reference solution and its report: an assignment is tested only when it has one
REFERENCE = {1: ("ass1/solution/fullSolution/ass1_sol", "ass1/solution/fullSolution/ass1_sol.tex"),
             2: ("ass2/solution/fullSolution/ass2_sol", "ass2/solution/fullSolution/ass2_sol.tex")}

OK = "works good"


def columns(ass):
    cols = ["Group"]
    for t in TASKS[ass] + ["Others"]:
        cols += [t + " comment", t + " points"]
    return cols + ["Total"]


def no_reference(ass):
    """the refusal message, or None when the assignment can be tested"""
    if ass in REFERENCE and ass in TASKS:
        return None
    return ("Assignment %d has no reference solution yet, so it cannot be checked automatically. "
            "Nothing was tested." % ass)


# ================================================================ build
def run(cmd, cwd=None, timeout=600, env=None):
    p = subprocess.run(cmd, cwd=cwd, capture_output=True, text=True, timeout=timeout, env=env,
                       errors="replace")
    return p.returncode, p.stdout + p.stderr


def build(project):
    """Build the Debug configuration; return the .elf path, or raise with a short reason."""
    env = dict(os.environ, PATH=GCC_BIN + os.pathsep + MAKE_BIN + os.pathsep + os.environ["PATH"])
    project = os.path.normpath(os.path.abspath(project))     # Eclipse reads "C:/..." as a URI scheme
    debug = os.path.join(project, "Debug")
    if os.path.isfile(os.path.join(debug, "makefile")):
        code, out = run([os.path.join(MAKE_BIN, "make.exe"), "-j8", "all"], cwd=debug, env=env)
    else:
        # no generated makefile: let CubeIDE import and build the project headless
        ws = tempfile.mkdtemp(prefix="ass1ws_")
        code, out = run([os.path.join(CUBEIDE, "stm32cubeidec.exe"), "--launcher.suppressErrors", "-nosplash",
                         "-application", "org.eclipse.cdt.managedbuilder.core.headlessbuild",
                         "-data", ws, "-import", project, "-build", "all"], timeout=900)
    elfs = glob.glob(os.path.join(debug, "*.elf"))
    if code != 0 or not elfs:
        errors = [l for l in out.splitlines() if "error" in l.lower()]
        raise RuntimeError("build failed: " + (errors[0][:120] if errors else "no .elf"))
    return elfs[0]


def symbols(elf):
    """name -> (address, size, type) of the global data symbols"""
    code, out = run([os.path.join(GCC_BIN, "arm-none-eabi-nm.exe"), "-S", elf])
    table = {}
    for line in out.splitlines():
        f = line.split()
        if len(f) == 4:
            table[f[3]] = (int(f[0], 16), int(f[1], 16), f[2])
    return table


# ================================================================ the board
class Board:
    """SWD through STM32_Programmer_CLI, and the ST-LINK virtual COM port."""

    def __init__(self, port):
        self.ser = serial.Serial(port, 115200, timeout=0.05)
        self.lines = []                      # (pc time in s, text)
        self.lock = threading.Lock()
        self.alive = True
        self.thread = threading.Thread(target=self._reader, daemon=True)
        self.thread.start()

    def _reader(self):
        buf = b""
        while self.alive:
            data = self.ser.read(4096)
            if not data:
                continue
            buf += data
            while b"\n" in buf:
                raw, buf = buf.split(b"\n", 1)
                with self.lock:
                    self.lines.append((time.monotonic(), raw.decode("ascii", "replace").rstrip("\r")))

    def close(self):
        self.alive = False
        self.thread.join()
        self.ser.close()

    def mark(self):
        with self.lock:
            return len(self.lines)

    def since(self, mark):
        with self.lock:
            return list(self.lines[mark:])

    def wait_for(self, mark, pred, timeout):
        """wait until a line after `mark` satisfies pred; return (time, text) or None"""
        end = time.monotonic() + timeout
        while time.monotonic() < end:
            for t, s in self.since(mark):
                if pred(s):
                    return t, s
            time.sleep(0.05)
        return None

    @staticmethod
    def cli(*args):
        code, out = run([PROGRAMMER, "-q", *args], timeout=60)
        if code != 0 or "Error" in out:
            raise RuntimeError("ST-LINK: " + next((l for l in out.splitlines() if "rror" in l), "failed").strip())
        return out

    def flash(self, elf):
        self.cli("-c", "port=SWD", "mode=UR", "-d", elf, "-v", "-rst")

    # HOTPLUG: connect without halting or resetting, so the program keeps running.
    # The data must be given in hex: -w8 reads a decimal "11" as 0.
    def write8(self, addr, value):
        self.cli("-c", "port=SWD", "mode=HOTPLUG", "-w8", hex(addr), "0x%02X" % value)

    def write16(self, addr, value):
        self.cli("-c", "port=SWD", "mode=HOTPLUG", "-w16", hex(addr), "0x%04X" % value)

    def write32(self, addr, value):
        self.cli("-c", "port=SWD", "mode=HOTPLUG", "-w32", hex(addr), "0x%08X" % value)

    def read32(self, addr):
        out = self.cli("-c", "port=SWD", "mode=HOTPLUG", "-r32", hex(addr), "4")
        m = re.search(r"0x%08X\s*:\s*([0-9A-Fa-f]{8})" % addr, out)
        if not m:
            raise RuntimeError("ST-LINK: read failed")
        return int(m.group(1), 16)


# ================================================================ helpers
def b8(v):
    return "0b" + format(v & 0xFF, "08b")


def tagged(lines, tag):
    return [(t, s) for t, s in lines if s.startswith(tag + ":")]


def median(xs):
    xs = sorted(xs)
    return xs[len(xs) // 2] if xs else None


def short(s, n=45):
    return s if len(s) <= n else s[:n] + "..."


def compare(got, expected):
    """first differing line as a short comment, or None"""
    for i, exp in enumerate(expected):
        if i >= len(got):
            return "%d of %d lines" % (len(got), len(expected))
        if got[i] != exp:
            return "line %d wrong: '%s'" % (i + 1, short(got[i]))
    if len(got) > len(expected):
        return "%d lines, expected %d" % (len(got), len(expected))
    return None


def wrap_add(name, v):
    """v + 1 in the type of the A.1 variable"""
    if name == "u8":
        return (v + 1) & 0xFF
    if name == "i8":
        return ((v + 1 + 128) & 0xFF) - 128
    return (v + 1) & 0xFFFF


def a1_line(u8, i8, u16, binary=True):
    if binary:
        return ("A1: u8=%d 0x%02X %s i8=%d 0x%02X %s u16=%d 0x%04X 0b%s"
                % (u8, u8, b8(u8), i8, i8 & 0xFF, b8(i8), u16, u16, format(u16, "016b")))
    return "A1: u8=%d 0x%02X i8=%d 0x%02X u16=%d 0x%04X" % (u8, u8, i8, i8 & 0xFF, u16, u16)


def a1_diagnose(got):
    """short comments on the A.1 lines: start values, binary, steps, format"""
    start = {"u8": 250, "i8": 120, "u16": 65530}
    if not got:
        return ["no A1 lines"]
    issues = []
    values = [dict((k, int(v)) for k, v in re.findall(r"\b(u8|i8|u16)=(-?\d+)", s)) for s in got]
    first = values[0]
    wrong = [k for k in start if k in first and first[k] != start[k]]
    if wrong:
        issues.append("starts %s (not %s)" % (" ".join("%s=%d" % (k, first[k]) for k in wrong),
                                             ", ".join(str(start[k]) for k in wrong)))
    binary = any("0b" in s for s in got)
    if not binary:
        issues.append("no binary")
    if len(got) != 13:
        issues.append("%d of 13 lines" % len(got))
    # the steps and the exact format, from the group's own start values
    if all(k in first for k in start):
        u8, i8, u16 = first["u8"], first["i8"], first["u16"]
        for i, (s, v) in enumerate(zip(got, values)):
            if v != {"u8": u8, "i8": i8, "u16": u16}:
                issues.append("line %d: wrong values" % (i + 1))
                break
            if binary and s != a1_line(u8, i8, u16):
                issues.append("line %d format: '%s'" % (i + 1, short(s)))
                break
            if not binary and s != a1_line(u8, i8, u16, binary=False):
                issues.append("line %d format: '%s'" % (i + 1, short(s)))
                break
            u8, i8, u16 = wrap_add("u8", u8), wrap_add("i8", i8), wrap_add("u16", u16)
    elif not issues:
        issues.append("line 1 format: '%s'" % short(got[0]))
    return issues


class Tester:
    def __init__(self, board, syms):
        self.b = board
        self.task_addr = syms["task"][0]
        self.blink = syms.get("blinkMs")
        self.notes = []                      # for the Others column

    def start(self, n, settle=0.3):
        """choose task n (0 first, so that the same task restarts); return the mark"""
        self.b.write8(self.task_addr, 0)
        time.sleep(settle)
        mark = self.b.mark()
        self.b.write8(self.task_addr, n)
        if not self.b.wait_for(mark, lambda s: s.strip() == "=== task %d ===" % n, 3):
            self.notes.append("no '=== task %d ===' line" % n)
        return mark

    def done_line(self, mark, n, timeout):
        return self.b.wait_for(mark, lambda s: s.strip() == "=== task %d done ===" % n, timeout)

    # ------------------------------------------------------------ A
    def a1(self):
        m = self.start(11)
        done = self.done_line(m, 11, 12)
        got = tagged(self.b.since(m), "A1")
        issues = a1_diagnose([s for _, s in got])
        steps = [b[0] - a[0] for a, b in zip(got, got[1:])]
        if steps and not 0.45 <= median(steps) <= 0.6:
            issues.append("step %.0f ms, expected 500" % (1000 * median(steps)))
        if not done:
            issues.append("no 'done' line")
        return issues

    def a2(self):
        exp = ["A2: hex=0x7F uint8_t dec=127 bin=0b01111111 raw=0x7F",
               "A2: hex=0x7F int8_t dec=127 bin=0b01111111 raw=0x7F",
               "A2: hex=0xF0 uint8_t dec=240 bin=0b11110000 raw=0xF0",
               "A2: hex=0xF0 int8_t dec=-16 bin=0b11110000 raw=0xFFFFFFF0",
               "A2: hex=0xFFF6 uint16_t dec=65526 bin=0b1111111111110110 raw=0xFFF6",
               "A2: hex=0xFFF6 int16_t dec=-10 bin=0b1111111111110110 raw=0xFFFFFFF6"]
        m = self.start(12)
        done = self.done_line(m, 12, 3)
        issues = []
        got = [s for _, s in tagged(self.b.since(m), "A2")]
        if got and not any("bin=" in s for s in got):
            issues.append("no binary (bin=)")
            exp = [re.sub(r" bin=0b[01]+", "", e) for e in exp]
        diff = compare(got, exp)
        if diff:
            issues.append(diff)
        if not done:
            issues.append("no 'done' line")
        return issues

    def a3(self):
        m = self.start(13)
        done = self.done_line(m, 13, 3)
        got = [s for _, s in tagged(self.b.since(m), "A3")]
        issues = []
        if got != ["A3: wrong=77 right=77.5"]:
            issues.append("got '%s'" % (got[0] if got else "nothing"))
        if not done:
            issues.append("no 'done' line")
        return issues

    # ------------------------------------------------------------ B
    def b1(self):
        exp = ["B1: start LD2=0", "B1: set LD2=1", "B1: clear LD2=0", "B1: toggle LD2=1", "B1: LED0=1"]
        m = self.start(21)
        done = self.done_line(m, 21, 6)
        got = tagged(self.b.since(m), "B1")
        issues = []
        diff = compare([s for _, s in got], exp)
        if diff:
            issues.append(diff)
        steps = [b[0] - a[0] for a, b in zip(got[:4], got[1:4])]
        if len(steps) == 3 and not all(0.9 <= s <= 1.15 for s in steps):
            issues.append("steps not 1 s")
        if not done:
            issues.append("no 'done' line")
        else:
            odr = self.b.read32(0x48000014)          # GPIOA->ODR: LD2 = bit 5
            if not (odr >> 5) & 1:
                issues.append("LD2 pin off at the end")
        return issues

    def walk(self, n, tag, start, count=10):
        exp = []
        p = start
        for _ in range(count):
            exp.append("%s: %s" % (tag, b8(p)))
            p = ((p << 1) | (p >> 7)) & 0xFF
        m = self.start(n)
        self.b.wait_for(m, lambda s: False, count + 0.5)
        got = tagged(self.b.since(m), tag)[:count]
        issues = []
        diff = compare([s for _, s in got], exp)
        if diff:
            issues.append(diff)
        steps = [b[0] - a[0] for a, b in zip(got, got[1:])]
        if steps and not 0.9 <= median(steps) <= 1.15:
            issues.append("step %.0f ms, expected 1000" % (1000 * median(steps)))
        return issues

    # ------------------------------------------------------------ D
    def d1(self):
        m = self.start(41)
        t0 = time.monotonic()
        res = self.b.wait_for(m, lambda s: s.startswith("D1: toggles="), 130)
        lines = self.b.since(m)
        issues = []
        status = [s for _, s in lines if re.match(r"D1: toggle=\d+", s)]
        nums = [int(re.match(r"D1: toggle=(\d+)", s).group(1)) for s in status]
        if nums != list(range(1, 201)):
            issues.append("%d status lines, expected 200" % len(nums))
        lengths = [len(s) + 2 for s in status]
        if lengths and not 50 <= median(lengths) <= 70:
            issues.append("status line %d chars, expected about 60" % median(lengths))
        if not res:
            return issues + ["no 'D1: toggles=200 total=' line"]
        mm = re.fullmatch(r"D1: toggles=(\d+) total=(\d+)", res[1])
        if not mm:
            return issues + ["final line format: '%s'" % res[1]]
        total = int(mm.group(2))
        pc = 1000 * (res[0] - t0)
        if abs(total - pc) > 400:
            issues.append("total %d ms, PC clock %d ms" % (total, pc))
        if not 100000 <= total <= 104000:
            issues.append("total %d ms" % total)
        if not self.done_line(m, 41, 2):
            issues.append("no 'done' line")
        return issues

    def d2(self):
        if not self.blink or self.blink[1] != 4:
            return ["no global uint32_t blinkMs"]
        issues = []
        self.b.write32(self.blink[0], 500)
        m = self.start(42)
        for d in (500, 100):
            if d != 500:
                self.b.write32(self.blink[0], d)
                time.sleep(4 * d / 1000 + 0.2)          # let the new value take over
            mark = self.b.mark()
            self.b.wait_for(mark, lambda s: False, max(3.0, 12 * d / 1000))
            got = [re.fullmatch(r"D2: t=(\d+) LD2=([01])", s) for _, s in tagged(self.b.since(mark), "D2")]
            if not got or None in got:
                issues.append("line format" if got else "no D2 lines")
                break
            ticks = [int(g.group(1)) for g in got]
            leds = [g.group(2) for g in got]
            half = median([b - a for a, b in zip(ticks, ticks[1:])])
            if half is None or not d <= half <= d + 6:
                issues.append("blinkMs=%d: half period %s ms" % (d, half))
            if any(a == b for a, b in zip(leds, leds[1:])):
                issues.append("LD2 does not alternate")
        self.b.write32(self.blink[0], 500)
        return issues

    def d3(self):
        issues = []
        # D.3 with HAL_Delay: the numbers depend on the design; check that they are honest
        m = self.start(43)
        t0 = time.monotonic()
        res = {}
        for led in (0, 1):
            r = self.b.wait_for(m, lambda s, led=led: s.startswith("D3: led=%d ms=" % led), 45)
            if r:
                res[led] = (int(r[1].split("ms=")[1]), 1000 * (r[0] - t0))
        if len(res) < 2:
            issues.append("HAL_Delay: no D3 lines for both LEDs")
        for led, (ms, pc) in res.items():
            if abs(ms - pc) > max(300, 0.03 * ms):
                issues.append("HAL_Delay: LED%d %d ms, PC clock %d" % (led, ms, pc))
        # D.3 with HAL_GetTick: within a few ms of the theory
        m = self.start(44)
        theory = {0: 10000, 1: 7692}
        for led in (0, 1):
            r = self.b.wait_for(m, lambda s, led=led: s.startswith("D3: led=%d ms=" % led), 25)
            if not r:
                issues.append("HAL_GetTick: no line for LED%d" % led)
                continue
            mm = re.fullmatch(r"D3: led=%d ms=(\d+)" % led, r[1])
            if not mm:
                issues.append("format: '%s'" % r[1])
            elif abs(int(mm.group(1)) - theory[led]) > 3:
                issues.append("HAL_GetTick: LED%d %s ms (theory %d)" % (led, mm.group(1), theory[led]))
        return issues

    # ------------------------------------------------------------ protocol
    def protocol(self):
        mark = self.b.mark()
        self.b.write8(self.task_addr, 99)
        if not self.b.wait_for(mark, lambda s: s.strip() == "no task 99", 2):
            self.notes.append("no 'no task 99' line")
        self.b.write8(self.task_addr, 0)




def test_ass1(name, elf, port, logdir):
    """run all the automatic checks of A1; return {task: comment} and the Others comment"""
    syms = symbols(elf)
    if "task" not in syms or syms["task"][1] != 1 or syms["task"][2] not in "BD":
        return {}, "no global uint8_t task: not tested"
    board = Board(port)
    comments = {}
    tester = Tester(board, syms)
    try:
        mark = board.mark()
        board.flash(elf)
        board.wait_for(mark, lambda s: False, 3)               # start-up prints
        if any(s.startswith("=== task") for _, s in board.since(mark)):
            tester.notes.append("does not start idle")
        for key, check in [("A.1", tester.a1), ("A.2", tester.a2), ("A.3", tester.a3),
                           ("B.1", tester.b1), ("B.2", lambda: tester.walk(22, "B2", 0x01)),
                           ("B.3", lambda: tester.walk(23, "B3", 0x03)),
                           ("D.1", tester.d1), ("D.2", tester.d2), ("D.3", tester.d3)]:
            print("  %s ..." % key, flush=True)
            try:
                issues = check()
            except RuntimeError as e:
                issues = [str(e)]
            comments[key] = "; ".join(issues) if issues else OK
            print("  %s: %s" % (key, comments[key]), flush=True)
        tester.protocol()
    finally:
        os.makedirs(logdir, exist_ok=True)
        with open(os.path.join(logdir, safe(name) + ".log"), "w", encoding="utf-8") as f:
            t0 = board.lines[0][0] if board.lines else 0
            for t, s in board.lines:
                f.write("%9.3f  %s\n" % (t - t0, s))
        board.close()
    return comments, "; ".join(tester.notes)


# ================================================================ Assignment 2
# The cart must be on its block with the wheels in the air, nothing in front of the IR and
# ultrasonic sensors. What needs a hand or a video (the hand passes of B.2 and B.3, D.1.1, D.1.2)
# or only the report (A.4, B.1, E.1, E.3) is not tested here. B1 (PC13, HIGH while pressed) is
# pressed over SWD: PC13 is made an output driving HIGH for a moment, then an input again.
TIM1_CNT = 0x40012C24
TIM2_CCR1 = 0x40000034
TIM7_CR1, TIM7_PSC, TIM7_ARR = 0x40001400, 0x40001428, 0x4000142C
GPIOC_MODER, GPIOC_IDR, GPIOC_ODR = 0x48000800, 0x48000810, 0x48000814


class Tester2(Tester):
    """the A2 checks; `task` is a uint16_t with three-digit numbers"""

    def __init__(self, board, syms):
        super().__init__(board, syms)
        self.pwmN = syms.get("pwmN")
        self.pwmM = syms.get("pwmM")

    def start(self, n, settle=0.4):
        self.b.write16(self.task_addr, 0)
        time.sleep(settle)
        mark = self.b.mark()
        self.b.write16(self.task_addr, n)
        if not self.b.wait_for(mark, lambda s: s.strip() == "=== task %d ===" % n, 3):
            self.notes.append("no '=== task %d ===' line" % n)
        return mark

    def stop(self):
        self.b.write16(self.task_addr, 0)
        time.sleep(0.4)

    def lines(self, m, tag, pattern):
        """the tagged lines since mark m, as (time, match-or-None, text)"""
        return [(t, re.fullmatch(re.escape(tag) + r": " + pattern, s), s) for t, s in tagged(self.b.since(m), tag)]

    @staticmethod
    def fmt(got):
        for _, mm, s in got:
            if mm is None:
                return "line format: '%s'" % short(s)
        return None

    def once_a_second(self, got, name):
        steps = [b[0] - a[0] for a, b in zip(got, got[1:])]
        if steps and not 0.85 <= median(steps) <= 1.2:
            return "%s: lines %.0f ms apart, expected 1000" % (name, 1000 * median(steps))
        return None

    # ------------------------------------------------------------ A
    def pwm_lines(self, n, tag, N, M, seconds=3.5, expect_M=None):
        if self.pwmN:
            self.b.write32(self.pwmN[0], N)
        if self.pwmM and M is not None:
            self.b.write32(self.pwmM[0], M)
        m = self.start(n)
        self.b.wait_for(m, lambda s: False, seconds)
        got = self.lines(m, tag, r"N=(\d+) M=(\d+) passes=(\d+)")
        issues = []
        if not got:
            return ["%s: no lines" % tag], []
        f = self.fmt(got)
        if f:
            return ["%s: %s" % (tag, f)], got
        good = [g for g in got if g[1]]
        last = good[-1][1]
        want_M = expect_M if expect_M is not None else M
        if (int(last.group(1)), int(last.group(2))) != (N, want_M):
            issues.append("%s: N=%s M=%s, expected N=%d M=%d" % (tag, last.group(1), last.group(2), N, want_M))
        o = self.once_a_second(good, tag)
        if o:
            issues.append(o)
        return issues, good

    def a1(self):
        if not self.pwmN or not self.pwmM or self.pwmN[1] != 4 or self.pwmM[1] != 4:
            return ["no global uint32_t pwmN/pwmM"]
        issues, _ = self.pwm_lines(111, "A1.1", 100, 50)
        i2, _ = self.pwm_lines(112, "A1.2", 10, None, expect_M=5)
        self.stop()
        return issues + i2

    def a2(self):
        if not self.pwmN or not self.pwmM:
            return ["no global uint32_t pwmN/pwmM"]
        issues = []
        i, _ = self.pwm_lines(121, "A2.1", 100, 50)
        issues += i
        # A.2.2: the wheel turns
        i, _ = self.pwm_lines(122, "A2.2", 100, 50, seconds=1.5)
        issues += i
        c1 = self.b.read32(TIM1_CNT) & 0xFFFF
        time.sleep(1.0)
        c2 = self.b.read32(TIM1_CNT) & 0xFFFF
        if c1 == c2:
            issues.append("A2.2: wheel 1 does not turn")
        # A.2.3: 2 s at 10 kHz, then 1.7 MHz; the program must still let task change, and TIM7 ends at 10 kHz, stopped
        i, _ = self.pwm_lines(123, "A2.3", 100, 50, seconds=4.0)
        issues += [x for x in i if "lines" not in x]          # at 1.7 MHz the loop may not print at all
        m = self.b.mark()
        self.b.write16(self.task_addr, 124)
        if not self.b.wait_for(m, lambda s: s.strip() == "=== task 124 ===", 4):
            issues.append("A2.3: does not leave the task at 1.7 MHz")
        else:
            psc, arr, cr1 = self.b.read32(TIM7_PSC), self.b.read32(TIM7_ARR), self.b.read32(TIM7_CR1)
            if (psc, arr) != (169, 99) or cr1 & 1:
                issues.append("A2.3: TIM7 left at PSC=%d ARR=%d %s" % (psc, arr, "running" if cr1 & 1 else "stopped"))
        # A.2.4 (task 124 is running now)
        r = self.b.wait_for(m, lambda s: s.startswith("A2.4:"), 5)
        if not r:
            issues.append("A2.4: no line")
        else:
            mm = re.fullmatch(r"A2.4: calls=1000 ms=(\d+)", r[1])
            if not mm:
                issues.append("A2.4: line format: '%s'" % short(r[1]))
            elif not 30 <= int(mm.group(1)) <= 300:
                issues.append("A2.4: %s ms for 1000 calls" % mm.group(1))
        if not self.done_line(m, 124, 3):
            issues.append("A2.4: no 'done' line")
        self.stop()
        return issues

    def a3(self):
        issues = []
        m = self.start(131)
        self.b.wait_for(m, lambda s: False, 5.0)              # duty 0, 5, 10 %: enough to see the steps
        ccr = self.b.read32(TIM2_CCR1)
        got = self.lines(m, "A3.1", r"duty=(\d+) CCR1=(\d+)")
        f = self.fmt(got)
        if not got:
            issues.append("A3.1: no lines")
        elif f:
            issues.append("A3.1: " + f)
        else:
            duties = [int(g[1].group(1)) for g in got]
            if duties != list(range(0, 5 * len(duties), 5)):
                issues.append("A3.1: duties %s, expected 0, 5, 10 ..." % duties[:4])
            if any(int(g[1].group(2)) != 100 * int(g[1].group(1)) for g in got):
                issues.append("A3.1: CCR1 is not duty x 100")
            steps = [b[0] - a[0] for a, b in zip(got, got[1:])]
            if steps and not 1.85 <= median(steps) <= 2.2:
                issues.append("A3.1: steps %.0f ms apart, expected 2000" % (1000 * median(steps)))
            if ccr != 100 * duties[-1]:
                issues.append("A3.1: TIM2->CCR1 = %d, printed %d" % (ccr, 100 * duties[-1]))
        m = self.start(132)
        self.b.wait_for(m, lambda s: False, 3.5)
        got = self.lines(m, "A3.2", r"passes=(\d+)")
        f = self.fmt(got)
        if not got:
            issues.append("A3.2: no lines")
        elif f:
            issues.append("A3.2: " + f)
        else:
            o = self.once_a_second(got, "A3.2")
            if o:
                issues.append(o)
        self.stop()
        return issues

    # ------------------------------------------------------------ B
    def press_b1(self, hold=0.5):
        """press B1 over SWD: PC13 drives HIGH (as when pressed) for `hold` s, then is an input again"""
        moder = self.b.read32(GPIOC_MODER)
        self.b.write32(GPIOC_ODR, self.b.read32(GPIOC_ODR) | (1 << 13))
        self.b.write32(GPIOC_MODER, (moder & ~(3 << 26)) | (1 << 26))     # PC13 output
        time.sleep(hold)
        self.b.write32(GPIOC_MODER, moder & ~(3 << 26))                    # PC13 input again

    def b_run(self, n, wait):
        """start task n with nothing in front of the sensors, check that wheel 1 turns at 40 %,
        press B1 after `wait` s; return (mark, issues, done)"""
        issues = []
        m = self.start(n)
        self.b.wait_for(m, lambda s: False, wait)
        ccr = self.b.read32(TIM2_CCR1)
        if ccr != 4000:
            issues.append("%d: CCR1=%d with nothing in front of the sensors, expected 4000 (40 %%)" % (n, ccr))
        self.press_b1()
        done = self.done_line(m, n, 3)
        if not done:
            issues.append("%d: B1 does not end the task" % n)
        elif self.b.read32(TIM2_CCR1) != 0:
            issues.append("%d: wheel 1 still turns after the end" % n)
        return m, issues, done

    def b2(self):
        """B.2.1, B.2.2: the hand passes are not tested; start, 40 %, B1 ends, no false stop"""
        issues = []
        for n, tag in ((221, "B2.1"), (222, "B2.2")):
            m, i, _ = self.b_run(n, 2.5)
            issues += i
            got = self.lines(m, tag, r"stops=(\d+) period=(\d+)")
            if got:
                f = self.fmt(got)
                issues.append("%s: %s" % (tag, f) if f else "%s: %d stops with nothing in front" % (tag, len(got)))
        self.stop()
        return issues

    def b3(self):
        """B.3.1, B.3.2: no hand; B1 ends the task and the readings line is checked"""
        issues = []
        for n, tag, polling in ((231, "B3.1", True), (232, "B3.2", False)):
            m, i, _ = self.b_run(n, 4.0)
            issues += i
            got = self.lines(m, tag, r"readings=(\d+) missed=(\d+) ms=(\d+)")
            got = [g for g in got if not g[2].startswith(tag + ": stops=")]
            if not got:
                issues.append("%s: no readings line" % tag)
            elif self.fmt(got):
                issues.append("%s: %s" % (tag, self.fmt(got)))
            else:
                rd, ms, t = (int(x) for x in got[-1][1].groups())
                due = t / 100
                if rd + ms > due + 5:
                    issues.append("%s: readings + missed = %d, more than the %d due" % (tag, rd + ms, due))
                if not polling and rd < 0.9 * due - 2:
                    issues.append("%s: readings=%d in %d ms, expected about %d" % (tag, rd, t, due))
        self.stop()
        return issues

    # ------------------------------------------------------------ C
    def c1(self):
        m = self.start(310)
        done = self.done_line(m, 310, 6)
        issues = []
        lines = [s for _, s in tagged(self.b.since(m), "C1")]
        found = [re.fullmatch(r"C1: found=0x([0-9A-F]{2})", s) for s in lines if s.startswith("C1: found=")]
        count = [re.fullmatch(r"C1: count=(\d+)", s) for s in lines if s.startswith("C1: count=")]
        if None in found:
            issues.append("found= format (0x and two capital hex digits)")
        addrs = {int(f.group(1), 16) for f in found if f}
        missing = [a for a in (0x01, 0x02, 0x07, 0x44, 0x71) if a not in addrs]
        if missing:
            issues.append("not found: " + " ".join("0x%02X" % a for a in missing))
        if not count or None in count:
            issues.append("no 'count=' line")
        elif int(count[-1].group(1)) != len(found):
            issues.append("count=%s, but %d found lines" % (count[-1].group(1), len(found)))
        if not done:
            issues.append("no 'done' line")
        return issues

    def c2(self):
        m = self.start(320)
        done = self.done_line(m, 320, 4)
        got = self.lines(m, "C2", r"led=(\d) r=(\d+) g=(\d+) b=(\d+)")
        issues = []
        if self.fmt(got):
            issues.append(self.fmt(got))
        elif sorted(int(g[1].group(1)) for g in got)[:8] != list(range(8)):
            issues.append("%d LED lines, expected LEDs 0 to 7" % len(got))
        elif any(int(v) > 255 for g in got for v in g[1].groups()[1:]):
            issues.append("a value above 255")
        if not done:
            issues.append("no 'done' line")
        return issues

    def c3(self):
        m = self.start(330)
        done = self.done_line(m, 330, 8)
        got = self.lines(m, "C3", r"single=(\d+) burst=(\d+)")
        issues = []
        if not got:
            return ["no C3 line"]
        if self.fmt(got):
            return [self.fmt(got)]
        single, burst = int(got[-1][1].group(1)), int(got[-1][1].group(2))
        # at 400 kHz: 8 x 29 bits = 580 us and 92 bits = 230 us per set, 1000 sets
        if not 540 <= single <= 1000:
            issues.append("single=%d ms (theory at 400 kHz: 580)" % single)
        if not 215 <= burst <= 450:
            issues.append("burst=%d ms (theory at 400 kHz: 230)" % burst)
        if burst >= single:
            issues.append("burst not faster than single")
        if not done:
            issues.append("no 'done' line")
        return issues

    def c4(self):
        m = self.start(340)
        self.b.wait_for(m, lambda s: False, 1.5)
        self.stop()
        lines = [s for _, s in tagged(self.b.since(m), "C4")]
        issues = []
        a = [re.fullmatch(r"C4: addr=0x10 status=(\d+)", s) for s in lines if s.startswith("C4: addr=")]
        b = [re.fullmatch(r"C4: buttons=0b([01]{8}) status=(\d+)", s) for s in lines if s.startswith("C4: buttons=")]
        if not a or None in a:
            issues.append("no 'C4: addr=0x10 status=' line")
        elif int(a[0].group(1)) == 0:
            issues.append("address 0x10 gave status=0 (no chip there)")
        if not b or None in b:
            issues.append("no 'C4: buttons=0b........ status=' line")
        elif (b[0].group(1), b[0].group(2)) != ("00001111", "0"):
            issues.append("at rest: buttons=0b%s status=%s, expected 0b00001111 status=0" % b[0].groups())
        return issues

    def c5(self):
        m = self.start(350)
        self.b.wait_for(m, lambda s: False, 4.5)
        self.stop()
        got = self.lines(m, "C5", r"us=(\d+) off=(\d+)")
        if not got:
            return ["no C5 lines"]
        if self.fmt(got):
            return [self.fmt(got)]
        issues = []
        pairs = [(int(g[1].group(1)), int(g[1].group(2))) for g in got]
        theory = {900: 184, 1500: 307, 2100: 430}
        if [p for p, _ in pairs[:3]] != [900, 1500, 2100]:
            issues.append("pulses %s, expected 900 1500 2100" % [p for p, _ in pairs[:3]])
        for us, off in pairs:
            if us in theory and abs(off - theory[us]) > 1:
                issues.append("us=%d off=%d (theory %d)" % (us, off, theory[us]))
                break
        steps = [b[0] - a[0] for a, b in zip(got, got[1:])]
        if steps and not 0.9 <= median(steps) <= 1.15:
            issues.append("moves %.0f ms apart, expected 1000" % (1000 * median(steps)))
        return issues

    # ------------------------------------------------------------ D
    def d1(self):
        """D.1.3: one 8 s run at 50 % in each mode (D.1.1, D.1.2 are by hand)"""
        m = self.start(413)
        done = self.done_line(m, 413, 25)
        got = self.lines(m, "D1.3", r"mode=(TI1|TI12) counts=(-?\d+)")
        if not got:
            return ["D1.3: no lines"]
        if self.fmt(got):
            return ["D1.3: " + self.fmt(got)]
        issues = []
        modes = [g[1].group(1) for g in got]
        counts = [abs(int(g[1].group(2))) for g in got]
        if modes != ["TI1", "TI12"]:
            issues.append("D1.3: modes %s, expected TI1 TI12" % " ".join(modes))
        elif counts[0] == 0 or not 1.8 <= counts[1] / counts[0] <= 2.2:
            issues.append("D1.3: factor %.2f, expected 2" % (counts[1] / counts[0] if counts[0] else 0))
        if not done:
            issues.append("D1.3: no 'done' line")
        return issues

    def d2(self):
        m = self.start(420)
        done = self.done_line(m, 420, 320)                    # until two wraps: about 30 s on our cart
        self.stop()
        got = self.lines(m, "D2", r"t=(\d+) naive=(-?\d+\.\d\d) wrap=(-?\d+\.\d\d)")
        if not got:
            return ["no D2 lines"]
        if self.fmt(got):
            return [self.fmt(got)]
        issues = []
        t = [int(g[1].group(1)) for g in got]
        naive = [float(g[1].group(2)) for g in got]
        wrap = [float(g[1].group(3)) for g in got]
        lost = sum(1 for a, b in zip(t, t[1:]) if b - a != 10)
        if lost:
            issues.append("%d samples not 10 ms apart" % lost)
        speed = median([abs(w) for w in wrap]) or 1
        wraps = [i for i, (n, w) in enumerate(zip(naive, wrap)) if abs(n - w) > 10 * speed + 5]
        if len(wraps) < 2:
            issues.append("%d wraps seen, expected 2" % len(wraps))
        if max(abs(w) for w in wrap) > 3 * speed + 5:
            issues.append("the wrap= speed has a spike")
        if not done:
            issues.append("no 'done' line")
        return issues

    # ------------------------------------------------------------ E
    def e2(self):
        """E.2.1 to E.2.4: one step response per wheel, duty of the group's choice"""
        issues = []
        for n, tag in ((521, "E2.1"), (522, "E2.2"), (523, "E2.3"), (524, "E2.4")):
            m = self.start(n)
            done = self.done_line(m, n, 8)
            got = self.lines(m, tag, r"t=(\d+\.\d\d) w=(-?\d+\.\d\d)")
            if self.fmt(got):
                issues.append("%s: %s" % (tag, self.fmt(got)))
                continue
            ts = [round(float(g[1].group(1)) * 100) for g in got]
            if ts != list(range(1, 201)):
                issues.append("%s: %d samples, expected t=0.01 to 2.00" % (tag, len(ts)))
            if got:
                final = median([float(g[1].group(2)) for g in got[-50:]])
                if abs(final) < 1:
                    issues.append("%s: the wheel does not turn (final %.2f rad/s)" % (tag, final))
                elif final < 0:
                    issues.append("%s: forward speed is negative (%.2f rad/s)" % (tag, final))
            if not done:
                issues.append("%s: no 'done' line" % tag)
        return issues

    # ------------------------------------------------------------ protocol
    def protocol(self):
        mark = self.b.mark()
        self.b.write16(self.task_addr, 999)
        if not self.b.wait_for(mark, lambda s: s.strip() == "no task 999", 2):
            self.notes.append("no 'no task 999' line")
        self.b.write16(self.task_addr, 0)


def test_ass2(name, elf, port, logdir):
    """run all the automatic checks of A2; return {task: comment} and the Others comment"""
    syms = symbols(elf)
    if "task" not in syms or syms["task"][1] != 2 or syms["task"][2] not in "BD":
        return {}, "no global uint16_t task: not tested"
    board = Board(port)
    comments = {}
    tester = Tester2(board, syms)
    try:
        mark = board.mark()
        board.flash(elf)
        board.wait_for(mark, lambda s: False, 3)
        if any(s.startswith("=== task") for _, s in board.since(mark)):
            tester.notes.append("does not start idle")
        for key, check in [("A.1", tester.a1), ("A.2", tester.a2), ("A.3", tester.a3),
                           ("B.2", tester.b2), ("B.3", tester.b3),
                           ("C.1", tester.c1), ("C.2", tester.c2), ("C.3", tester.c3),
                           ("C.4", tester.c4), ("C.5", tester.c5),
                           ("D.1", tester.d1), ("D.2", tester.d2), ("E.2", tester.e2)]:
            print("  %s ..." % key, flush=True)
            try:
                issues = check()
            except RuntimeError as e:
                issues = [str(e)]
            comments[key] = "; ".join(issues) if issues else OK
            print("  %s: %s" % (key, comments[key]), flush=True)
        tester.protocol()
    finally:
        try:
            board.write16(tester.task_addr, 0)              # wheels stop
        except RuntimeError:
            pass
        os.makedirs(logdir, exist_ok=True)
        with open(os.path.join(logdir, safe(name) + ".log"), "w", encoding="utf-8") as f:
            t0 = board.lines[0][0] if board.lines else 0
            for t, s in board.lines:
                f.write("%9.3f  %s\n" % (t - t0, s))
        board.close()
    return comments, "; ".join(tester.notes)


TESTS = {1: test_ass1, 2: test_ass2}


def safe(name):
    return re.sub(r"[^\w.-]+", "_", name)


# ================================================================ Excel
YELLOW = PatternFill("solid", fgColor="FFF2CC")


def open_sheet(ass):
    """the workbook and the assignment's sheet, made with its header row if new"""
    wb = openpyxl.load_workbook(WORKBOOK) if os.path.exists(WORKBOOK) else openpyxl.Workbook()
    title = "Assignment %d" % ass
    if title in wb.sheetnames:
        ws = wb[title]
        head = [ws.cell(row=1, column=c).value for c in range(1, len(columns(ass)) + 1)]
        if head != columns(ass):
            # the items changed since the sheet was made: writing now would put comments in the wrong columns
            raise RuntimeError("the header of sheet '%s' does not match the items of the script (%s); "
                               "ask Yohai before changing the sheet" % (title, ", ".join(TASKS[ass])))
        fixed_rows(ws, ass)
        return wb, ws
    if wb.sheetnames == ["Sheet"] and wb["Sheet"].max_row == 1 and wb["Sheet"]["A1"].value is None:
        ws = wb["Sheet"]                                      # the empty default sheet of a new file
        ws.title = title
    else:
        ws = wb.create_sheet(title)
    for c, head in enumerate(columns(ass), 1):
        cell = ws.cell(row=1, column=c, value=head)
        cell.font = Font(bold=True)
        cell.alignment = Alignment(wrap_text=True, vertical="top")
        width = 8 if head.endswith("points") or head == "Total" else 24
        ws.column_dimensions[get_column_letter(c)].width = width
    ws.column_dimensions["A"].width = 30
    ws.freeze_panes = "B2"
    # keep the sheets in assignment order
    wb._sheets.sort(key=lambda s: s.title)
    fixed_rows(ws, ass)
    return wb, ws


# the rows every sheet starts with, in this order; groups are named only by number (no student names)
FIXED_ROWS = ["ref good solution", "ref wrong solution"] + ["group%d" % i for i in range(1, 11)]


def fixed_rows(ws, ass):
    """add the FIXED_ROWS that are missing, each with its Total formula; existing cells are never changed"""
    col = {head: c for c, head in enumerate(columns(ass), 1)}
    for name in FIXED_ROWS:
        row = find_row(ws, name)
        total = ws.cell(row=row, column=col["Total"])
        if total.value is None:
            points = [get_column_letter(c) + str(row) for head, c in col.items() if head.endswith("points")]
            total.value = "=100+" + "+".join("N(%s)" % p for p in points)
            total.font = Font(bold=True)


def find_row(ws, name, create=True):
    for r in range(2, ws.max_row + 1):
        if str(ws.cell(row=r, column=1).value or "").strip().lower() == name.lower():
            return r
    if not create:
        return None
    r = ws.max_row + 1
    ws.cell(row=r, column=1, value=name)
    return r


def set_comment(ws, row, col, text):
    cell = ws.cell(row=row, column=col, value=text)
    cell.alignment = Alignment(wrap_text=True, vertical="top")
    cell.fill = PatternFill() if text == OK else YELLOW


# Comments already in the sheet are Yohai's (he edits them after a run), so they are never overwritten
# silently: a new comment for a cell that holds a different one is kept in work/assN/pending.json until
# Yohai approves it (`apply`). Cells written by the latest `test` run of a group (work/assN/written.json)
# may still get report and code remarks appended. Points columns are never written.
def state_file(ass, what):
    return os.path.join(HERE, "work", "ass%d" % ass, what + ".json")


def load_state(ass, what, default):
    try:
        with open(state_file(ass, what), encoding="utf-8") as f:
            return json.load(f)
    except (OSError, ValueError):
        return default


def save_state(ass, what, data):
    os.makedirs(os.path.dirname(state_file(ass, what)), exist_ok=True)
    with open(state_file(ass, what), "w", encoding="utf-8") as f:
        json.dump(data, f, indent=2)


def propose(ass, ws, row, col, name, task, new, written, pending):
    """write `new` into an empty cell (or one this run wrote); otherwise queue it for approval"""
    old = ws.cell(row=row, column=col).value
    key = "%s|%s" % (name.lower(), task)
    if old == new or (isinstance(old, str) and old.startswith(new + "; ")):
        return False                                          # same result (plus the review remarks after it)
    if old in (None, "") or key in written:
        set_comment(ws, row, col, new)
        if key not in written:
            written.append(key)
        return True
    pending[:] = [p for p in pending if not (p["name"].lower() == name.lower() and p["task"] == task)]
    pending.append({"name": name, "task": task, "old": old, "new": new})
    return False


def write_results(ass, name, comments, others):
    wb, ws = open_sheet(ass)
    row = find_row(ws, name)
    col = {head: c for c, head in enumerate(columns(ass), 1)}
    written = [k for k in load_state(ass, "written", []) if not k.startswith(name.lower() + "|")]
    pending = load_state(ass, "pending", [])
    items = dict(comments)
    if others:
        items["Others"] = others
    for task, text in items.items():
        propose(ass, ws, row, col[task + " comment"], name, task, text, written, pending)
    points = [get_column_letter(c) + str(row) for head, c in col.items() if head.endswith("points")]
    total = ws.cell(row=row, column=col["Total"], value="=100+" + "+".join("N(%s)" % p for p in points))
    total.font = Font(bold=True)
    save(wb)
    save_state(ass, "written", written)
    save_state(ass, "pending", pending)


def add_note(ass, name, task, text, kind):
    """append "<kind>: text" to a task's comment (the report and code review)"""
    wb, ws = open_sheet(ass)
    row = find_row(ws, name, create=False)
    if row is None:
        sys.exit("no row for '%s' in %s" % (name, ws.title))
    col = {head: c for c, head in enumerate(columns(ass), 1)}
    if task + " comment" not in col:
        sys.exit("no task '%s' in %s" % (task, ws.title))
    c = col[task + " comment"]
    old = ws.cell(row=row, column=c).value
    note = "%s: %s" % (kind, text)
    if old and note in old:
        return
    written = load_state(ass, "written", [])
    pending = load_state(ass, "pending", [])
    queued = next((p for p in pending if p["name"].lower() == name.lower() and p["task"] == task), None)
    if queued:                                                # add to the change already waiting
        queued["new"] = queued["new"] + "; " + note
    elif propose(ass, ws, row, c, name, task, note if not old else old + "; " + note, written, pending):
        save(wb)
    save_state(ass, "written", written)
    save_state(ass, "pending", pending)


def show_pending(ass):
    pending = load_state(ass, "pending", [])
    if not pending:
        print("no changes waiting")
    for i, p in enumerate(pending, 1):
        print("%d. %s, %s\n   now: %s\n   new: %s" % (i, p["name"], p["task"], p["old"], p["new"]))
    return pending


def apply_pending(ass, name, tasks):
    """write the waiting changes that Yohai approved (all of a group, or some of its items)"""
    pending = load_state(ass, "pending", [])
    wb, ws = open_sheet(ass)
    col = {head: c for c, head in enumerate(columns(ass), 1)}
    keep = []
    for p in pending:
        if (name is None or p["name"].lower() == name.lower()) and (not tasks or p["task"] in tasks):
            row = find_row(ws, p["name"], create=False)
            if row is not None:
                set_comment(ws, row, col[p["task"] + " comment"], p["new"])
                print("applied: %s, %s" % (p["name"], p["task"]))
                continue
        keep.append(p)
    save(wb)
    save_state(ass, "pending", keep)


def drop_pending(ass, name, tasks):
    pending = load_state(ass, "pending", [])
    keep = [p for p in pending if not ((name is None or p["name"].lower() == name.lower())
                                       and (not tasks or p["task"] in tasks))]
    save_state(ass, "pending", keep)
    print("dropped %d" % (len(pending) - len(keep)))


def save(wb):
    try:
        wb.save(WORKBOOK)
    except PermissionError:
        sys.exit("cannot write '%s': close it in Excel and run again" % WORKBOOK)


# ================================================================ submissions
def group_name(zip_path):
    """assN_groupK.zip -> 'groupK' (the course policies: only the group number, no names)"""
    base = os.path.splitext(os.path.basename(zip_path))[0]
    m = re.search(r"group[\s_-]*0*(\d+)", base, re.IGNORECASE)
    if m:
        return "group%d" % int(m.group(1))
    return " ".join(base.split("_")[1:]) or base


def find_project(folder):
    """the folder that holds .project and .cproject, extracting nested ZIP files first"""
    for z in glob.glob(os.path.join(folder, "**", "*.zip"), recursive=True):
        with zipfile.ZipFile(z) as f:
            f.extractall(os.path.splitext(z)[0])
    for root, _, files in os.walk(folder):
        if ".project" in files and ".cproject" in files:
            return root
    return None


def find_reports(folder, project):
    """the PDF files of a submission that are not inside the project"""
    pdfs = glob.glob(os.path.join(folder, "**", "*.pdf"), recursive=True)
    return [p for p in pdfs if not (project and os.path.abspath(p).startswith(os.path.abspath(project)))]


def com_port():
    for p in serial.tools.list_ports.comports():
        if p.vid == 0x0483:                                   # STMicroelectronics
            return p.device
    sys.exit("no ST-LINK COM port found")


def cmd_test(a):
    groups = []                                               # name, project, elf, reports
    work = os.path.join(HERE, "work", "ass%d" % a.ass, "submissions")    # unpacked anew every run
    if a.submissions:
        shutil.rmtree(work, ignore_errors=True)
        for z in sorted(glob.glob(os.path.join(a.submissions, "*.zip"))):
            name = group_name(z)
            folder = os.path.join(work, safe(name))
            with zipfile.ZipFile(z) as f:
                f.extractall(folder)
            project = find_project(folder)
            groups.append((name, project, None, find_reports(folder, project)))
        if not groups:
            sys.exit("no ZIP files in " + a.submissions)
    elif a.name and (a.project or a.elf):
        reports = [a.report] if a.report else []
        if not reports and a.project:
            # the report usually sits next to the project folder (e.g. ass1_sol.pdf beside ass1_sol/)
            parent = os.path.dirname(os.path.normpath(os.path.abspath(a.project)))
            reports = sorted(glob.glob(os.path.join(parent, "*.pdf")))
        groups.append((a.name, a.project, a.elf, reports))
    else:
        sys.exit("give --submissions, or --name with --project or --elf")

    if os.path.exists(WORKBOOK):                              # fail now, not after minutes on the board
        try:
            open(WORKBOOK, "r+b").close()
        except PermissionError:
            sys.exit("'%s' is open in Excel: close it and run again" % WORKBOOK)
        backup = os.path.join(HERE, "work", "ass%d" % a.ass, "gradings_backup_%s.xlsx" % time.strftime("%Y-%m-%d_%H%M"))
        os.makedirs(os.path.dirname(backup), exist_ok=True)
        shutil.copy2(WORKBOOK, backup)                        # nothing in the sheet can be lost
        print("workbook backed up to " + backup, flush=True)
    port = a.port or com_port()
    summary = {g["name"].lower(): g for g in load_state(a.ass, "summary", [])}
    for name, project, elf, reports in groups:
        print(name, flush=True)
        try:
            if not elf:
                if not project:
                    raise RuntimeError("no CubeIDE project found")
                elf = build(project)
            comments, others = TESTS[a.ass](name, elf, port, os.path.join(HERE, "logs", "ass%d" % a.ass))
        except (RuntimeError, subprocess.TimeoutExpired) as e:
            comments, others = {}, "not tested: %s" % e
        if not reports:
            others = "; ".join(x for x in (others, "no report PDF found") if x)
        write_results(a.ass, name, comments, others)
        summary[name.lower()] = {"name": name, "project": project and os.path.abspath(project),
                                 "reports": [os.path.abspath(r) for r in reports],
                                 "comments": comments, "others": others}
        print("  -> %s, sheet 'Assignment %d'" % (WORKBOOK, a.ass), flush=True)
    save_state(a.ass, "summary", list(summary.values()))
    show_pending(a.ass)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)
    t = sub.add_parser("test", help="build, flash and test, write the comments")
    t.add_argument("--ass", type=int, required=True)
    t.add_argument("--submissions")
    t.add_argument("--project")
    t.add_argument("--elf")
    t.add_argument("--name")
    t.add_argument("--report")
    t.add_argument("--port")
    n = sub.add_parser("note", help="add a report or code remark to an item")
    n.add_argument("--ass", type=int, required=True)
    n.add_argument("--name", required=True)
    n.add_argument("--task", required=True)
    n.add_argument("--text", required=True)
    n.add_argument("--kind", choices=["report", "code"], default="report")
    p = sub.add_parser("pending", help="show the changes that wait for approval")
    p.add_argument("--ass", type=int, required=True)
    for cmd, help_text in (("apply", "write the approved changes"), ("drop", "forget changes that were refused")):
        x = sub.add_parser(cmd, help=help_text)
        x.add_argument("--ass", type=int, required=True)
        x.add_argument("--name", help="only this group (default: all)")
        x.add_argument("--task", action="append", help="only these items (repeat; default: all)")
    a = ap.parse_args()

    refusal = no_reference(a.ass)
    if refusal:
        print(refusal)
        sys.exit(2)
    if a.cmd == "test":
        cmd_test(a)
    elif a.cmd == "note":
        add_note(a.ass, a.name, a.task, a.text, a.kind)
    elif a.cmd == "pending":
        show_pending(a.ass)
    elif a.cmd == "apply":
        apply_pending(a.ass, a.name, a.task)
    else:
        drop_pending(a.ass, a.name, a.task)


if __name__ == "__main__":
    main()
