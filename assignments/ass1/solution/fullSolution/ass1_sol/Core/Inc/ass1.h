/**
 ******************************************************************************
 * @file    ass1.h
 * @brief   Assignment 1 reference solution: task numbers and shared helpers.
 *          TA ONLY, never handed to students.
 *
 *          All parts live in one program. What runs is chosen by writing a
 *          task number into the global variable `task` in Live Expressions.
 *          Tens digit = part (1 = A ... 5 = E), units digit = sub-task.
 *          Every result line printed to PuTTY starts with its tag (A1:,
 *          B2:, ...) in the format the instructions prescribe.
 ******************************************************************************
 */
#ifndef ASS1_H
#define ASS1_H

#include <stdint.h>

/* ------------------------------------------------------------------ */
/* task numbers (write one of these into `task`)                       */
/* ------------------------------------------------------------------ */
enum
{
    TASK_IDLE         = 0,
    TASK_A1           = 11,
    TASK_A2           = 12,
    TASK_A3           = 13,
    TASK_B1           = 21,
    TASK_B2           = 22,
    TASK_B3           = 23,
    TASK_C1           = 31,   /* buttons held                         */
    TASK_C2           = 32,   /* level-based counting                 */
    TASK_C3           = 33,   /* edge-based counting                  */
    TASK_C4           = 34,   /* debounced counting                   */
    TASK_C5_DEBOUNCED = 35,   /* C.5 on the debounced counter of C.4  */
    TASK_C5_EDGE      = 36,   /* C.5 on the edge counter of C.3       */
    TASK_D1           = 41,
    TASK_D2           = 42,
    TASK_D3_DELAY     = 43,   /* D.3 with HAL_Delay (blocking)        */
    TASK_D3_TICK      = 44,   /* D.3 with HAL_GetTick (non-blocking)  */
    TASK_E            = 51
};

extern volatile uint8_t task;        /* Live Expressions: what runs    */

#define LED_LEVEL   80u              /* "on" brightness of an RGB LED  */

/* ------------------------------------------------------------------ */
/* shared helpers (ass1_common.c)                                      */
/* ------------------------------------------------------------------ */
void    printBinary(uint32_t value, uint8_t nBits);  /* no %b in printf */
void    taskDone(uint8_t myTask);                    /* end of a task   */
void    showByteOnLeds(uint8_t pattern);             /* bit n -> LED n  */
uint8_t readButtonsHeld(void);                       /* bit n = 1: held */

/* debouncer: a new level is accepted after it was stable for stableMs */
typedef struct
{
    uint8_t  raw;          /* last sample                             */
    uint8_t  stable;       /* accepted (debounced) level              */
    uint32_t changeTick;   /* HAL_GetTick() of the last raw change    */
} Debouncer;

uint8_t debounceUpdate(Debouncer *d, uint8_t sample, uint32_t tick, uint32_t stableMs);

/* ------------------------------------------------------------------ */
/* the parts (partA.c ... partE.c) and the dispatcher (ass1_tasks.c)   */
/* ------------------------------------------------------------------ */
void partA1(void);
void partA2(void);
void partA3(void);
void partB1(void);
void partB2(void);
void partB3(void);
void partC1(void);
void partC2(void);
void partC3(void);
void partC4(void);
void partC5Debounced(void);
void partC5Edge(void);
void partD1(void);
void partD2(void);
void partD3Delay(void);
void partD3Tick(void);
void partE(void);

void printMenu(void);
void ass1Run(void);        /* call from the main loop                 */

#endif /* ASS1_H */
