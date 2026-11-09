# MP.md — Microprocessors course (TA work), read first every session

This file is the context for the **Microprocessor-Based Product Design** course. Read it at the start
of every session, so Yohai never has to re-explain the course. Keep it current: when something about
the course is settled, update this file in the same session.

## 1. Who, what, when

- **Course:** Microprocessor-Based Product Design (MP), Technion, Faculty of Mechanical Engineering,
  2026/27 winter semester.
- **Lecturer:** Prof. Izhak Bucher. **TA:** Yohai Ackerman (that's who I work for), and he's the
  **only TA this year**. In 2025 there were two: Yohai and Rami Sabbagh. So 2025 tutorials may be
  Rami's work.
- **Dates:** semester begins **2026-10-28**, ends **2027-01-28**. A3 is due later, on 3.3.2027. 13 meetings:
  sessions 1 to 12 are each a 2 h lecture + 1 h tutorial, and session 13 is a lab meeting. Lab work is self-run in **Room 26** (Robotics/Manufacturing
  building).
- **Students** know Python, not C. Two lectures bridge Python → C.
- **Yohai's own level:** somewhat familiar with STM32, still learning. He prepares before the
  semester. Explain from first principles when he asks; don't assume embedded background.

## 2. Folders and permissions — the most important rules

| Folder | What it is | My permission |
|---|---|---|
| `STM32\` (this folder, and all subfolders): `C:\Users\yohai\git_codes\STM32\` or `D:\git_codes\STM32\`, depending on the PC (see below) | Yohai's **private** working folder. Bucher has **no** access. Git repo → `https://github.com/Yohai-ack/MP.git` | **Read and write**, but only on Yohai's request or as these rules say |
| `C:\Users\yohai\OneDrive - Technion\Izhak Bucher's files - shared_2026\` | Bucher's shared folder. Yohai has write access, and puts files there when he wants Bucher to see them | **READ-ONLY, ALWAYS.** Never write, move, rename or delete anything there, even if asked in passing. If Yohai wants something placed there, I prepare it in `STM32\` and he copies it himself |

- **Two PCs.** Yohai works on two PCs, and the repo is cloned on both: `C:\Users\yohai\git_codes\STM32\`
  on one, `D:\git_codes\STM32\` on the other. He moves work between them with git push/pull, so
  check `git status` and the latest commit at the start of a session. Paths written in files (e.g.
  the checkBounce instruction sheets) should give both. The tools differ too: CubeIDE 2.2.0 and its
  ARM toolchain are on the C: PC (§5). On the D: PC only a stub `C:\ST\STM32CubeIDE_1.5.1` was
  found (no compiler, 2026-10-14), so there the best check is a host-GCC `-fsyntax-only`.
  pdflatex works there (checked 2026-10-18; /skl_glb_tex2Word needed
  `python -m pip install pypandoc_binary` there, done 2026-10-19), and so does PowerPoint, which is also used to render
  slides for checking.
- **Last year's folder** `C:\Users\yohai\OneDrive - Technion\Bucher Izhak's files - Shared_2025\`:
  **READ-ONLY, ALWAYS**, same rule as above (see §4.6a).
- Read the shared folder freely to understand the course. Bucher will keep adding material
  (lessons, code), so **re-survey it** when starting a task that depends on it, rather than trusting
  the snapshot in §4.
- **Ignore every folder named `old`**, in either location.
- **Git:** never commit or push without Yohai's approval. Show what changed, then ask.
- Built files stay out of git (`Debug/`, `Release/`, `*.elf`, Office lock files `~$*`). See
  `.gitignore`.
- Office files open in PowerPoint/Word are locked and can't be read. If a read fails, say which
  file and ask Yohai to close it. Don't silently skip it. Word can hold a file locked after its
  window is closed: if a write is refused, ask Yohai to quit Word completely.
- **Never drive real hardware** (flash a board, run motors) on my own. Building and static analysis
  are fine. Anything that runs on the cart, Yohai does or approves.

## 3. The scope of my help

1. **Understanding questions.** Yohai is learning STM32/embedded C. His running question list is
   `others/rolling doc MP course.docx` (questions keyed to lesson + slide, e.g. "Les2 s17").
2. **Tutorials.** Help build the 1 h tutorials that accompany each lecture.
3. **Assignments.** Write **one unified assignments document** covering all labs. Bucher's lessons
   mention assignments piecemeal; the unified doc is ours. Then **solve** the assignments (reference
   solutions).
4. **Quizzes.** Two quizzes (lectures 4 and 9). Help write them and their solutions.
5. **Grading.** Possibly help check submitted assignments.

Course-specific technical conclusions belong here or in a document in this folder, not only in chat.

## 4. The course as it stands (snapshot 2026-10-02 — re-check the shared folder)

### 4.1 Platform — one cart, one MCU

- **A1's `.ioc` is provided, not built by students** (agreed with Bucher, 2026-10-15): `assignments/ass1/ass1.ioc`,
  generated from the hardware-test `.ioc` with only A1's items (GPIO LD2/B1/IR_SENSOR, LPUART1,
  I2C1, EXTI15_10). Checked 2026-10-15: CubeMX 6.18 headless generates it, and it builds with
  `io_board.c`, 0 warnings. Tutorial 6 teaches creating a `.ioc` (moved from tutorial 5 on 2026-10-19; tutorial 5 is now I2C).
  For A2, students duplicate `ass1.ioc` (CubeMX *Save Project As* → `ass2/ass2.ioc`) and add only
  the new items. **No `ass2.ioc` is ever given to students** (Yohai, 2026-10-17): they get only the
  pinout and settings, as the "Assignment 2: project configuration" section of the A2 instructions.
  A TA-built `ass2.ioc`, if made to check that section, is TA-only (under a `solution/` folder).
- **Pin/peripheral configuration for students: a "project configuration (.ioc)" section at the end
  of each assignment** in `MP2026_plan.tex` (Yohai, 2026-10-11; replaced the separate
  `ass_*_ioc.tex` files, now deleted). For A1 it's a reference list of what `ass1.ioc` contains.
  **A1 exception (Yohai, 2026-10-26):** A1's full configuration section was replaced by one friendly table near
  the top, "The hardware you use, and how to reach it from your code" (what / where / the call or macro:
  LD2_Pin, IR_SENSOR_Pin, io_board functions, PuTTY settings), plus the power-jumper note. Clock, SWD pins, NVIC and
  the I2C addresses were dropped from A1 (students get `ass1.ioc` and never change it); the full I2C device table
  (all 5 chips) now lives in A2's configuration section, where the bus is opened.
  Each lists only what that assignment needs, so students
  aren't confused, and they're **cumulative**: A2's contains everything in A1's, even where A2
  doesn't use it. A3 may share A2's or get its own. Unconfirmed hardware stays out of them (open
  items in §6). All values come from the working hardware-test project
  (`stmProjects/hardwaret_test/align_servo.ioc` + `main.c`).
- **CubeIDE 2.x has no built-in CubeMX.** Double-clicking a `.ioc` opens the separate STM32CubeMX
  (installed at `%LOCALAPPDATA%\Programs\STM32CubeMX`). Projects come into CubeIDE through
  *File → Import → STM32CubeMX/STM32CubeIDE Project*. Students need both programs. To check a
  `.ioc` headlessly: `jre\bin\java.exe -jar STM32CubeMX.exe -q script.txt` (the `.exe` alone
  opens the GUI), with `config load`, `project toolchain Makefile`, `project path`,
  `project generate`, `exit`; then `make` with CubeIDE's toolchain.
- **The system clock is PLL from HSI (16 MHz / 4 × 85 / 2 = 170 MHz)**, not HSE, in the working
  hardware-test project (`RCC_PLLSOURCE_HSI`). The HSE/LSE pins are only board defaults.
- **USART3 (PC10 TX / PC11 RX, 115200)** is the **wireless module** (used in 2025). It may be used
  in A3 this year, so it's left out of the A1/A2 docs for now.
- **History:** in 2025, Lab 1 ran on an older lab system (F446, steppers), and Labs 2–3 on the
  Mecanum cart (4 DC motors, 4 encoders, one binary IR sensor at the front). **In 2026, every
  assignment uses the Mecanum cart** ("car" and "cart" mean the same thing). New this year: a
  servo-gimballed **HC-SR04 ultrasonic** (gives distance, not just presence) and the **I2C board
  with 8 RGB LEDs** (+ buttons).
- **NUCLEO-G474RE** (STM32G474RE, Cortex-M4F, 170 MHz) in a **Mecanum-wheel cart**. It's the only
  MCU in the course. The old STM32F446ZE boards and the Wiggler project are retired.
- **I2C1:** SCL = PB8, SDA = PB9. **printf** over LPUART1 at 115200 8N1 (ST-LINK virtual COM, PuTTY).
- **The whole course runs I2C1 at 400 kHz (Fast mode)** (Yohai, 2026-10-22), so A1 Part C can show
  contact bounce (at 100 kHz the reads are too far apart to see it; see bounceCheck400 in §4.6).
  Timing register 0x40621236 = the value the CubeMX GUI computes for Fast mode (it recomputes it
  when the .ioc is opened, so students get this value). First tests used a hand-computed
  0x30C22737; both meet the Fast-mode rules. CubeMX headless does NOT compute it (with the value
  removed it falls back to 0x00303D5B, wrong for 170 MHz). Switched everything to 0x40621236 on
  2026-10-22 and re-tested on the board: all passed. At 400 kHz in: ass1.ioc, align_servo.ioc +
  hardware test main.c, bounceCheck400 (.ioc + main.c), tut1proj (.ioc + main.c), tutorials/
  tut2proj.ioc, and inside tutorials/tut2Files/tut2ForMoodle.zip (.ioc + main.c; if already on
  Moodle, re-upload). checkBounce and checkBounce2 stay at 100 kHz on purpose: they record the
  100 kHz measurements. Tutorial deck updated for 400 kHz: slide 32 (CubeMX setting), 34 (0.08 ms
  per LED write, 2.5 us per bit), 35 (24 writes about 2 ms), 36 (0.08 ms per pass; the 1.2 ms
  pacing and 29 ms refresh are set by code, unchanged). The 0.08 ms is calculated (29 bits x
  2.5 us + HAL), consistent with the measured 110 us per button read. A red flag in A1 Part C
  asks Bucher to agree to 400 kHz course-wide. Set in ass1.ioc
  (I2C_Speed_Mode = I2C_Fast; CubeMX headless generates it unchanged) and in the hardware test
  (main.c + align_servo.ioc). Reads tested on the board (0 errors); the hardware test at 400 kHz
  passed all checks on 2026-10-22 (all 5 chips FOUND, LEDs white and fading, servo, buttons to
  wheels, wCounter, sonar). Still to do:
  see what Timing value the CubeMX GUI shows when ass1.ioc is opened; tell Bucher (his I2C slides
  and code assume 100 kHz). The plan's A1/A2 tables and A2 C.3 now say 400 kHz.
- **I2C IO board:** 8 RGB LEDs via 3× **PCA9634** (one chip per colour: RED 0x01, GREEN 0x02,
  BLUE 0x07; channel n = LED n). 4 buttons on **PCA9538A** at 0x71 (active-low, bits 0–3).
  **PCA9685** at 0x44 makes 50 Hz servo pulses (PRESCALE = 121).
- **Servo + HC-SR04 sonar** (scanning "radar"). Trigger from an MCU pin; echo measured by timer
  input capture. The working test code uses **900 / 1500 / 2100 µs** (min/centre/max), so use
  that, not the syllabus's 1000–2000 µs.
- **4 DC motors + quadrature encoders**, through H-bridges (timer PWM).
- **IR on/off obstacle sensor** at the front (PA7, active-low): a digital input, later used with EXTI.
- Power-jumper note (corrected by Yohai, 2026-10-26): the Nucleo power-source jumper must be on **E5V**,
  so the board takes its 5 V from the cart (the 2026-10-01 note said 5V_STLK; that was wrong). With the
  jumper on E5V and the cart's power off, the MCU is unpowered while the ST-LINK still enumerates (USB),
  which gives "No STM32 target found". In the A1 hardware table, `ass1_sol.tex`, `HowToUse.txt` and the
  checkBounce/checkBounce2/bounceCheck400 instruction sheets (the `_filled` records keep what was used then).

### 4.2 Grading (syllabus `syllabus_G474_cart_2026`)

| Lab | Content | Weeks | Weight |
|---|---|---|---|
| 1 | Real-time digital IO: HAL_Delay, logic, loops, buttons, optical sensor | 1–4 | 15% |
| 2 | Timers, PWM & sensors: timer interrupts, servo, SR04 input capture, EXTI, own I2C drivers | 4–7 | 22% |
| 3 | Wheels: PI speed control @100 Hz, Mecanum kinematics, odometry, trajectories | 7–10 | 26% |
| 4 | Cart + MATLAB: UART protocol, App Designer GUI, sonar polar map, Simulink → C | 10–13 | 28% |

That's 91% total, plus up to 3% creativity bonus per lab (the 9% was originally meant as the
creativity bonus). Dates go on Moodle.

**Agreed with Bucher (2026-10-08):** **3 assignments + 2 quizzes**, replacing the 4 labs above.
A1 "No wheels" (NUCLEO + IO board: types, wrap-around, bit ops, logic, blinking, RGB). A2 "Wheels in
the air" (motors on the block). A3 "Wheels on the ground". **Weights: Quiz 1 10%, Quiz 2 10%,
A1 20%, A2 30%, A3 30%.** (A2/A3 changed from 25/35 by Yohai, 2026-10-19.) Each quiz takes the **first hour of a lecture**: Quiz 1 in lecture 4
(24.11.2026), Quiz 2 in lecture 9 (29.12.2026; moved from lecture 8 on 2026-10-17). Yohai will probably
write the quizzes.

**A1 revisions (Yohai, 2026-10-26):** D.1 is timed with HAL_GetTick. D.3 = RGB LEDs 0 and 1 at 1 and 1.3 Hz, 10
blinks timed per LED, compared with the calculation.

**A1 made testable automatically (Yohai, 2026-10-28):** the whole assignment is one program run by
`volatile uint8_t task` (exact name and type, global), with the solution's numbering: 11 to 13 A, 21 to 23 B, 31 to 36
C, 41 to 44 D, 51 E. A dispatcher skeleton is in the instructions; protocol: start idle, print `=== task N ===`, check
`task` every loop pass, `=== task N done ===` for tasks with an end, write 0 then N to rerun, `no task N`. Every result
goes to PuTTY in a MANDATORY tagged line format (`A1: u8=250 0xFA 0b11111010 ...`, hex padded to the type, binary
`0b` + all bits). A.1 fixed: start 250 / 120 / 65530, +1 every 500 ms, 12 steps. D.2 variable fixed:
`volatile uint32_t blinkMs = 500`. B.4 moved to C.1 (buttons held); the counting versions are now C.2 level, C.3
edge, C.4 debounced, C.5 delay (35 debounced, 36 edge), each with an "Expect ..." hint (much more than 20 / 20 or a
little more / exactly 20 / much less than 20). Videos only `V_C` and `V_E` (where input is needed).
**A1 shortened (Yohai, 2026-11-03), tested on the cart 2026-11-04** (all of it; see the cart session below): A.1 gives `printBinary`
as a listing (students call it from their own line function); Part C has no LEDs, button 1 ends a round (prints the
result line once, then ignored 0.5 s against bounce; C.2's line adds passes/s over the round), nothing printed while
counting, no round of light presses in C.3; the video shows one representative round of 20 per version (3 rounds stay
in the report). Part E has no dimming: a brightening always runs its 2 s, the next one only after the condition went
false and true again after its end; inputs still read on every pass. Done in the plan (red flag at the top of A1), in
both reference projects (`ass1_sol`, `ass1_wrong`: partC.c, partE.c, printBinary; 0 warnings) and both reports (red
"not yet tested" note in their status). The TA bounce numbers came from bounceCheck400, which writes no LEDs, so the
C hints fit the new version better.

**Automatic checking of submissions (2026-10-29): the project skill `/skl_prj_checkAss`** (`.claude/skills/
skl_prj_checkAss/SKILL.md`) runs `assignments/autotest/assignments_autotest.py` (TA only). It asks which assignment;
only those with a reference solution (`REFERENCE` in the script: A1, and A2 since 2026-11-04) are checked, A3 gets "no
reference solution yet, nothing was tested". Per group: builds the project (Debug makefile, or CubeIDE headless: 18 s),
flashes it with STM32_Programmer_CLI, runs the testable items (A1: A.1 to A.3, B.1 to B.3, D.1 to D.3) by writing
`task`/`blinkMs` over SWD in HOTPLUG mode (the program keeps running), reads COM3 with pyserial (installed `--user`,
so run Python without `-I`). Then Claude reads the group's report PDF against the reference report and adds
`report: ...` remarks (`note` subcommand). Output: `assignments/autotest/assignments gradings.xlsx`, one sheet per
assignment; per item a comment column ("works good" or a VERY short issue) and a points column for Yohai, then
Others, then Total = 100 + all points. A1: C and E from the videos. About 3 min per group (D.1 alone 101 s). Logs in
`autotest/logs/assN/`, unpacked submissions in `autotest/work/assN/` (+ summary.json); workbook, logs and work are
git-ignored (student names). **Routine (Yohai, 2026-10-29):** Claude runs the check, then Yohai edits the sheet himself
(points and wording). So Claude never touches points, and never changes a comment already in the sheet without his
OK: the script writes only empty cells (or cells its own run wrote); any other change waits in
`work/assN/pending.json`, is shown to Yohai as now/new, and is written only with `apply` (refused: `drop`). Claude
also reviews the code (`note --kind code`): the board cannot see rules like "B.2/B.3 with bit operations only".
Sheet rows: "ref good solution" (`ass1/solution/fullSolution/ass1_sol` + `ass1_sol.pdf`) and "ref wrong solution"
(`ass1/solution/wrongSol/ass1_wrong` + `ass1_wrong.pdf`: A.1 starts all at 120, no binary in Part A, B.2/B.3 by an if
chain; see its README.txt). Result 2026-10-29: the board caught A.1 and A.2, the code review caught B.2/B.3 (the board
output is identical), the report review all three. Learned: `-w8` takes hex data only (a decimal "11" writes 0); PuTTY and a CubeIDE debug
session both block the test, close them first. Run on ass1_sol (row "yohai ackerman"): all 9 items "works good",
every number matched the TA report. Tested: nested-ZIP discovery, group name from `ass1_name1_name2_name3.zip`, the
`note` command; NOT yet tested on a faulty program, nor on a real student report. The test cannot see the IO-board
LEDs (B.2, B.3, D.3 checked in the code). Each run flashes a board: Yohai's OK.

**Measured on the board (2026-10-29), now in ass1_sol.tex:** C.2 makes 9,679 loop passes/s (one button read
103 us, not 110); A.1 lines 510 ms apart (500 + 1 tick + 7.4 ms UART); B.2 steps 1,003 ms; the PCA9538A raw byte at
rest is `0b00001111` (bits 4 to 7 read 0); the instructions' C.1 example was changed to `raw=0b00001110` (Yohai).

**Schedule as of 2026-10-17 (Yohai):** A1 out session 2, due session 5. A2 out session 7 (15.12),
due session 10 (5.1.2027) (both moved one week later). **"Last material included" rule:** an
assignment may require material taught up to and including the lecture one week after it is out
(A1: session 3, A2: session 8), nothing later. Shown in bold in the plan's milestone column.

**Work order and roles (agreed 2026-10-08):** the first focus was **writing Assignment 1** (done; A2 and A3 drafted by 2026-10-19).
Bucher derives the first lectures' content from it. After that: A2 and A3; Bucher writes the
lectures, Yohai the tutorials and (probably) the quizzes.

The working plan is `MP2026_plan.tex`
(13 sessions, lecture n ↔ tutorial n; the first tutorial is not one week after the first lecture, so "one week after the lecture" was removed from the table header, Yohai 2026-10-26). **It is the reference for which tutorial matches which lesson.** Lectures are on
**Tuesdays, from 3.11.2026** (moved from Mondays by Yohai, 2026-10-26): 12 lecture+tutorial sessions → 19.1.2027, plus session 13 (26.1.2027),
a lab meeting for open questions. Checked against the Technion calendar. A3 is out 12.1.2027 and due
**3.3.2027**.

**The course has started (tutorial 1 given, 2026-11-03). Lectures come from Moodle, not from Bucher (Yohai,
2026-11-03):** Bucher can't share his lectures in advance, so Yohai downloads them from the course website while they
are available, into `<OneDrive>\courses\teaching\MP\2026\lectures from moodle\lecture N\` (on this PC
`C:\Users\yohai\OneDrive - Technion\...`; the OneDrive part of the path may differ on the other PC). **The plan's
"Lecture" column is filled from there:** for each lecture N, if its folder has presentations, read them and write the
topics only, briefly; otherwise write "no lecture yet". Re-check the folder at every session start and update the
column. The planned lecture topics (Bucher's syllabus, my drafts) were replaced on 2026-11-03; they are in git history
(commit d85ddaf and earlier). State 2026-11-03 (evening): `lecture 1\` has 3 PDFs (intro + numbers/overflow/bitwise,
first project LED/printf, stack/buses/decoding/flip-flops/counters) and the example project zip; row 1 is filled.
**Lecture 2 (2026-11-09, 8 PDFs L2_1..L2_8 + `PA7_Debounce_Demo.zip`):** STM32 inside, UART/printf, RGB LEDs, IO
board and demo program, variables in C, four triggers, debouncing, live PA7 demo. Row 2 filled; the merged red cell of
rows 2 and 3 was split: row 3 now holds only what is still needed for A1 and Quiz 1 (bit idioms on registers;
HAL_Delay vs HAL_GetTick; truth tables, state diagrams). Lecture 2 announced for lesson 3: bits, where variables live,
enum and struct, EXTI, counters, HAL_Delay. When lecture 3 arrives, check it against row 3's list. **UART timing for
A1 D.1 is taught** (lecture 1 and L2_2: 10 bits per character, 86.8 us at 115200 baud, "a 40-character line costs
3.5 ms"), so no extra tutorial slide is needed. The folder is Yohai's own (not Bucher's), but treat it as
read-only: only read it.

**Quiz 1 (written 2026-11-03):** `\subsection{Quiz 1}` right after A1 in the plan: 7 multiple-choice questions (4
options, the correct one marked green, full solutions) and one open coding question (`blinkAndCount`: non-blocking
LD2 toggle with HAL_GetTick + XOR, edge-based count of button 0) with a solution and grading points. It has the
answers, so it is internal: the A1 export stops at the next `\subsection`, so `ass1_instructions.pdf` does not include
it, but the full `export/MP2026_plan.pdf`/`.docx` does. Answers are kept in `\begin{solution}...\end{solution}`
and `\correct`; the open question has `\answerspace{14cm}`. **The export makes the student versions** (Yohai,
2026-11-03): `export/quizzes/quizN_en.pdf` and `quizN_he.pdf`, both from the plan. **The plan is the only source
(Yohai: a separate quiz .tex is redundant and dangerous; the export never produces source files):** the Hebrew quiz
is a block of `%HE ` comment lines at the end of the Quiz subsection (pdflatex and the Word copy ignore them); the
export strips the prefix and compiles it with lualatex, with `HEBREW_PREAMBLE` from export_plan.py (babel bidi, font
David; `\en{}`/`\code{}` for English runs, `codeblock` around listings, longer formulas in `\mbox`). Hebrew wording,
professional terms and all code in English. The Hebrew block is a hand-kept mirror of the English quiz: change both
together. The open question gives the code of part (2) (button counting); students write the variables, the loop and
part (1), the non-blocking LD2 toggle. The quiz starts with instructions (no calculator) and a table of 2^0..2^7.
Section 3 of the plan is now "Assignments and quizzes".

**Assignment emphasis (Yohai, 2026-10-02):** start from the 2025 cart assignments, but shift the
focus to **coding, hardware and timing issues**. Kinematics and trajectory work is "less
interesting", so give it less weight. **A2 drafted 2026-10-17** from Yohai's outline: A PWM in three ways
(HAL_GetTick 1 ms software PWM as a demo of its limit; software PWM in a 10 kHz TIM7 interrupt and its
CPU cost; hardware PWM: TIM2 CCR and the PCA9634's own PWM), B polling vs. interrupts (IR + ultrasonic stop a
wheel), C own I2C driver, D encoder resolution (TI1 vs. TI1 and TI2) and wrap-safe speed, E step
response to MATLAB. PI control is no longer in A2. **A3 drafted 2026-10-19** from Yohai's outline: A PI speed
controller from the A2 Part E model (design, 100 Hz C code with anti-windup, air vs. floor); B obstacle
course (stop as close as possible, bypass right, left, right and return to the line, BOTH sensors with
the strategy explained, a state machine, encoder distances); C bonus: pivot and return to the start in
straight lines. Tutorial changes the same day: 8 = encoder configurations, 9 = control workflow (was
8), 10 = kinematics (straight lines) and state machine; the UART parser and MATLAB GUI tutorial
was dropped.
**A2 rewritten in the A1 spirit (Yohai, 2026-10-29):** no "Window" line; one program chosen by
`volatile uint16_t task` (three digits: part, item, sub-item; an item without sub-items ends in 0, report-only items
such as A.1.3 and A.4 have no task): 111/112, 121 to 124, 131/132, 210, 221 to 223, 231 to 233, 310 to 350, 411/412,
420, 511 to 513. Mandatory tagged lines (`A1.1:`, `B2.2:` ...); Part A globals `pwmN`/`pwmM`. Videos `V_A` (Part A,
with sound) and `V_B` (B.2.1 and B.3.1); the rest is meant for the automatic test (never written in the instructions).
Interrupts on only in the tasks that use them: PA7's EXTI disabled once in USER CODE 2 and enabled only in B.3 (that
is why B.2 needed `HAL_NVIC_DisableIRQ`: CubeMX enables EXTI9_5, and in one program the B.3 callback would also run in
B.2); `sonarStop()` added to the provided sonar.c (starting now resets the counts). D.1 switches the encoder mode at
run time (`TIM1->SMCR` SMS bits), so one `.ioc` (TI1) serves both settings, and A3 can keep it. D.2 starts TIM1 at
65,000 so it wraps soon. E.1 = steps 30/50/70 % recorded into MATLAB data files (how: shown in class); E.2 = fits,
overlaid plot; no E.3. A.2.3 at 17 kHz: the TIM7 callback itself must stop when `task` changes (the main loop may
starve). Red note in Part C: Yohai thinks Part C should be deleted (students would ignore the io_board.c they use).
**A2 solution run on the cart (2026-10-29, Yohai's OK; every task over SWD, no hand):** all tasks work. Measured:
A.1 904k passes/s; TIM7 at 10 kHz costs about 13 % of the CPU (1.85M vs 2.12M passes/s), so **t_ISR is about 13 µs in
the Debug build, not the 1 µs of tutorial 4's examples** (item 8 below; no scope, Yohai); at 1.7 MHz the main loop
is fully starved (nothing printed for 3 s); A.2.4 75 ms per 1000 LED writes; dead zone 15 % (wheel starts), 3,750
counts/s at 100 %; **wheel 1 forward counts DOWN**; B.2.2 readings=0 missed=64, B.3.2 103/0, loop period 156 ms both;
**C.1 finds 7 addresses** (0x03 = PCA9634 software reset, 0x70 = All Call of the uninitialised PCA9685); C.3 605 / 233 ms;
E steps 10.5 / 18.5 / 26.8 counts per 10 ms at 30/50/70 %, tau about 0.1 s, line through them gives u0 about 4 %
(below the 15 % start: stiction). **D.2 as first written (start 65,000, 5 s each way) never wraps on this cart** (forward
counts down, away from 65,535). **Changed 2026-11-04 (Yohai):** start at 0 and turn in one direction until two wraps,
plus 1 s, whatever the cart; solution changed the same way (5 min safety stop). **Every "on the cart" check is a red
flag in the plan** ("Yohai, on the cart: ...", Yohai 2026-11-04), so it is not forgotten near the cart; after the
session below only V_A/V_B trial videos remained (all closed on 2026-11-05: see below).

**Cart session 2026-11-04 (Yohai at the cart, Claude over SWD; COM6 on this PC; the Nucleo is powered by the cart
battery through E5V, so with the battery off the ST-LINK sees 0.2 V).** A1: automatic test all "works good"; C.1 to
C.5 and E with presses (C.2 22,625 at 9,615 passes/s, C.3 23, C.4 20, C.5 0 debounced / 13 edge, E all cases right,
brightening 2.07 s). **The IR sensor of this cart is HIGH for an obstacle** (hardware test assumed LOW): Yohai decided
the students find the level themselves (A1 Part E opens with it, `#define IR_OBSTACLE_LEVEL`, A2 B uses it; red flag:
check the other carts); all three solution projects use the define (SET). **Buttons seen from the front: 3, 2, 1, 0
left to right** (from the other side it is reversed, which confused us). In C.5 a quick press of button 1 is missed
(150 ms loop): it must be held. A2: **wheel/encoder mapping and directions are per cart and the students' job** (Yohai):
the configuration section now says "Check your cart" (shared lab carts). **No motor enable pin** (PC3 is not even
configured). D.1 rewritten (Yohai): D.1.1 by hand, one revolution at a time three times (long hand runs miscount);
D.1.2 with the motor, 50 %, 2 s per mode alternating TI1/TI12/TI1/TI12 (`D1.2: mode=TI1 counts=<n>`). Measured:
**660 counts/rev in TI1** (661, 663), factor 2.00 to 2.01, so 1,320 in TI12 (0.55 and 0.27 deg; 660 = 11 lines x 30:1
x 2, inferred). B.1 rewritten short ("an engineering approach is expected"); measured 5/10/15 cm: 101.9 counts/mm,
offset -6 mm (334 m/s), reliable from about 30 mm. Test 1 needs passes about 3 s apart (1 s restart rule; added to the
instructions): B.2.1 6 stops, B.3.1 9. B.2.2 0/64, B.3.2 102/0, periods 155/156 ms. C.4 all buttons right, **PCA9538A
fitted**. **Servo on PCA9685 channel 0** (now in C.5). D.2 new version: wraps at 10 ms and 28.6 s, 3.5 rev/s at 50 %.
E.1: -14.1/-21.6/-29.5 rad/s, tau about 0.1 s; K no longer grows with u (battery-dependent). Constants now in the
solution: COUNTS_PER_REV 660, RAW_PER_MM_X10 1019, SERVO_CHANNEL 0 (confirmed), IR_OBSTACLE_LEVEL SET. Both TA reports
updated. Log: `assignments/autotest/logs/ass2/cart_session_2026-11-04.log` (A1 and A2; git-ignored). The test tools
(COM logger, SWD helper) were scratch scripts, not kept. Also on 2026-11-04: the wrong A1 solution passed the
automatic test as designed (A.1 and A.2 caught); the E.1 MATLAB recording script was run for real (R2025b; it needed
`s.Timeout` + an empty-line check, else `readline` loops forever) and E.2 fitted with `fminsearch`: K -0.445/-0.420/
-0.416 rad/s per %, tau 0.123/0.094/0.089 s, rms about half a count step; figure `ass2/solution/fullSolution/
e2_fit.png` in the TA report. (The eyes-and-ears checks and the V_A/V_B trial videos left open here were closed on 2026-11-05.) **A2 automatic test added (2026-11-04)** to `assignments_autotest.py`
(`Tester2`, `test_ass2`, `task` written with `-w16`, `REFERENCE[2]`): A.1 to A.3, B.1, B tests 2 and 3, C.1 to C.5,
D.1.2, D.2, E.1 (A.4/E.2 report, B test 1 and D.1.1 hand). ass2_sol: all "works good" (row "ref good solution",
sheet "Assignment 2", about 4 min). `assignments/ass2/solution/wrongSol/ass2_wrong` (4 planted errors, README.txt):
all caught. skl_prj_checkAss updated for A2.
**2026-11-05 (Yohai): privacy and one assignment per check.** Course policies: the ZIP is `ass#_group#.zip` and the
title page has only the group number; no names, IDs or emails anywhere (Yohai keeps the key; Claude never sees it). The
script reads `groupK` from the ZIP name; every sheet starts with "ref good solution", "ref wrong solution", group1 to
group10 (`FIXED_ROWS`, added if missing, with the Total formula); it backs the workbook up to
`work/assN/gradings_backup_<date>.xlsx` before each test. skl_prj_checkAss checks ONE assignment per run (argument
`/skl_prj_checkAss 1` or `2`), and first tells Yohai whether the workbook exists, what it holds and how nothing is lost,
then waits for his OK. A1/A2: "decimal unless specifically required otherwise"; A1 A.2 line is now
`A2: hex=0x7F uint8_t dec=127 bin=0b01111111 raw=0x7F` (the literal was always equal to hex=; raw stays, it shows the
sign extension), in the plan, both A1 solutions, both reports and the test. The IR "check the other carts" flag removed.
Log: `assignments/autotest/logs/ass2/`.
**A2 shortened and checked on the cart (2026-11-05/06, Yohai).** The instructions from A1 to A3 should be less detailed
(the loss of automatic testing is accepted); A2 was edited part by part with Yohai's approval. Task numbers now: 111/112,
121 to 124, 131/132, 210, 221/222 (B.2 IR, polling/interrupts), 231/232 (B.3 ultrasonic), 310 to 350, 411/412 (D.1.1/
D.1.2 by hand, TI1 / TI1 and TI2), 413 (D.1.3: 8 s at 50 % per mode, `D1.3: mode= counts=`), 420, 510 (E.1, the four
wheels, own design), 521 to 524 (E.2 step of each wheel, `E2.k: t= w=`); E.3 = the fit in physical units (u = duty x
12 V, rad/s), a new "E.3" column pair in the workbook. B: wheel 1 at 40 %, status line only on change, HAL_Delay(150)
every pass, **B1 ends every B task** (the test presses it over SWD: PC13 ODR bit set, MODER to output 0.5 s); **one
sensor per test** (Yohai, 2026-11-05): B.2 stops only for the IR sensor, B.3 only for the sonar. B.2 asks for **very
quick** flicks. Tutorial flags added: 4 (counting passes, PSC/ARR at run time), 5 (sonar with polling and interrupts,
IR EXTI demo), 8 (encoder mode at run time): content still to add to the slides. Measured (B.2 on the first cart, the
rest on a second cart): B.2 polling 9 of 10 quick flicks, interrupts 10 of 10; B.3.1 readings=0 missed=139 (polling
can never complete a reading: two edges about 1 ms apart, one look every 152 ms; the hand speed does not matter),
B.3.2 244 readings, 10 of 10 stops; D.1.2 1,320.2 counts/rev; D.1.3 -18,654 / -37,815 (factor 2.03, the speed drifted
1.4 %; the int32 sum is needed); D.2 wraps at 10 ms and 14.08 s, 22.4 rad/s; E.1 each motor moves only its own
encoder; **on the second cart motor 4 runs forward with DIR RESET** (the configuration section says SET; it already
says "check your cart"), signs -1, +1, -1, +1 in the solution; E.2 K 3.58/3.43/3.22/3.12 rad/s per V, tau 0.09 to 0.13 s
(figure `e3_wheels.png` in the TA report). Eyes and ears (Yohai): LD2 steady from N = 20 (50 Hz), the border at 30;
the wheel smooth and acceptable at 100 Hz and 1 kHz (the gearbox makes most of the noise), still turning normally at
1.7 MHz (the failure shows in the passes: 1 in 23 s), a little quieter at 17 kHz; C.2 colours exactly as sent (larger
= brighter). So the A hint now says a motor PWM in the audible range "may be heard", and the TA report accepts an
honest description. Automatic test of the new version: ref good all "works good", ref wrong all 4 planted errors
caught (A.3.1, B.3.2 readings=0, C.5, D.1.3 factor 1.00). **Yohai: no more cart re-tests just because carts differ**:
we check the code and the coherence of the instructions, not a specific cart. All "Yohai, on the cart" flags are gone;
Yohai will not film trial videos.
**A3 rewritten (2026-11-07, Yohai):** wheels on the ground, so no Live Expressions there: `task` 120 (A.2, air) and
220 (B.2 sensor measurements) with the cable; IO-board buttons for the floor: 0 = A.3, 1 = obstacle course (5 s
delay), 2 = send the data recorded in RAM (cable plugged back in, MATLAB recording), 3 = emergency stop (read on every
pass, ends the program until RESET, but button 2 still sends the data). All four encoders TI1 and TI2 in ass3.ioc.
Videos V_A (A.3) and V_B (whole course + one emergency stop). Lines `A2:`/`A3:` (t r w1..w4 u1..u4), `B3:` (t c1..c4).
A3 is meant to be **open-minded** (many solutions): the instructions only say to use the sonar with its servo to choose
the side; obstacles have no known size. **Logic wanted in the A3 reference solution (Yohai):** at an obstacle, scan
with the servo to decide which side is better; if at the gimbal limit the obstacle is still seen, move the cart a bit
sideways to each side, scan again, and only then decide left or right. **The course:** two cases (obstacle positions
and sizes) published later, so the algorithm is not tailored; one video per case (V_B1, V_B2).
**USB re-plug tested on the cart (2026-11-07, socket supply, temporary scratch program):** unplugging leaves the program
running (LD2 blinks), but **plugging the ST-LINK USB back in resets the board** (reset cause PINRSTF, `up` restarted).
RAM in a `.noinit` linker section (NOLOAD, after `.bss`) survives that reset (a counter went on 92 to 120). So A3 tells
students to add the `.noinit` section to `STM32G474RETX_FLASH.ld` and keep the recorded arrays, sample count and a magic
"valid" word there; button 2 sends them after the restart. Red flag left: check the same with the cart on its battery.
**Wheel positions (2026-11-07, Yohai at the cart, ass2_sol task 510, encoder counters read over SWD):** motor 1 = rear
right (TIM1), 2 = rear left (TIM8), 3 = front left (TIM3), 4 = front right (TIM4); each motor moved only its own timer, as
in the A2 table (that is car 12's wiring: see "motor-to-wheel map" below; the positions were later removed from the
A2 table). 1 s at 30 % gave 1,664 / 742 / 1,548 / 727 counts: motors 2 and 4
about half: **slower motors, not resolution** (by hand all four give 1,320 per turn, measured the same day).
**Car-specific parameters (Yohai, 2026-11-07):** the A3 solution keeps them per car in
`assignments/ass3/solution/fullSolution/ass3_sol/Core/Src/car_params.c` (+ `Core/Inc/car_params.h`): `carParamsGet(id)`
returns motorDir, encDir (+1/-1), countsPerRev, K, tau, Kp, Ki, maxVelCmdNoSat (4 each) and the design goals
(designOvershoot 1, designTauC 0.032 s; Yohai first asked for overshoot 1.05 and settling 0.1 s, then chose the
closed-loop time constant as the clearer goal, also in A3 A.1: "no overshoot and tau_c, PI zero on the motor pole,
tau_c not below about 30 ms at 100 Hz"). The A3 solution will start with a variable that selects the car. **Car 12** measured 2026-11-07
(wheels in the air, socket supply 12 V): motorDir -1/+1/+1/-1, encDir +1/-1/-1/+1, 1,320 counts/rev, K 2.76/1.57/2.49/
2.23 rad/s per V, tau 0.082/0.091/0.087/0.104 s (50 % step). Step data in `fullSolution/car_data/`.
**PI design (redone the same day, Yohai's point: a linear loop does not care about the step size):** the first gains
were searched in a simulation WITH the 12 V limit at a 10 rad/s step, so they met the goals only by saturating (linear:
overshoot 1.11 to 1.52). Now designed on the linear loop (PI every 10 ms, speed over the last 10 ms, ZOH, no limit):
PI zero on the motor pole, Ki = Kp/tau, Kp = tau/(K tc), the same tc = 0.032 s for all four wheels (they respond alike),
the slowest tc that meets the goals with a 20 % margin on settling. Car 12: Kp 0.921/1.804/1.087/1.451, Ki
11.31/19.93/12.57/14.02; simulated overshoot 1.000, settling 0.079 to 0.082 s. (With the 10 ms loop, tc = 0.025 gives
0.040 s, not 4 tc.) `maxVelCmdNoSat` (Yohai): the largest step from rest whose PI output stays within 90 % duty
(10.8 V), transient included; the peak is the first sample, Kp + Ki x 0.01 per rad/s: 10.44 / 5.39 / 8.90 / 6.79 rad/s.
**Skill `/skl_prj_getCarParams X` (2026-11-07):** `.claude/skills/skl_prj_getCarParams/` (SKILL.md + `car_params_tool.py`:
prepare, spin N, cnt, steps, write) runs the three steps interactively (directions, 3 hand turns, 50 % steps) and
writes car X's entry into car_params.c; the step build is a CLEAN build of a patched ass2_sol copy (a stale build ran
wheels backward once). **A3 reference solution (2026-11-07, NOT yet run on the cart):**
`assignments/ass3/solution/fullSolution/ass3_sol/` (copy of ass2_sol: `ass3_sol.ioc` with all four encoders TI1 and
TI2, `.noinit` in STM32G474RETX_FLASH.ld, `carId = 12` in main.c USER CODE PV). Files: ass3_tasks.c (dispatcher,
buttons read by its own I2C call so 3 bus errors in a row = emergency stop, since readButtonsRaw returns 0xFF on
error; callbacks), ass3_wheels.c (PI at 100 Hz in TIM6 with conditional anti-windup, motorDir/encDir from the car,
Mecanum motions, odometry), ass3_io.c (IR, sonar mm with median of 3, servo), ass3_record.c (`.noinit`: A 200 x 9
floats, B 12,000 x 4 int8 counts per 10 ms + state = 120 s; 67 KB at 0x200005FC, below the CCM alias), partA.c (120
and button 0), partB.c (course state machine: FORWARD, SCAN 13 directions, PROBE = Yohai's sideways-and-rescan logic,
SIDESTEP, PASS with the sonar turned to the obstacle, RETURN; task 220). Speed: every wheel at 0.9 x the smallest
maxVelCmdNoSat (car 12: 4.85 rad/s); motions change only through a stop, so no step saturates. **Cart sizes (Yohai,
2026-11-07):** wheel radius 30 mm; wheel centres 165 mm front to rear, 210 mm left to right; stop distance STOP_MM 80.
The side is never fixed in advance: the course takes the side that needs the shorter sideways move. STRAFE_SIGN is
only the Mecanum kinematics (+1: wheels 2 and 4 forward, 1 and 3 back moves the cart left; usual X rollers), to
confirm once on the floor (red flag in the A3 section of the plan). Overall size: 240 mm wide (outside of the
wheels), 300 mm long (sensors to the rear): CAR_HALF_WIDTH_MM 120, CAR_LENGTH_MM 300.
**IR level and servo zero are car parameters** (car_params.c: irObstacleLevel, servoZeroUs), measured by step 5 of
the skill with TA task 900 of ass3_sol (the servo follows `calServoUs`, the IR pin printed). **Servo (Yohai,
2026-11-09): only the zero is car-specific** (the pulse at which the sonar looks straight ahead, found by stepping
the servo 50 us, then 10 us, while Yohai watches; 1500 us is only the servo's own mid-travel, the holder's mounting
shifts it). The scale is shared, `SERVO_DEG_PER_US` = -124/1200 deg/us in ass3.h (+ left: a longer pulse turns
right), from car 12's 2026-11-07 readings (900 us 62 deg left, 1500 us 5 deg left, 2100 us 62 deg right). The scan
is symmetric about the zero within 900 to 2100 us. Car 12: IR 1 = obstacle; servoZeroUs 1545 derived from the old
three-point readings, **to redo with the zero step** (red flag in A3). `carParamsGet` refuses an entry with Kp = 0 (partial) or a
wheelMotor that is not 1..4 each once (the A3 solution halts before driving it).
**Motor-to-wheel map is car-specific (Yohai, 2026-11-09):** car 11 is wired with motor 3 = front RIGHT and motor 4 =
front LEFT (car 12: 3 front left, 4 front right). car_params.c now has `wheelMotor[4]` (the motor at rear right, rear
left, front left, front right; car 12 {1,2,3,4}, car 11 {1,2,4,3}); all other arrays are per MOTOR, and only the
Mecanum sideways pattern and the y odometry use positions. The A2 table no longer gives positions; red flag in A3:
decide whether students must find their own map. The skill's step 1 asks which wheel each motor turned (`setmap`),
and step 2 goes by position: rear right, front right, then the left side (Yohai's order).
**Car 11, measured in full 2026-11-09:** motors 1..4 at RR, RL, FR, FL; motorDir {+1,-1,-1,-1}, encDir {-1,+1,-1,+1}
(the two right motors have opposite motor signs but equal encoder signs: probably one motor's leads swapped; the step
speeds were all positive, so the signs are right); countsPerRev 1320 (by hand 1313.7 / 1316.0 / 1317.3 / 1321.0);
K 3.48 / 3.41 / 3.23 / 3.22 rad/s/V, tau 0.089 / 0.102 / 0.079 / 0.103 s (fit rms 0.39 to 0.67): four well matched
motors, much stronger than car 12's; Kp 0.80 / 0.93 / 0.77 / 1.00, Ki 9.0 / 9.2 / 9.7 / 9.7; maxVelCmdNoSat 12.14 /
10.55 / 12.52 / 9.83 (motor 4 limits: the A3 speed is 0.9 x 9.83 = 8.85 rad/s); IR 1 = obstacle; servoZeroUs 1575
(sonar holder assembled that day). The `.ioc` key `TIMx.EncoderMode=TIM_ENCODERMODE_TI12` was written by
hand (main.c has TI12 too); not yet opened in CubeMX. Builds with 0 warnings (-Wall -Wextra).
**A2 reference solution (2026-10-29):** `assignments/ass2/solution/fullSolution/ass2_sol/`
(CubeIDE project; `ass2_sol.ioc` = the TA's ass2.ioc, built from ass1.ioc + the configuration section and checked in
the generated code; builds with 0 warnings) and `ass2_sol.tex` (TA report; the PDF is git-ignored). The constants were measured
on the cart on 2026-11-04 (above); since 2026-11-06 no "[to measure]" is left in it.
C.1 finds 7 addresses (0x03 and 0x70 too), as written in the report. Learned: **CubeMX on this PC is now 6.18.1**, and a
6.18.0 `.ioc` makes the headless CubeMX hang at `config load` (a hidden migration dialog); write `MxCube.Version=6.18.1`
and `MxDb.Version=DB.6.0.181` into the file first. Students opening ass1.ioc (6.18.0) in a newer CubeMX get a
"migrate?" question in the GUI.

### 4.3 Meeting plan (syllabus)

1. Intro; counters, flip-flops, edge/level triggering; integers (hex, two's complement, overflow);
   basic IO. *Tutorial:* CubeIDE, first project from the course `.ioc`, blink LD2. **Lab 1 out.**
2. Python → C (1): types, memory, functions, loops.
3. Python → C (2): pointers, structs, memory map, GPIO registers, bit ops, volatile; UART/PuTTY.
4. Timers: prescaler/period, polling vs interrupts, period interrupts; state machines.
   **Lab 1 due, Lab 2 out.**
5. PWM & servo; SR04 input capture; optical sensor with EXTI.
6. I2C bus; HAL I2C; drivers for PCA9634/PCA9538A/PCA9685. **Lab 2 due (end wk 7).**
7. DC motors: H-bridge, encoders, PI @100 Hz, anti-windup, CubeMonitor. **Lab 3 out.**
8. Mecanum kinematics, odometry, trajectories (trapezoidal profiles, waypoints).
9. MATLAB I: UART protocol, serialport, App Designer GUI. **Lab 3 due, Lab 4 out.**
10. MATLAB II: Simulink, discrete controllers, C code generation.
11. Integration: scanned sonar, streaming, filtering (median/IIR), polar map.
12. Case studies, advanced topics, wrap-up. **Lab 4 due.**

### 4.4 Divergences to keep in mind

Syllabus consistency is Bucher's responsibility, not a focus of ours (Yohai, 2026-10-02). Note a
divergence when it affects an assignment, tutorial or quiz. Otherwise don't chase it.

- **Two syllabi exist.** `syllabus_G474_cart_2026.docx` matches `course_plan_2026_deck_v2.pptx`
  and is treated as **current**. `syllabus_I2C_IOboard_2026` is an earlier variant (different lab
  split: cart basics / Mecanum / IO board / sonar). Confirm with Yohai before relying on either one
  for a fine detail.
- **The lessons Bucher has written don't follow the syllabus numbering.** For example, Lesson 3 is
  "LEDs, buttons, Catch-the-Dot game" (the I2C IO board used as a black box through
  `io_board.h`), whereas syllabus meeting 3 is Python → C part 2. There is also a **Lesson 0**
  (inside the STM32: memory map, pipeline, toolchain, linker, reset) outside the 12-meeting plan.
  Treat the lesson files as the truth for what is actually taught, and the syllabus as intent.
- The I2C chips are a **black box until meeting 6** (Lab 1 uses the provided LED helper library).
  Lab 2 then has students write their own drivers. Assignments and quizzes must respect that order.

### 4.5 Bucher's shared folder — inventory

```
lessons/
  course_plan_2026_deck(_v2).pptx       one-page course plan (v2 = current)
  syllabus_G474_cart_2026.docx          current syllabus
  syllabus_I2C_IOboard_2026.docx/.pdf   earlier variant
  I2C_Explained_Teaching_Deck.pptx/.pdf
  Lesson_0/ lesson00_inside_the_STM32.pptx
  Lesson_1/ lesson01_intro_logic_numbers_C.pptx, stack_buses_decoding_flipflops_counters.pptx,
            tutorial_first_stm32_app.pptx
  Lesson_2/ lesson02_python_to_C_STM32.pptx
  Lesson_3/ lesson03_LEDs_buttons_game.pptx, lesson3_program_manual.docx, technote_enum_in_C.docx,
            lesson3_cubeide_sources(.zip)  — io_board.c/.h, console, 12 examples, games, menu main.c
new_cart_test_code_with_doc/  NUCLEO_G474RE hardware test report + teaching deck, test code zip, video
NEW_Servo_scanner_Setup/      servo program explanation (docx/pdf/pptx)
cads/                         Creo CAD of the cart/IO board (+ MG996R servo datasheet)
new_pcb_demo.zip, test_pcb2a.zip
```

### 4.6a Last year's course folder (2025/26), also READ-ONLY

`C:\Users\yohai\OneDrive - Technion\Bucher Izhak's files - Shared_2025\` is the same rule as the
2026 folder: never write there. What's worth reusing:
- `assignments/Lab_1/`: the 2025 Lab 1 (F446 lab system: stepper, encoder in timer mode, port-D
  LEDs) and `Lab0_background_p1` (non-submitted prep). `assignments/Lab_2/part1` (100 Hz timer ISR,
  states IDLE/PWM/SQUARE/DRIVE, UART interpreter + MATLAB GUI) and `part2` (wheel model fit, PI
  design, kinematics, figure-8 trajectory, obstacle stop). These are the natural base for our
  A2/A3. `הנחיות להגשת דוח מעבדה.pdf` holds the report guidelines.
- `tutorials/2025/`: TA1 (intro/lab rules, by TA Rami Sabbagh), TA6 (MATLAB GUI + PWM/UART-interrupt
  project), TA7/TA8 (control workflow: fit → sisotool → c2d → C), TA9/TA10 (G474 projects with
  MATLAB), TA11 (control, plus a Wiggler Simulink tutorial that is retired with the Wiggler).
  `tutorials/Tutorial_1/number_representations_bit_operations.pptx` is a fit for A1.
- `lessons/`: only lesson 1 (intro, bits/numbers) and PI-control/DC-motor decks. Root: Mecanum
  trajectory/Frenet MATLAB scripts, Li-ion battery safety instructions (Hebrew).
- Student submissions (`הגשת מעבדה …`): don't read them unless they're needed for grading
  calibration.

### 4.6 This folder

```
MP.md                          this file
CLAUDE.md                      auto-loaded pointer to this file
MP2026_plan.tex                13-session plan (lecture ↔ tutorial, dates) + assignment proposals (A1 and A2 detailed, A3 drafted 2026-10-19)
                               (the .tex goes in git, not the PDF)
.claude/skills/skl_prj_exportplan/   project skill (2026-10-22): /skl_prj_exportplan runs export_plan.py, which
                               rebuilds export/ from scratch: MP2026_plan.pdf + .docx (tex2word on a copy in
                               export/_build, never the repo's .docx), coursePolicies.pdf, assN/assN_instructions.pdf
                               (red flags removed) + the top-level files of assignments/assN (never solution/), and
                               tutorials/Tutorial1.pdf, Tutorial2.pdf, ... (one per deck section, numbered from the
                               section name "Tutorial N: ...", via PowerPoint COM). PowerPoint must be saved and
                               closed (a recovered "[Autosaved]" file blocks it); export/_build is deleted on success
export/                        generated by the skill, git-ignored, local only. Never put hand-made files there
(MP2026_plan.docx is no longer in git (Yohai, 2026-10-22): it is generated, git-ignored; the Word copy of
 the plan is export/MP2026_plan.docx, made by /skl_prj_exportplan only when Yohai asks.)
(Folders reorganised by Yohai on 2026-10-22: ass1/ moved to assignments/ass1/, hardwaret_test/ to
 stmProjects/hardwaret_test/, general/ to others/; paths below are the new ones.)
assignments/ass2/              files handed to A2 students: sonar.h, sonar.c (2026-10-22), a RAW
                               HC-SR04 reading (echo width in TIM5 counts, no unit; students find
                               the factor to mm by experiment). Based on the hardware test's
                               capture callback (pin level tells rising from falling). Two modes
                               for Part B: sonarStartPolling + sonarPoll in the main loop (B.1;
                               detects overcapture, counts missed readings) and
                               sonarStartInterrupt + sonarCaptureEdge(htim) called from the
                               student's own HAL_TIM_IC_CaptureCallback (B.2). Compiles with 0
                               warnings (-Wall -Wextra) against hardwaret_test's headers; not
                               yet run on the board
assignments/ass1/              files handed to A1 students: ass1.ioc (the project configuration),
                               io_board.h, io_board.c (from Bucher's Lesson 3 package, TRIMMED for
                               A1 on 2026-10-22: removed readButtonsPressed, BTN_MASK, setLedMask,
                               printByte, printButtons, which solve parts of A1; kept ioBoardInit,
                               setLedChannel, setAllLedChannels, setRGB, setRGB_led, allRgbOff,
                               gamma2, readButtonsRaw, checkI2C; compiles with 0 warnings).
                               The ultrasonic sensor is
                               not part of A1 (Yohai, 2026-10-11); A1 uses only the IR sensor
assignments/ass1/solution/     TA-ONLY, never handed to students. checkBounce/ = CubeIDE project
                               (from hardwaret_test) that measures bounce on the I2C buttons,
                               B1 (EXTI + polled) and the IR sensor; driven via Live Expressions.
                               Results in checkBounce_instructions_filled.txt (2026-10-13):
                               B1 clean (RC filter on the Nucleo), I2C buttons bounce rarely
                               (0 to 4 short gaps per 20 presses, always one 407 us sample at
                               100 kHz), IR sensor chatters heavily near its switching distance.
                               Decided: C.1(a) stays on button 0; (b) and (c) use button 0 and
                               the IR sensor (not yet written into the plan).
                               checkBounce2/ = follow-up: press recount (raw presses/releases +
                               20 ms debounced count), IR polarity (LD2 mirrors PA7), I2C at
                               100 or 400 kHz switched at run time, edge-timing log dumped to
                               PuTTY. Yohai fills checkBounce2_instructions.txt (never run so far)
                               bounceCheck400/ (2026-10-22) = from checkBounce2, button 0 only, I2C
                               at 400 kHz by default (i2cFast = 0 gives 100 kHz for comparison),
                               plus loopDelayMs to show MISSING presses (quick taps vs a slow
                               loop) next to the UNWANTED ones (bounce); outputs samplePeriodUs and
                               b0MinHeldUs (shortest tap). Full build with the CubeIDE toolchain
                               outside the IDE: 0 warnings. Yohai fills
                               bounceCheck400_instructions.txt; then decide whether Part C can show
                               both effects (the red flag on 400 kHz in Part C waits for this).
                               RESULTS (bounceCheck400_instructions_filled.txt, 2026-10-22): 400 kHz
                               works (0 errors, 9058 reads/s = 110 us). Natural presses at 400 kHz:
                               23 raw / 20 debounced (3 extra, 6 short gaps); light presses 26 / 20;
                               at 100 kHz 20 / 20, 0 short gaps. Bounce pulses are shorter than one
                               read (min gap always 1 sample: 111 us here, 407 us at 100 kHz), so
                               the read rate decides what is seen. Quick taps last 110 to 130 ms;
                               loop delay 150 ms: 11/20 raw, 0 debounced; 300 ms: 6/20, 0
                               debounced (debounce + slow loop counts nothing). Test A rounds 2 and
                               3 identical to round 1: ask Yohai if copied. Proposal: Part C keeps
                               the button, ass1.ioc I2C at 400 kHz (check io_board LEDs at 400 kHz;
                               A2 says 100 kHz), C.1(b) must update LEDs/print only on change, new
                               (d) missing events with HAL_Delay(200). DONE 2026-10-22: Part C is
                               now C.1 level, C.2 edge (+ one round of light presses), C.3
                               debounced 20 ms, C.4 missing presses with HAL_Delay(150) (debounced
                               and edge); LEDs/print only on change. Button only: Yohai decided
                               to keep the IR sensor out of Part C
                               fullSolution/ (2026-10-26) = the full A1 reference solution.
                               ass1_sol/ = CubeIDE project (copy of tut1Proj, whose .ioc equals
                               ass1.ioc; renamed ass1_sol), students' io_board.c/h unchanged, one
                               file per part (partA.c to partE.c), bits.c (B.1), ass1_common.c,
                               ass1_tasks.c (dispatcher). Everything runs in one program: the global
                               `task` chosen in Live Expressions (11 = A.1 ... 51 = E; 35/36 = C.5
                               debounced/edge, 43/44 = D.3 HAL_Delay/HAL_GetTick), printing the
                               mandatory tagged lines (2026-10-28). ass1_sol.tex =
                               TA report: how to run every task in Live Expressions, answers,
                               expected lines and numbers, grading notes. Built with the CubeIDE
                               toolchain, 0 warnings (2026-10-28); NOT yet run on the board.
                               Choices in it: E brightens again if the condition
                               returns while dimming; D.3 deadlines kept in us so 1.3 Hz gives
                               7692 ms, not 7700
tutorials/MP_tutorials_2026.pptx    all tutorials in one deck, one section per tutorial; the export
                               skill splits it into Tutorial1.pdf, Tutorial2.pdf, ... Yohai edits it in
                               PowerPoint himself, so slide numbers shift: refer to slides by TITLE.
                               Never regenerate it: add slides in place (python-pptx, see §5).
                               Content on 2026-10-26 (47 slides):
                               T1 (1 to 11): course policies; from a .ioc to a CubeIDE project; USER
                               CODE sections; main.c sections; PFP; .h/.c; archive/move; live example.
                               T2 (12 to 19): live example overflow; "Bit operations on a single bit"
                               (NOT/AND/OR/XOR truth tables); "The four bit idioms" (LD2 = PA5, symbol
                               names); live example Nucleo LED; live example printf to PuTTY (code /
                               where to put / comments, standard C vs STM32, __io_putchar called by
                               _write in syscalls.c); HAL_UART_Transmit taken apart (argument types,
                               return value, 87 us per character).
                               T3 (20 to 22): live examples with io_board (setRGB_led after ioBoardInit).
                               T4 (23 to 29): timer-to-callback chain; the ISR; three ways to make a PWM
                               (N, M, t_step, T_PWM); polling / interrupt / hardware timing slides
                               (working vs not-working: printf 1.9 ms; ISR entry 0.07 us, 1 kHz vs
                               20 kHz; hardware f_PWM · N <= f_clk, 17 kHz vs 100 kHz).
                               T5 (30): I2C, title only.
                               T6 (31 to 40): the .ioc in CubeMX (4 slides; I2C1 = Fast mode 400 kHz);
                               self-learning: fast vs slow outputs (I2C write about 0.08 ms at 400 kHz),
                               blocking (24 writes about 2 ms), non-blocking, three layers, super loop.
                               No SR04 slides yet. T7 to T12 and session 13: title slides only.
tutorials/tut1Files, tut2Files live-example CubeIDE projects (400 kHz); tut2Files/tut2ForMoodle.zip

others/rolling doc MP course.docx    Yohai's running list of understanding questions
stmProjects/hardwaret_test/    CubeIDE project (G474RE; align_servo.ioc; HardwareTest + servo builds)
                               + HowToUse.txt (how to run the test: states, debugger, PuTTY)
```

## 5. How we work

- **Be critical.** If Bucher's material, a syllabus detail or Yohai's plan has a gap or an error,
  say so plainly. Students will find it otherwise.
- **Verify technical claims** against the actual code/datasheet (STM32G474 RM0440, PCA9634/9538A/
  9685 datasheets, HAL sources), not memory. Mark unverified details 🔴.
- **Code for students** must build in STM32CubeIDE against the course `.ioc`, stay at the level of
  the course at that point (§4.4), and follow Bucher's existing style (`io_board.h` API, layered
  files, `USER CODE` markers).
- **Assignment work keeps three things apart:** the assignment sheet (student-facing), the
  reference solution (TA-only), and the grading rubric. The solution never goes anywhere a student
  could see it.
- **Never write a double hyphen (`--`)**, in any document, LaTeX included (Yohai's standing rule,
  2026-10-02). For ranges write "1 to 3" or a single hyphen, and use a colon or comma instead of a
  dash. The only exception is markdown table syntax (`|---|`), which the format requires.
- **`MP2026_plan.tex` is clean content.** No notes to self, rationale or source references; those
  go in §6 and §7 of this file. **The one exception: red `\flag{...}` notes for Bucher and Yohai**
  (Yohai, 2026-10-11), placed where an assignment item needs a joint decision, e.g. a provided
  function that already solves the item. They are deleted before anything is published to
  students. **Blue text** marks content Yohai considers less important (a candidate to shrink),
  e.g. the IO-board part of lecture 3, which may give way to timers and `HAL_GetTick`. Instructions never tell students "don't use function X"; flag it instead.
  **Yellow `\bnote{...}`** (2026-10-23) = Bucher's own remarks kept for discussion, e.g. on A1 Part C
  ("a bit tedious, shorter with more freedom"), for the Sunday meeting. Like the red flags, the export
  skill removes them from student PDFs.
- **Bucher's edits (2026-10-23, `02_assignments_to_moodle/ass1/ass1_instructions_ib.docx`):** his
  phrasing was taken as is (better English). Content decisions by Yohai: %b note added to A.1; B.1 =
  "functions (not macros), using bitwise operations" (his "Boolean" is the wrong word) and a clearer
  set/clear/toggle/test sequence; B.2 keeps 1 s steps and "forever"; the bit-7 rule and C.1 to C.4 labels
  kept; Part C kept with his remark in yellow; A.1 says "printf via UART (see tutorial 2)".
  Meeting with Bucher (25.10.2026, edits typed into `01_course plan/MP2026_plan_vs_Bucher.docx` on OneDrive, no
  tracked changes): taken back into the .tex on 2026-10-26 after a paragraph diff against the file brought to the
  meeting: "examples" typo; tutorial 3 + "using includes for external files"; quiz contents "Questions related to
  Assignment 1" / "Assignments 1 and 2"; A1 contents + "interacting with external events in real time";
  "fulfill"; B.1 split into 4 lines ("The functions will use bitwise operations", "affect"); B.3 "'Walking' pair
  … Repeat B.2 above, but now …"; Part E "stepwise". Not changes: Word turned equation minus signs into hyphens;
  the io_board yellow box was deleted (= keep "through").
  ⚠ His copy of `ass1.ioc` there was re-saved in CubeMX 6.18.1 as `ProjectName=mytest`: a student
  generating from it gets a project called "mytest". Tell him, or replace it with the repo's ass1.ioc
  (Yohai copies; the folder is read-only for Claude).
- **Every function students need but haven't learned yet is provided, and the instructions say so**
  (Yohai, 2026-10-11). Short code goes as text in the assignment's appendix, for copy-paste, unless
  a tutorial already teaches it (the `__io_putchar` printf redirect is taught in tutorial 2, so
  Appendix A1 was deleted on 2026-10-22). Long code goes as files in that assignment's folder
  (`assignments/ass1/`, `assignments/ass2/`, …), and the instructions name the files and where to
  copy them. Project setup from a `.ioc` is taught in tutorial 1, not repeated in the assignments.
- **Compile-check after every code edit** (verified working 2026-10-10 on `hardwaret_test`: 28
  files, 0 warnings, 2 s). CubeIDE 2.2.0 is at `C:\ST\STM32CubeIDE_2.2.0`. Put on PATH its
  `plugins\com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.14.3.rel1.win32_*\tools\bin`
  and `plugins\com.st.stm32cube.ide.mcu.externaltools.make.win32_*\tools\bin`, then run
  `make -j8 all` inside the project's `Debug\` folder. It uses CubeIDE's own generated makefile,
  so it builds what the IDE builds. Limit: that makefile lists the source files, so a **new** `.c`
  file is only picked up after CubeIDE regenerates it (Yohai builds once in the IDE). Building is
  fine; **flashing the board is not**, unless Yohai explicitly asks.
- **Deliverables for Bucher** are prepared here; Yohai moves them to the shared folder himself.
- **Word copies for Bucher** (only when Yohai asks): `/skl_prj_exportplan` writes `export/MP2026_plan.docx` with
  tex2Word (real tables, red/blue notes kept as the Word styles "Color red"/"Color blue"). It reports TABLES and
  LEAKS; both must be clean. Generated files are not kept in git.
- **Scripted edits of the .tex:** never put backslashes inside a bash heredoc (the shell tool mangles
  `\\`, which once spliced the plan at the wrong "Part B" and duplicated half of it, 2026-10-18).
  Write the script to a file and build backslashes with `chr(92)`. After any scripted splice, check
  that each heading appears once and diff against the last commit. The same happens in a Python heredoc, and in
  sed a `\l`/`\u` in the replacement is a case operator (a rename `\le{` to `\lv{` turned `\title` into `\titv`,
  2026-10-26): always use a script file.
- Planned documents (create when the task starts, not before): `assignments/` (unified assignment
  document + solutions + rubrics), `quiz/`, and a `PROGRESS.md` once there is ongoing
  task state to track.
- **Timing vocabulary** (Yohai, 2026-10-18): explain timing in time [s], not frequency. Terms:
  t_clk = 1/f_clk, t_tick = (PSC+1)·t_clk, T_period = (ARR+1)·t_tick, t_ISR (one pass through the
  interrupt), t_free = T_period − t_ISR. Rule: t_ISR < T_period, with a margin (rule of thumb
  about 10 %). The term **ISR** is used: taught in lecture 4 and on tutorial 4's timing slides.
  **PWM vocabulary** (tutorial 4's PWM slides and A2 Part A): a step counter runs 0 to N − 1;
  the output is on while it is below M; t_step = one step, T_PWM = N · t_step, f_PWM = 1 / T_PWM,
  duty = M / N. The three methods are named **polling** (`HAL_GetTick` in the main loop, not
  `TIM->CNT`), **interrupt** (TIM7 callback, t_step = T_period) and **hardware** (timer CCR,
  N = ARR + 1, M = CCR). "Tick" stays the timer count, t_tick.
- **Editing the tutorials deck:** insert slides in place with a python-pptx script kept in the
  scratchpad. Back up the deck first, and add each new slide to its tutorial's section. The deck
  must be closed in PowerPoint, or the save is refused. Check by rendering through PowerPoint COM
  (`Presentations.Open` on a copy, then `Slides(i).Export(...png)`). python-pptx names a new slide
  by the slide count, so add new slides **before** deleting any in the same script, or two parts
  get the same name. For COM rendering pass Windows paths (backslashes) and the slide index as
  `[int]`.

## 6. Open questions for Yohai

1. **Cart hardware, to check on the cart/PCB** (from the hardware-test project; kept out of the
   student-facing configuration sections until confirmed):
   - (Settled 2026-11-04, Yohai: the wheel mapping and directions are per cart, and the students check them;
     A2 "Check your cart". No motor enable pin: PC3 is not configured and the wheels turn. Servo on channel 0.)
   - Configured in the test `.ioc` but unused, and purpose unknown: ADC1_IN12 (PB1), ADC5_IN1 (PA8),
     TIM15 (50 Hz base, interrupt enabled), TIM20_CH3 PWM (PC8). Possibly leftovers (battery
     sense? old direct servo timer?). Left out of the student docs.
   - Free GPIO outputs in the test `.ioc`: PC3, PC4, PB5, PB6, PB7, PB14.
   - (Measured 2026-11-04: 660 counts per wheel revolution in TI1, 1,320 in TI12. The PCA9538A is fitted.)
   - `main.h` has a stale comment with TRIG/ECHO swapped (ECHO = PB2, TRIG = PC12). The `.ioc` and
     `main.c` (TRIG = PB2 TIM5_CH1 PWM, ECHO = PC12 TIM5_CH2 IC) are correct.
   - A2's 100 Hz sample interrupt: TIM6 (PSC 169, ARR 9999) chosen in the A2 configuration section,
     plus TIM7 (PSC 169, ARR 99, 10 kHz) for the software PWM of A.2,
     and PA7 becomes GPIO_EXTI7 (EXTI9_5). Checked: ass2_sol.ioc builds and runs with them.
2. **Do A1 students get Bucher's `io_board.h`/`.c`** (Lesson 3 package, `lessons/Lesson_3/
   lesson3_cubeide_sources/`)? His syllabus and Lesson 3 slides say so ("provided LED helper
   library", "black-box principle"), and A1 as written depends on it for the RGB LEDs and buttons,
   but Yohai and Bucher haven't agreed it. Yohai's decision (2026-10-22): give a trimmed copy
   (see §4.6); tell Bucher.
3. (Resolved 2026-10-17: A2 is now due 5.1.2027, one week before A3 goes out on 12.1.2027.)
4. (Decided 2026-11-04, Yohai: the servo-scanned sonar becomes a task in A3; red flag at the top of A3 in the
   plan. A2 keeps the sonar alone in Part B and the servo alone in C.5.) Part C stays as it is until Yohai
   discusses it with Bucher.
5. 🔴 **`io_board.c` writes PCA9634 MODE1 = 0x20, commented "auto-increment".** On the PCA9634 the
   MODE1 auto-increment bits are, I believe, read-only and set through the control byte. Check the
   datasheet before A2 C.3 (one transaction vs. many) is published; if confirmed, tell Bucher. (Red flag in A2 C.3
   since 2026-11-04.)
6. Agree with Bucher: Quiz 2 moved to lecture 9 and now covers sessions 4 to 8 (my assumption), and
   PI speed control is no longer in A2 (Yohai's outline, 2026-10-17); it is A3 Part A.
   Also: lectures 8 and 9 (PI control; UART and MATLAB GUI) no longer match tutorials 8 and 9
   (encoders; control workflow) since 2026-10-19, and no tutorial covers the UART parser or the
   MATLAB GUI any more. A3 flags: the obstacles and the course, and the bonus weight.
   Also changed by Yohai on 2026-10-19, to tell Bucher: A2/A3 weights 25/35 → 30/30; tutorial 5 is
   now I2C and the .ioc tutorial moved to tutorial 6 (with SR04 reading, then self-learning: blocking
   vs non-blocking). The "EXTI interrupt with a counter" left tutorial 6, so no tutorial covers EXTI
   before A2 Part B.
7. 🔴 **The motor winding's electrical time constant L/R** (red flag in A2 A.2.2, for Bucher): we
   assume about 1 ms. On the cart (2026-11-05, in the air) a 100 Hz PWM was NOT loud: the wheel ran smoothly and
   the gearbox made most of the noise; the flag now says so.
8. (Settled 2026-10-29, Yohai: no scope measurement.) t_ISR from the A2 run: about 13 µs per TIM7 interrupt in
   the Debug build (from the loop passes), not the 1 µs of tutorial 4's examples; a bare polling pass is 1.1 µs, as the
   slide says. Tutorial 4 slide 28 gets a note that its 1 µs is only an example value, not our hardware.
9. **Bucher's 2025 polling-PWM code** (main loop: `pwm = (TIM2->CNT % 100) < x`). With TIM2 at
   PSC 0 the counter moves about 170 counts per loop pass, so `% 100` is sampled almost at random:
   the LED's average brightness is right, but it isn't a clean PWM. Its comment "48.47 times per
   second" matches nothing. Ask Bucher which TIM2 settings he used before anyone reuses it.
10. **A3 encoder mode.** A3's `ass3.ioc` is a copy of `ass2.ioc`, but A2 D.1 has students switch
   TIM1 between Encoder Mode TI1 and TI1 and TI2, so TIM1 may end in either mode while TIM3/4/8 stay
   in TI1. A3 should state one mode for all four wheels (TI1 and TI2 gives ×4 resolution).
11. (Done 2026-10-22: A2 now uses the provided sonar.h/.c. Part B is B.1 calibration (raw to mm,
   50 to 500 mm, compare with 343 m/s and the TIM5 settings), B.2 polling (sonarPoll, read/missed
   counts), B.3 interrupts (EXTI for PA7, TIM5 capture via sonarCaptureEdge); stop threshold 150 mm.
   The configuration section no longer gives the tick size or the distance formula.) Still to
   check on the board: the 150 ms HAL_Delay in B.2 should make nearly every polled reading an
   overcapture (missed); confirmed on the cart (readings=0, missed=64, on 2026-10-29 and 2026-11-04).

## 7. Plan rationale and notes (kept out of MP2026_plan.tex)

- **Sources per session:** session 1 = Bucher's Lesson 1 + his first-app tutorial deck (Lesson 0 as
  self-study background). Session 2 = Lesson 2. Session 3 = Lesson 3. Session 8 tutorial = 2025
  TA7/TA8. Session 9 tutorial = 2025 TA6. Session 11 tutorial = 2025 TA10/TA11. Sessions 4 onwards
  follow the syllabus order, otherwise.
- **I2C before PWM (session 5):** A1 uses the IO board as a black box, so opening it right after A1
  motivates writing the drivers.
- **Quizzes:** Quiz 1 (lecture 4) covers sessions 1 to 3, Quiz 2 (lecture 9) covers sessions 4 to 8.
  That coverage split is my reading of the plan and appears in the plan's grading table; Bucher and
  Yohai agreed only on the dates and weights.
- **Submission format (2025, assumed kept):** one zip per assignment with the CubeIDE project,
  `main.c`, a short demo video and a short typed report (no pasted code). Up to 3% creativity bonus
  per assignment.
- **A1 draft details removed from the plan:** deliverables = project zip, video of Parts D to F, and
  a report with the predict-then-run table (A), truth table (C), timing answers (D) and one paragraph
  per part. Suggested internal weighting: A 15, B 20, C 15, D 15, E 20, F 15.
