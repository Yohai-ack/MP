---
name: skl_prj_getCarParams
description: Measure the car-specific parameters of ONE MP cart (STM32 repo), given its number as the argument (/skl_prj_getCarParams 12), interactively with Yohai at the cart, and write them into the A3 solution's car_params.c. Step 1 spins each motor with DIR = 1 (Yohai says which wheel turned, forward or backward) for wheelMotor (the motor-to-wheel map, which differs between cars), motorDir and encDir; step 2 has Yohai turn each wheel 3 turns by hand for countsPerRev; step 3 records an open-loop 50 % step of each wheel, fits K and tau, designs Kp and Ki for no overshoot and a closed-loop time constant tauC (default 0.032 s), and computes maxVelCmdNoSat; step 5 finds the servo zero (the pulse at which the sonar looks straight ahead, by stepping the servo while Yohai watches) and the IR sensor's obstacle level. Use when Yohai invokes /skl_prj_getCarParams or asks to measure, calibrate or add the parameters of a car.
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
- Motors (the A2 table): motor N = PWM TIM2_CHN, DIRN, encoder TIM1 / TIM8 / TIM3 / TIM4 for N = 1..4. **Which wheel
  each motor drives differs between cars** (car 12: 1 rear right, 2 rear left, 3 front left, 4 front right; car 11:
  motors 3 and 4 swapped): never assume it, ask in step 1. All per-wheel arrays in car_params.c are per MOTOR;
  wheelMotor maps the positions. "Forward" is the direction that drives the cart forward.
- PuTTY and any CubeIDE debug session must be closed (they block the COM port and the ST-LINK).
- Never commit without Yohai's approval.

## Steps

0. **Prepare** (ask "ready?": it flashes the A2 reference): `python .claude/skills/skl_prj_getCarParams/car_params_tool.py prepare`.
   Idle, nothing moves; the encoders are set to TI1 and TI2.
1. **Wheels and directions.** For N = 1 to 4, one at a time (ask "ready?" before each): `... car_params_tool.py spin N`.
   Motor N runs with its DIR pin = 1 at 30 % for 1 s; the tool prints the change of every counter. Check that only
   motor N's own timer moved (the table above; if not, stop and tell Yohai: the encoder cable is not with its motor).
   Ask Yohai **which wheel turned** (rear right, rear left, front left, front right) **and whether forward or
   backward**. After the four, write the map: `... car_params_tool.py setmap X <RR,RL,FL,FR>` (the motor number at
   each position, e.g. `1,2,4,3`). Then
   - motorDir[N] = +1 if DIR = 1 drove it forward, -1 if backward;
   - encDir[N] = +1 if the counter goes up for a forward turn: the sign of the change, flipped if the wheel went backward.
   Show the table of the four results (motor, wheel, motorDir, encDir) before going on. (The two sides of a car are
   mirrored, so usually the two right wheels share signs, and the two left ones the opposite ones.)
2. **Counts per revolution.** Nothing moves by motor; no "ready?" needed, but go one wheel at a time. Read the
   counters (`... car_params_tool.py cnt`) as the start value, tell Yohai the start value, and ask him to turn the
   wheel exactly 3 full turns by hand and say "done". **Go by wheel position, not by motor number: rear right, then
   front right, then the left side** (Yohai's order, 2026-11-09); name the wheel and its motor (from step 1's map).
   The reading after one wheel is the start value of the next (read again only if that wheel's counter moved). Read again: counts per revolution = |change| / 3 (the counters are 16
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
   the servo at 1500 us.
   - **Servo zero** (the only car-specific servo value): find the pulse at which the sonar looks straight ahead.
     Yohai watches; after each move ask him "more", "less" or "good". Steps of 50 us
     (`... servo 1550`, `... servo 1600`, ...; "less" goes the other way), and once he says "too far", go back halfway
     and continue in steps of 10 us (a 10 us step is about 1 degree). A longer pulse turns the sonar to the right.
     Write it: `... car_params_tool.py setzero X <us>`. (Car 11, 2026-11-09: 1575 us.)
   - The scale is NOT car-specific: SERVO_DEG_PER_US in `ass3.h` (-124 deg / 1200 us, + left, measured on car 12).
     The course scans symmetrically about the zero, within 900 to 2100 us.
   - **IR:** `... ir` with nothing in front of the IR sensor, then with a hand about 5 cm in front (ask him to say
     "go" before each); irObstacleLevel is the level read with the hand. Write it: `... car_params_tool.py setir X <0|1>`.
   `setzero` and `setir` can run before steps 1 to 4: a car with no entry gets a partial one, which carParamsGet
   refuses (so the A3 solution does not drive it) until `write` adds the wheels; `write` keeps both sensor lines.
6. **Check and report.** Compile-check `car_params.c` (`arm-none-eabi-gcc -c ... -Wall -Wextra`, 0 warnings). Report a
   table per wheel: motorDir, encDir, countsPerRev, K, tau, Kp, Ki, maxVelCmdNoSat, and say which wheel limits the
   speed (the A3 solution drives at 0.9 x the smallest maxVelCmdNoSat). Add irObstacleLevel and servoZeroUs to the report. Note anything unusual (a wheel much weaker,
   a fit rms well above one count step). Add a line to MP.md. Remind Yohai that the cart still runs the temporary
   copy of the A2 reference, and ask before committing.
