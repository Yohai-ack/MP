# MP.md — Microprocessors course (TA work), read first every session

This file is the context for the **Microprocessor-Based Product Design** course. Read it at the start
of every session, so Yohai never has to re-explain the course. Keep it current: when something about
the course is settled, update this file in the same session.

---

## 1. Who, what, when

- **Course:** Microprocessor-Based Product Design (MP), Technion, Faculty of Mechanical Engineering,
  2026/27 winter semester.
- **Lecturer:** Prof. Izhak Bucher. **TA:** Yohai Ackerman (that's who I work for).
- **Dates:** semester begins **2026-10-28**, runs to about **April 2027**. About 12–13 meetings,
  each a 2 h lecture + 1 h tutorial. Lab work is self-run in **Room 26** (Robotics/Manufacturing
  building).
- **Students** know Python, not C. Two lectures bridge Python → C.
- **Yohai's own level:** somewhat familiar with STM32, still learning. He prepares before the
  semester. Explain from first principles when he asks; don't assume embedded background.

## 2. Folders and permissions — the most important rules

| Folder | What it is | My permission |
|---|---|---|
| `C:\Users\yohai\git_codes\STM32\` (this folder, and all subfolders) | Yohai's **private** working folder. Bucher has **no** access. Git repo → `https://github.com/Yohai-ack/MP.git` | **Read and write**, but only on Yohai's request or as these rules say |
| `C:\Users\yohai\OneDrive - Technion\Izhak Bucher's files - shared_2026\` | Bucher's shared folder. Yohai has write access, and puts files there when he wants Bucher to see them | **READ-ONLY, ALWAYS.** Never write, move, rename or delete anything there, even if asked in passing. If Yohai wants something placed there, I prepare it in `STM32\` and he copies it himself |

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
4. **Quiz.** This semester probably includes a quiz. Help write it (and its solution).
5. **Grading.** Possibly help check submitted assignments.

Course-specific technical conclusions belong here or in a document in this folder, not only in chat.

## 4. The course as it stands (snapshot 2026-10-02 — re-check the shared folder)

### 4.1 Platform — one cart, one MCU

- **NUCLEO-G474RE** (STM32G474RE, Cortex-M4F, 170 MHz) in a **Mecanum-wheel cart**. It's the only
  MCU in the course. The old STM32F446ZE boards and the Wiggler project are retired.
- **I2C1:** SCL = PB8, SDA = PB9. **printf** over LPUART1 at 115200 8N1 (ST-LINK virtual COM, PuTTY).
- **I2C IO board:** 8 RGB LEDs via 3× **PCA9634** (one chip per colour: RED 0x01, GREEN 0x02,
  BLUE 0x07; channel n = LED n). 4 buttons on **PCA9538A** at 0x71 (active-low, bits 0–3).
  **PCA9685** at 0x44 makes 50 Hz servo pulses (PRESCALE = 121, 1000–2000 µs).
- **Servo + HC-SR04 sonar** (scanning "radar"). Trigger from an MCU pin; echo measured by timer
  input capture. Per Bucher's mail (2026-08-10), servo centre is 1500 µs and ±900 µs reaches the
  extremes. That is wider than the syllabus's 1000–2000 µs, so check which one is meant.
- **4 DC motors + quadrature encoders**, through H-bridges (timer PWM).
- **Optical on/off proximity sensor**: a digital input, later used with EXTI.
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

That's 91% total, plus up to 3% creativity bonus per lab. Dates go on Moodle. 🔴 The remaining 9%
isn't stated anywhere. It may be the quiz; ask Yohai or Bucher.

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

🔴 Not yet read (they were locked when I surveyed on 2026-10-02): `Lesson_1/tutorial_first_stm32_app`,
`Lesson_1/stack_buses_decoding_flipflops_counters`, `Lesson_2/lesson02_python_to_C_STM32`.

### 4.6 This folder

```
MP.md                          this file
CLAUDE.md                      auto-loaded pointer to this file
general/rolling doc MP course.docx   Yohai's running list of understanding questions
hardwaret_test/                CubeIDE project (G474RE; align_servo.ioc; HardwareTest + servo builds)
                               + mailFromBucher_100826.txt (servo pulse explanation)
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
- **Deliverables for Bucher** are prepared here; Yohai moves them to the shared folder himself.
- Planned documents (create when the task starts, not before): `assignments/` (unified assignment
  document + solutions + rubrics), `tutorials/`, `quiz/`, and a `PROGRESS.md` once there is ongoing
  task state to track.

## 6. Open questions for Yohai

1. What is the remaining 9% of the grade (the quiz?).
2. Is 12 or 13 meetings correct for the actual timetable, and which lesson file maps to which week?
3. Servo pulse range: 1000–2000 µs (syllabus) or 1500 ± 900 µs (Bucher's servo program)?
