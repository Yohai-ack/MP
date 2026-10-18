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
  pdflatex works there (checked 2026-10-18), and so does PowerPoint, which is also used to render
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
  file and ask Yohai to close it. Don't silently skip it.
- **Never drive real hardware** (flash a board, run motors) on my own. Building and static analysis
  are fine. Anything that runs on the cart, Yohai does or approves.

## 3. The scope of my help

1. **Understanding questions.** Yohai is learning STM32/embedded C. His running question list is
   `general/rolling doc MP course.docx` (questions keyed to lesson + slide, e.g. "Les2 s17").
2. **Tutorials.** Help build the 1 h tutorials that accompany each lecture.
3. **Assignments.** Write **one unified assignments document** covering all labs. Bucher's lessons
   mention assignments piecemeal; the unified doc is ours. Then **solve** the assignments (reference
   solutions).
4. **Quizzes.** Two quizzes (lectures 4 and 9). Help write them and their solutions.
5. **Grading.** Possibly help check submitted assignments.

Course-specific technical conclusions belong here or in a document in this folder, not only in chat.

## 4. The course as it stands (snapshot 2026-10-02 — re-check the shared folder)

### 4.1 Platform — one cart, one MCU

- **A1's `.ioc` is provided, not built by students** (agreed with Bucher, 2026-10-15): `ass1/ass1.ioc`,
  generated from the hardware-test `.ioc` with only A1's items (GPIO LD2/B1/IR_SENSOR, LPUART1,
  I2C1, EXTI15_10). Checked 2026-10-15: CubeMX 6.18 headless generates it, and it builds with
  `io_board.c`, 0 warnings. Tutorial 5 (first tutorial after A1 is due) teaches creating a `.ioc`.
  For A2, students duplicate `ass1.ioc` (CubeMX *Save Project As* → `ass2/ass2.ioc`) and add only
  the new items. **No `ass2.ioc` is ever given to students** (Yohai, 2026-10-17): they get only the
  pinout and settings, as the "Assignment 2: project configuration" section of the A2 instructions.
  A TA-built `ass2.ioc`, if made to check that section, is TA-only (under a `solution/` folder).
- **Pin/peripheral configuration for students: a "project configuration (.ioc)" section at the end
  of each assignment** in `MP2026_plan.tex` (Yohai, 2026-10-11; replaced the separate
  `ass_*_ioc.tex` files, now deleted). For A1 it's a reference list of what `ass1.ioc` contains.
  Each lists only what that assignment needs, so students
  aren't confused, and they're **cumulative**: A2's contains everything in A1's, even where A2
  doesn't use it. A3 may share A2's or get its own. Unconfirmed hardware stays out of them (open
  items in §6). All values come from the working hardware-test project
  (`hardwaret_test/align_servo.ioc` + `main.c`).
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
- **I2C IO board:** 8 RGB LEDs via 3× **PCA9634** (one chip per colour: RED 0x01, GREEN 0x02,
  BLUE 0x07; channel n = LED n). 4 buttons on **PCA9538A** at 0x71 (active-low, bits 0–3).
  **PCA9685** at 0x44 makes 50 Hz servo pulses (PRESCALE = 121).
- **Servo + HC-SR04 sonar** (scanning "radar"). Trigger from an MCU pin; echo measured by timer
  input capture. The working test code uses **900 / 1500 / 2100 µs** (min/centre/max), so use
  that, not the syllabus's 1000–2000 µs.
- **4 DC motors + quadrature encoders**, through H-bridges (timer PWM).
- **IR on/off obstacle sensor** at the front (PA7, active-low): a digital input, later used with EXTI.
- Power-jumper note (learned 2026-10-01): the Nucleo power-source jumper must be on **5V_STLK** for
  USB power. On **E5V** the MCU is unpowered, while the ST-LINK still enumerates, which gives
  "No STM32 target found". Worth putting in the students' troubleshooting notes.

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
A1 20%, A2 25%, A3 35%.** Each quiz takes the **first hour of a lecture**: Quiz 1 in lecture 4
(23.11.2026), Quiz 2 in lecture 9 (28.12.2026; moved from lecture 8 on 2026-10-17). Yohai will probably
write the quizzes.

**Schedule as of 2026-10-17 (Yohai):** A1 out session 2, due session 5. A2 out session 7 (14.12),
due session 10 (4.1.2027) (both moved one week later). **"Last material included" rule:** an
assignment may require material taught up to and including the lecture one week after it is out
(A1: session 3, A2: session 8), nothing later. Shown in bold in the plan's milestone column.

**Work order and roles (agreed 2026-10-08):** the current main focus is **writing Assignment 1**.
Bucher derives the first lectures' content from it. After that: A2 and A3; Bucher writes the
lectures, Yohai the tutorials and (probably) the quizzes.

The working plan is `MP2026_plan.tex`
(13 sessions, lecture n ↔ tutorial n). **It is the reference for which tutorial matches which lesson.** Lectures are on
**Mondays, from 2.11.2026**: 12 lecture+tutorial sessions → 18.1.2027, plus session 13 (25.1.2027),
a lab meeting for open questions. Checked against the Technion calendar. A3 is out 11.1.2027 and due
**3.3.2027**.

**Assignment emphasis (Yohai, 2026-10-02):** start from the 2025 cart assignments, but shift the
focus to **coding, hardware and timing issues**. Kinematics and trajectory work is "less
interesting", so give it less weight. **A2 drafted 2026-10-17** from Yohai's outline: A PWM in three ways
(HAL_GetTick 1 ms software PWM as a demo of its limit; software PWM in a 10 kHz TIM7 interrupt and its
CPU cost; hardware PWM: TIM2 CCR and the PCA9634's own PWM), B polling vs. interrupts (IR + ultrasonic stop a
wheel), C own I2C driver, D encoder resolution (TI1 vs. TI1 and TI2) and wrap-safe speed, E step
response to MATLAB. PI control is no longer in A2 (likely A3). A3 contents: don't design until asked.

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
MP2026_plan.tex                13-session plan (lecture ↔ tutorial, dates) + assignment proposals (A1 and A2 detailed)
                               (the .tex goes in git, not the PDF)
MP2026_plan.docx               Word copy of the plan for Bucher, made with /skl_glb_tex2Word (in git).
                               Regenerate it after plan edits before sending it to him
ass1/                          files handed to A1 students: ass1.ioc (the project configuration),
                               io_board.h, io_board.c (copied
                               unmodified from Bucher's Lesson 3 package). The ultrasonic sensor is
                               not part of A1 (Yohai, 2026-10-11); A1 uses only the IR sensor
ass1/solution/                 TA-ONLY, never handed to students. checkBounce/ = CubeIDE project
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
                               PuTTY. Yohai fills checkBounce2_instructions.txt
tutorials/MP_tutorials_2026.pptx    all 12 tutorials in one deck, one section per tutorial (may be
                               split into 12 PDFs for students later). Built from a pptxgenjs
                               script (scratchpad). Content so far: tutorial 1 (USER CODE
                               sections), tutorial 4 (slides 13 to 16, added 2026-10-18 with
                               python-pptx: timer-to-callback flow chart + formulas; timing
                               definitions t_clk, t_tick, T_period, t_ISR, t_free with the rule
                               t_ISR < T_period (works/fails cases); worked example TIM7
                               100 us / TIM6 1 s; the ISR
                               definition and HAL callback chain; then fast vs slow outputs, blocking, the
                               logic/store/transport layers, the super loop), tutorial 5
                               (creating a .ioc in CubeMX: flow, rebuilding ass1.ioc in two
                               tables, duplicating it for A2; now slides 23 to 26, added in place
                               2026-10-15 by duplicating slides 9 and 16). Once Yohai edits
                               it in PowerPoint, never regenerate it: add slides in place

general/rolling doc MP course.docx   Yohai's running list of understanding questions
hardwaret_test/                CubeIDE project (G474RE; align_servo.ioc; HardwareTest + servo builds)
                               + HowToUse.txt (how to run the test: states, debugger, PuTTY)
debug_011026/                  scratch project for the 2026-10-01 ST-LINK debugging
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
- **Every function students need but haven't learned yet is provided, and the instructions say so**
  (Yohai, 2026-10-11). Short code goes as text in the assignment's appendix, for copy-paste (e.g.
  the `__io_putchar` printf redirect in Appendix A1). Long code goes as files in that assignment's
  folder (`STM32/ass1/`, `ass2/`, …), and the instructions name the files and where to copy them.
- **Compile-check after every code edit** (verified working 2026-10-10 on `hardwaret_test`: 28
  files, 0 warnings, 2 s). CubeIDE 2.2.0 is at `C:\ST\STM32CubeIDE_2.2.0`. Put on PATH its
  `plugins\com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.14.3.rel1.win32_*\tools\bin`
  and `plugins\com.st.stm32cube.ide.mcu.externaltools.make.win32_*\tools\bin`, then run
  `make -j8 all` inside the project's `Debug\` folder. It uses CubeIDE's own generated makefile,
  so it builds what the IDE builds. Limit: that makefile lists the source files, so a **new** `.c`
  file is only picked up after CubeIDE regenerates it (Yohai builds once in the IDE). Building is
  fine; **flashing the board is not**, unless Yohai explicitly asks.
- **Deliverables for Bucher** are prepared here; Yohai moves them to the shared folder himself.
- **Word copies for Bucher:** `/skl_glb_tex2Word MP2026_plan.tex --overwrite` (real tables, red/blue
  notes kept as the Word styles "Color red"/"Color blue"). It reports TABLES and LEAKS; both must be clean.
- **Scripted edits of the .tex:** never put backslashes inside a bash heredoc (the shell tool mangles
  `\\`, which once spliced the plan at the wrong "Part B" and duplicated half of it, 2026-10-18).
  Write the script to a file and build backslashes with `chr(92)`. After any scripted splice, check
  that each heading appears once and diff against the last commit.
- Planned documents (create when the task starts, not before): `assignments/` (unified assignment
  document + solutions + rubrics), `quiz/`, and a `PROGRESS.md` once there is ongoing
  task state to track.
- **Timing vocabulary** (Yohai, 2026-10-18): explain timing in time [s], not frequency. Terms:
  t_clk = 1/f_clk, t_tick = (PSC+1)·t_clk, T_period = (ARR+1)·t_tick, t_ISR (one pass through the
  interrupt), t_free = T_period − t_ISR. Rule: t_ISR < T_period, with a margin (rule of thumb
  about 10 %). The term **ISR** is used: taught in lecture 4 and on tutorial 4's slides 13 to 16.
  In A2's software PWM, "tick" means one TIM7 period; say so wherever both meanings could meet.
- **Editing the tutorials deck:** insert slides in place with a python-pptx script kept in the
  scratchpad. Back up the deck first, and add each new slide to its tutorial's section. The deck
  must be closed in PowerPoint, or the save is refused. Check by rendering through PowerPoint COM
  (`Presentations.Open` on a copy, then `Slides(i).Export(...png)`).

## 6. Open questions for Yohai

1. **Cart hardware, to check on the cart/PCB** (from the hardware-test project; kept out of the
   student-facing configuration sections until confirmed):
   - Which physical wheel is motor 1 to 4 (FL/FR/RL/RR), and each encoder's count direction.
   - Motor-driver enable (M_EN): `main.h` assumes PC3, but the code never drives it and the wheels
     turn anyway. Is there an enable pin at all?
   - **Servo channel on the PCA9685:** the test drives CH0 to CH3 together with the same pulse, so
     it works whichever channel the servo is on. Most likely only one channel is wired (the
     ultrasonic gimbal) and the other three are free headers, but the code can't tell which. The
     A2 configuration section deliberately doesn't name a channel. Check the board before A2 is published.
   - Configured in the test `.ioc` but unused, and purpose unknown: ADC1_IN12 (PB1), ADC5_IN1 (PA8),
     TIM15 (50 Hz base, interrupt enabled), TIM20_CH3 PWM (PC8). Possibly leftovers (battery
     sense? old direct servo timer?). Left out of the student docs.
   - Free GPIO outputs in the test `.ioc`: PC3, PC4, PB5, PB6, PB7, PB14.
   - Encoder mode: TI1 (×2 counting) in the test; TI12 would give ×4. Also the counts per wheel
     revolution (to measure).
   - Is the PCA9538A button chip fitted on this year's IO board? The test code never reads it.
   - `main.h` has a stale comment with TRIG/ECHO swapped (ECHO = PB2, TRIG = PC12). The `.ioc` and
     `main.c` (TRIG = PB2 TIM5_CH1 PWM, ECHO = PC12 TIM5_CH2 IC) are correct.
   - A2's 100 Hz sample interrupt: TIM6 (PSC 169, ARR 9999) chosen in the A2 configuration section,
     plus TIM7 (PSC 169, ARR 99, 10 kHz) for the software PWM of A.2,
     and PA7 becomes GPIO_EXTI7 (EXTI9_5). Not yet checked in CubeMX: build ass2.ioc and verify.
2. **Do A1 students get Bucher's `io_board.h`/`.c`** (Lesson 3 package, `lessons/Lesson_3/
   lesson3_cubeide_sources/`)? His syllabus and Lesson 3 slides say so ("provided LED helper
   library", "black-box principle"), and A1 as written depends on it for the RGB LEDs and buttons,
   but Yohai and Bucher haven't agreed it. If it is given as is, B.4 must forbid
   `readButtonsPressed()` (it already does the invert + mask). Options: as is, a trimmed copy, or
   none (then A1 needs rethinking).
3. (Resolved 2026-10-17: A2 is now due 4.1.2027, one week before A3 goes out on 11.1.2027.)
4. Where does the servo-scanned sonar go: A2, A3 or a bonus? (A2 now uses the sonar alone in Part B
   and the servo alone in C.5; the scanned map is still unplaced.)
5. 🔴 **`io_board.c` writes PCA9634 MODE1 = 0x20, commented "auto-increment".** On the PCA9634 the
   MODE1 auto-increment bits are, I believe, read-only and set through the control byte. Check the
   datasheet before A2 C.3 (one transaction vs. many) is published; if confirmed, tell Bucher.
6. Agree with Bucher: Quiz 2 moved to lecture 9 and now covers sessions 4 to 8 (my assumption), and
   PI speed control is no longer in A2 (Yohai's outline, 2026-10-17). It probably goes to A3.
7. 🔴 **The motor winding's electrical time constant L/R** (red flag in A2 A.2(b), for Bucher): we
   assume about 1 ms, which would explain why a 100 Hz PWM is loud.
8. 🔴 **Measure t_ISR on the board** (Debug build). Tutorial 4's slides 14 and 15 and the A2 Part A
   answers assume about 1 µs (170 cycles).

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
