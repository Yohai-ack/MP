---
name: skl_prj_checkAss
description: Check the students' submissions of ONE MP assignment (STM32 repo), given as the argument (/skl_prj_checkAss 1, /skl_prj_checkAss 2) or asked for. Builds and flashes each group's CubeIDE project on the cart, runs the automatically testable tasks through assignments/autotest/assignments_autotest.py, reviews each group's code and PDF report against the reference solution, and fills the comment columns of that assignment's sheet in "assignments/autotest/assignments gradings.xlsx", never touching the points and never changing a comment Yohai already has without his OK. Refuses assignments that have no reference solution yet. Use when Yohai invokes /skl_prj_checkAss or asks to check, test or grade assignment submissions.
---

# skl_prj_checkAss: check an assignment's submissions

The script `assignments/autotest/assignments_autotest.py` does the board work; this skill adds the code and report
review. Everything goes into **`assignments/autotest/assignments gradings.xlsx`**, one sheet per assignment
("Assignment 1", ...). The workbook, the logs and `work/` are git-ignored: they hold student names.

## The sheet, and what Claude may touch

The sheet starts with the rows **ref good solution**, **ref wrong solution**, then **group1** to **group10**, always in
this order (the script adds any that are missing). Groups are known **only by their number**: the course policies
forbid names, ID numbers and emails in the submission, and Claude never asks for, writes or keeps student names (Yohai
has the key from group numbers to students). One row per group. For each item of the assignment (A.1, A.2, ... E for A1) a **comment** column and a **points**
column, then **Others** (comment, points), then **Total** = 100 + every points column (Yohai types deductions as
`-2`).

The routine: Claude runs the check; then **Yohai edits the sheet himself** (points and wording). So:
- **Never touch a points column**, ever. Claude writes only comment cells (and the Total formula).
- **Never overwrite or change a comment that is already in the sheet** without Yohai's OK. The script enforces it:
  a new comment for a cell that is empty (or that this run of `test` wrote itself) is written; a new comment for a
  cell that holds something different is **not** written but kept in `work/assN/pending.json`, and `test` prints the
  list. Before finishing, show Yohai every waiting change as a table (group, item, now, new), ask with
  AskUserQuestion which to apply, and run `apply` for the approved ones and `drop` for the refused ones:
  ```bash
  python assignments/autotest/assignments_autotest.py pending --ass 1
  python assignments/autotest/assignments_autotest.py apply --ass 1 --name "<name>" --task A.1 --task B.2
  python assignments/autotest/assignments_autotest.py drop --ass 1 --name "<name>"
  ```
  (`apply`/`drop` without `--name`/`--task` act on everything waiting.) Never edit the workbook by other means.

Comment wording:
- `works good` when an automatic check passed;
- otherwise a VERY short note of what is wrong (a few words, numbers where they help), yellow;
- review remarks are added as `code: ...` or `report: ...` to the item they concern (or to Others).
Items that need a hand on the board or an observation (A1: C.1 to C.5 and E, graded from the videos; A2: the hand
passes of B.2 and B.3, D.1.1, D.1.2, the videos, and the report items A.4, B.1, E.1 and E.3) get no automatic comment, but their code is still read (step 6)
and gets `code: ...` remarks, like every other item.
If a report or the code still shows a student's name or ID number, do not copy it anywhere; add `Others`:
`report: names in the submission` (the policies forbid them) and go on.

## Steps

1. **Which assignment: exactly one per run.** Take it from the argument (`/skl_prj_checkAss 1` checks Assignment 1
   only, `/skl_prj_checkAss 2` Assignment 2 only). Without an argument, ask with AskUserQuestion: "Which assignment do
   you want to check?", options Assignment 1, Assignment 2, Assignment 3. Never check two assignments in one run; every
   command below gets that one `--ass N`. An assignment can be checked only when it has a reference solution, listed
   in `REFERENCE` in the script (now: 1 and 2). For any other, **do nothing** and answer:
   "Assignment N has no reference solution yet, so it cannot be checked automatically. Nothing was tested."
   (The script refuses the same way, exit code 2.)
2. **Tell Yohai the state of the workbook, and ask before anything starts.** Before any build or flash, say in a
   short message:
   - whether `assignments/autotest/assignments gradings.xlsx` already exists, and whether its sheet "Assignment N"
     already holds comments or points (which rows, and how many cells);
   - how existing data is kept: points cells are never written; a comment cell that already holds text is never
     overwritten, the new text waits in `work/assN/pending.json` and is shown to him as now/new at the end, to apply
     or drop one by one; only empty cells (or cells this run wrote itself) are filled; rows are only added, never
     deleted or reordered; and a copy of the workbook is saved first as `work/assN/gradings_backup_<date>.xlsx`;
   - then ask with AskUserQuestion whether to start the check. Do nothing until he says yes.
3. **What to check.** Ask for the folder with the groups' ZIP files (named `assN_groupK.zip`, for example
   `ass1_group3.zip`, as the submission rules require; the row name is then `groupK`), or a single project folder plus the name to put in the row. With a single project, the
   report is the PDF passed with `--report`, or else every PDF next to the project folder. Only when no PDF is found
   does Others get "no report PDF found". The two reference rows for A1:
   - "ref good solution": `assignments/ass1/solution/fullSolution/ass1_sol` (report `ass1_sol.pdf`);
   - "ref wrong solution": `assignments/ass1/solution/wrongSol/ass1_wrong` (report `ass1_wrong.pdf`; its planted
     errors are listed in `wrongSol/README.txt`, so it shows whether the check catches them).
   For A2: "ref good solution" = `assignments/ass2/solution/fullSolution/ass2_sol` (report `ass2_sol.pdf`), and
   "ref wrong solution" = `assignments/ass2/solution/wrongSol/ass2_wrong` (planted errors in its `README.txt`).
   The reference PDFs are git-ignored (built from their `.tex`): if one is missing, e.g. on the other PC, build it
   with `pdflatex` (twice) in its folder first.
4. **Free the board, and get the OK to flash.** Ask Yohai to close PuTTY (it holds the COM port), to stop any debug
   session in CubeIDE (it holds the ST-LINK; CubeIDE may stay open), to close the workbook in Excel, and to check that
   the cart is powered with the Nucleo jumper on E5V. **For A2 the cart must be on its block with the wheels in the
   air, and nothing in front of the IR and ultrasonic sensors** (wheel 1 turns in A.2.2, A.3, B and D, all four
   wheels in E.2). The test presses B1 itself over SWD (PC13 driven HIGH for half a second) to end the B tasks, so
   nobody may press B1 during the run. Flashing is driving real hardware: wait for his explicit OK.
5. **Run the test** from the repo root, in the background (about 3 minutes per group for A1, about 4 for A2, most of
   it D.2, which runs until the counter has wrapped twice):
   ```bash
   python assignments/autotest/assignments_autotest.py test --ass 1 --submissions "<folder>"
   python assignments/autotest/assignments_autotest.py test --ass 1 --project "<folder>" --name "<name>" [--report "<pdf>"]
   ```
   Run Python **without `-I`** (pyserial is installed in the user site-packages). It prints one line per item, the
   changes waiting for approval, and keeps `work/assN/summary.json` (each group's project, report PDFs and comments)
   and `logs/assN/<name>.log` (every line the board printed, with PC time).
   If a check fails for a reason that looks like the test and not the student (an ST-LINK error, a timeout on a task
   that the log shows working), look at the log, fix the cause, and run that group again; do not leave a wrong
   comment.
6. **Review each group's code**, for **every item of the assignment**, in every assignment: also the items that the
   board check cannot run (they need button presses, an obstacle, a hand on a wheel, or are graded from a video or
   only from the report). The board cannot see everything. Read the group's source files (`Core/Src`,
   `Core/Inc`, the user code of `main.c`) and check the rules of the instructions that the output cannot show; for A1
   at least: `task` declared as required; B.1 with functions (not macros) and bitwise operators, LD2 through
   `GPIOA->ODR`; B.2/B.3 changing the variable only with bit operations (an `if`/`switch` chain, a counter mapped to
   LEDs or a table of patterns is **not** accepted, even though the output looks right) and lighting the LEDs from its
   bits; the provided `printBinary` (A.1) called from a line-printing function of their own; C.2 to C.5 without
   LEDs, button 1 ending a round and printing once, nothing printed while counting; E without dimming, a brightening
   that always runs to its end, the next one only after the condition went false and true again (debounce with
   `HAL_GetTick`, no blocking delay in E's stepping); tidy, commented code (general instructions). For A2 at least:
   `task` a global `volatile uint16_t`, `pwmN`/`pwmM` global `volatile uint32_t`; interrupts on only in the tasks
   that use them (PA7's EXTI disabled in USER CODE 2, enabled only in 222; the ultrasonic interrupt only in 232 and, if they use it, B.1); the interrupt
   callbacks write only `TIM2->CCR1` and flags (no I2C, no `printf`); in B, one sensor per task (B.2 the IR sensor only, B.3 the ultrasonic one only), LEDs and the status line only when the
   status changes, `HAL_Delay(150)` on every pass, B1 ending the task; `IR_OBSTACLE_LEVEL` defined once and used;
   Part C without `io_board.c` calls; D.1.3's change not lost to a 16-bit wrap; D.2 and E in the mode with the better
   resolution, D.2's second way as `(int16_t)(now - previous)` with `uint16_t` readings; E for all four wheels,
   positive forward, recorded inside the TIM6 interrupt and printed afterwards. One remark per problem, VERY short:
   ```bash
   python assignments/autotest/assignments_autotest.py note --ass 1 --name "<name>" --task B.2 --kind code --text "if chain picks the pattern"
   ```
7. **Review each report.** Read the group's report PDF(s) with the Read tool, and compare with the reference report
   (`REFERENCE` in the script; for A1 `assignments/ass1/solution/fullSolution/ass1_sol.tex`, its answers and its
   "Grading notes"), and with the instructions in `MP2026_plan.tex`. Look for: missing items, wrong or missing
   explanations, numbers that contradict the board results in the log, the required tables and diagrams (A1: the C
   table, E truth table and state diagram), the title page and the AI declaration, whole functions copied in (general
   instructions). One remark per problem, VERY short:
   ```bash
   python assignments/autotest/assignments_autotest.py note --ass 1 --name "<name>" --task C.2 --text "no table of the rounds"
   ```
   (`--task Others` for remarks that belong to no item.) A report with no problem gets no remark. A remark the cell
   already holds is not added twice.
8. **Ask about the waiting changes** (see above), then **report** in a few lines: how many groups, which items failed
   most, anything that could not be tested, and the path of the workbook. Do not commit anything.

## Adding an assignment (when its reference solution exists)

In the script: add its items to `TASKS`, its reference project and report to `REFERENCE`, a `test_assN` function
with its checks (see `test_ass1` and the `Tester` class: write `task` over SWD, wait for tagged lines, compare), and
register it in `TESTS`. The line formats and the type of `task` come from that assignment's instructions (A1:
`uint8_t`, written with `-w8`; A2: `uint16_t`, written with `-w16`, class `Tester2`). Make a wrong copy of its
reference (like `ass1/solution/wrongSol`) and check that the errors are caught.

## Notes

- `STM32_Programmer_CLI -w8` takes its data in hex only: a decimal `11` is written as 0 (the script sends `0x0B`).
- HOTPLUG mode connects without halting or resetting, so `task` is written while the program runs.
- The headless CubeIDE build needs a path with backslashes (Eclipse reads `C:/...` as a URI scheme); the script
  normalises it.
