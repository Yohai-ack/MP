---
name: skl_prj_getCarParams
description: Measure the car-specific parameters of ONE MP cart (STM32 repo), given its number as the argument (/skl_prj_getCarParams 12), interactively with Yohai at the cart, and write them into the A3 solution's car_params.c. Step 1 spins each motor with DIR = 1 (Yohai says forward or backward) for motorDir and encDir; step 2 has Yohai turn each wheel 3 turns by hand for countsPerRev; step 3 records an open-loop 50 % step of each wheel, fits K and tau, designs Kp and Ki for no overshoot and a closed-loop time constant tauC (default 0.032 s), and computes maxVelCmdNoSat; step 5 reads the servo's angles at 900, 1500 and 2100 us (Yohai says them) and the IR sensor's obstacle level. Use when Yohai invokes /skl_prj_getCarParams or asks to measure, calibrate or add the parameters of a car.
---

# skl_prj_getCarParams: the parameters of one car

The argument is the **car number X** (the number written on the car). Ask for it if it is missing.

Everything goes into `assignments/ass3/solution/fullSolution/ass3_sol/Core/Src/car_params.c` (one entry per car; an
old entry of car X is replaced), and the step recordings into `assignments/ass3/solution/fullSolution/car_data/
carX_step_wK.txt`. The A3 solution picks the car with `carId` in its `main.c`.

The tool is `car_params_tool.py` in this folder (run it from the repo root with `python`, not `python -I`: it needs
pyserial, installed `--user`). It drives the cart over SWD (STM32_Programmer_CLI, HOTPLUG) and reads the ST-LINK COM
port, as the autotest does.

## Rules (from MP.md and memory)

- **Before every action that flashes or moves anything, explain it, say that the wheels must be in the air (cart on its
  block), ask "ready?", and start only after Yohai says yes.** One spin, one question.
- Wheels: 1 rear right (TIM1), 2 rear left (TIM8), 3 front left (TIM3), 4 front right (TIM4). "Forward" is the
  direction that drives the cart forward.
- PuTTY and any CubeIDE debug session must be closed (they block the COM port and the ST-LINK).
- Never commit without Yohai's approval.

## Steps

0. **Prepare** (ask "ready?": it flashes the A2 reference): `python .claude/skills/skl_prj_getCarParams/car_params_tool.py prepare`.
   Idle, nothing moves; the encoders are set to TI1 and TI2.
1. **Directions.** For N = 1 to 4, one at a time (ask "ready?" before each): `... car_params_tool.py spin N`. Motor N
   runs with its DIR pin = 1 at 30 % for 1 s; the tool prints the change of every counter. Check that only that
   wheel's own timer moved (the table above; if not, stop and tell Yohai). Ask Yohai: forward or backward? Then
   - motorDir[N] = +1 if DIR = 1 drove it forward, -1 if backward;
   - encDir[N] = +1 if the counter goes up for a forward turn: the sign of the change, flipped if the wheel went backward.
   Show the table of the four results before going on. (The two sides of a car are mirrored, so usually wheels 1 and 4
   share signs, and 2 and 3 the opposite ones.)
2. **Counts per revolution.** Nothing moves by motor; no "ready?" needed, but go one wheel at a time. Read the
   counters (`... car_params_tool.py cnt`) as the start value, tell Yohai the start value, and ask him to turn wheel N
   exactly 3 full turns by hand and say "done". Read again: counts per revolution = |change| / 3 (the counters are 16
   bits: take the change modulo 65,536). If he says he moved it before starting, read a fresh start value. The encoder
   gives 1,320 (11 lines x 30:1 x 4 edges in TI1 and TI2): if all four are within about 0.5 % of it, use 1320 for all
   four and keep the hand values in the comment of the entry (edit it after step 3).
3. **Step responses** (ask "ready?": it flashes a copy of the A2 reference with this car's directions, then runs each
   wheel at 50 % for 2 s, one after the other, about 20 s):
   `... car_params_tool.py steps X <motorDir> <encDir>` with the signs as `-1,1,1,-1`. Every wheel's mean speed must
   be **positive** (the wheel ran forward and its sign is right); if not, the directions of step 1 are wrong: stop.
   The build needs the CubeIDE toolchain (`C:\ST\STM32CubeIDE_*`), so it runs only on the PC that has it.
4. **Fit and design** (no cart): `... car_params_tool.py write X <motorDir> <encDir> <cpr1,cpr2,cpr3,cpr4> [tauC]`.
   It fits w = K u (1 - exp(-t/tau)) (u = 6 V), sets Ki = Kp / tau and Kp = tau / (K tauC) (the PI zero on the motor
   pole: a first-order closed loop with time constant tauC, no overshoot), checks the overshoot in a simulation of
   the real 100 Hz loop, and computes maxVelCmdNoSat (the largest step from rest whose PI output stays within 90 %
   duty, transient included: 0.9 x 12 V / (Kp + Ki x 0.01 s)). Default tauC 0.032 s (Yohai, 2026-11-07); use
   another only if Yohai asks.
5. **Servo and IR sensor** (ask "ready?": it flashes the A3 solution, `ass3_sol/Debug/ass3_sol.elf`, build it first
   if it is missing; the board resets and only the servo moves). `... car_params_tool.py calstart` runs task 900 with
   the servo at 1500 us. Then `... servo 900` and `... servo 2100` (the course's servo range), one at a time, when
   Yohai asks; he notes the sonar's direction at each (degrees from straight ahead, + left, - right; he may give all
   three at the end), and `... servo 1500` to finish. IR: `... ir` with nothing in front of the IR sensor, then with
   a hand about 5 cm in front (ask him to say "go" before each); irObstacleLevel is the level read with the hand.
   Write both: `... car_params_tool.py sensors X <IR> <deg900,deg1500,deg2100>`. The course uses these three points
   for every servo angle (straight ahead, the side each pulse turns to).
6. **Check and report.** Compile-check `car_params.c` (`arm-none-eabi-gcc -c ... -Wall -Wextra`, 0 warnings). Report a
   table per wheel: motorDir, encDir, countsPerRev, K, tau, Kp, Ki, maxVelCmdNoSat, and say which wheel limits the
   speed (the A3 solution drives at 0.9 x the smallest maxVelCmdNoSat). Add irObstacleLevel and servoDeg to the report. Note anything unusual (a wheel much weaker,
   a fit rms well above one count step). Add a line to MP.md. Remind Yohai that the cart still runs the temporary
   copy of the A2 reference, and ask before committing.
