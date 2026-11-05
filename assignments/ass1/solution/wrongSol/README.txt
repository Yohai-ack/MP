wrongSol: a deliberately WRONG copy of the A1 reference solution (TA only).
It exists to check that /skl_prj_checkAss catches errors. Row name in the sheet: "ref wrong solution".

ass1_wrong/      the CubeIDE project (copy of fullSolution/ass1_sol, renamed ass1_wrong)
ass1_wrong.tex   its report (copy of fullSolution/ass1_sol.tex), and ass1_wrong.pdf

The planted errors, in both the code and the report:
1. A.1 starts all three variables at 120 (u8, i8 and u16), not 250, 120, 65530.
2. Part A prints no binary: A.1 lines have decimal and hex only, A.2 lines have no bin= field
   (since 2026-11-03 printBinary is provided in the instructions; this solution has it but does not call it).

Parts C and E follow the shortened A1 of 2026-11-03 (no LEDs in C, button 1 ends a round; no dimming in E),
identical to the good solution (run on the cart with presses on 2026-11-04). The automatic test was run on
this solution on 2026-11-04: it catches errors 1 and 2, as intended.
3. B.2 and B.3 choose the next pattern with an if chain (partB.c, walkPattern), which the
   instructions forbid. The board output is identical to the good solution, so only the
   code review (and the report) can catch it.
