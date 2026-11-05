wrongSol: a deliberately WRONG copy of the A2 reference solution (TA only).
It exists to check that /skl_prj_checkAss catches errors. Row name in the sheet: "ref wrong solution".

ass2_wrong/      the CubeIDE project (copy of fullSolution/ass2_sol, updated 2026-11-05 to the shortened A2
                 instructions; the project inside is still named ass2_sol, so its .elf is Debug/ass2_sol.elf).
                 There is no wrong report.

The planted errors (each marked "PLANTED ERROR n" in the code):
1. A.3.1 (partA.c): CCR1 is set to duty x 99, not duty x 100, while the line prints duty x 100.
2. B.3.2 (partB.c): the ultrasonic sensor is started in polling mode in an interrupt task, so no reading
   completes (readings=0).
3. C.5 (partC.c): the OFF count is computed with a wrong divisor (900 us gives 194, not 184).
4. D.1.3 (partD.c): both runs are in TI1 (the second still prints mode=TI12), so the factor is about 1.

History: on 2026-11-04 (the earlier instructions) the automatic test caught the four errors of that version
(A.3, B.1 avg / 9, C.5, D.1.2 with 3 runs). Errors 2 and 4 were replaced on 2026-11-05, because B.1 has no
mandatory lines any more and D.1 has one motor run per mode. On 2026-11-05 (one sensor per B task, the second cart) the automatic test caught all four errors of
this version (A.3.1, B.3.2 readings=0, C.5, D.1.3 factor 1.00), and passed every other item.
