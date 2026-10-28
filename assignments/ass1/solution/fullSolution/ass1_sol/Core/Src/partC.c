/**
 ******************************************************************************
 * @file    partC.c
 * @brief   Assignment 1, Part C: reading the buttons (C.1), and counting
 *          presses of button 0 four ways (C.2 to C.5).
 *
 *          The count is shown on the 8 RGB LEDs (lowest 8 bits, bit n =
 *          LED n) and printed, only when it changes: one LED update is
 *          24 I2C writes (about 2 ms) and a printed line a few ms.
 *          Write 1 into cReset (Live Expressions) to start a new round.
 ******************************************************************************
 */
#include "main.h"
#include "ass1.h"
#include "bits.h"
#include "io_board.h"
#include <stdio.h>

volatile uint8_t  c1Raw;                 /* C.1: raw byte (active-low)  */
volatile uint8_t  c1Held;                /* C.1: bit n = 1 while held   */
volatile uint8_t  c1Count;               /* C.1: buttons held (0 to 4)  */
volatile uint32_t cCount;                /* presses counted             */
volatile uint32_t cLoopsPerSec;          /* C.2: main-loop passes / s   */
volatile uint8_t  cReset;                /* write 1: count back to 0    */
volatile uint32_t cDebounceMs = 20;      /* C.4: stable time            */
volatile uint32_t cDelayMs    = 150;     /* C.5: the "other task"       */

/* C.1: count the buttons held, print a line at the start and on a change */
void partC1(void)
{
    uint8_t lastCount = 0xFF;               /* forces the first print  */

    while (task == TASK_C1)
    {
        c1Raw  = readButtonsRaw();
        c1Held = (uint8_t)(~c1Raw) & 0x0Fu; /* invert, mask bits 4..7  */
        c1Count = 0;
        for (uint8_t n = 0; n < 4; n++)
        {
            c1Count += bitTest(c1Held, n);
        }

        if (c1Count != lastCount)
        {
            printf("C1: raw=");
            printBinary(c1Raw, 8);
            printf(" held=");
            printBinary(c1Held, 8);
            printf(" count=%u\r\n", c1Count);
            lastCount = c1Count;
        }
    }
}

static void showCount(const char *tag)
{
    showByteOnLeds((uint8_t)cCount);
    printf("%s: count=%lu\r\n", tag, cCount);
}

static void checkReset(const char *tag)
{
    if (cReset)
    {
        cReset = 0;
        cCount = 0;
        printf("== new round ==\r\n");
        showCount(tag);
    }
}

/* C.2: level-based. The count grows on every pass while the button is
   held, so it is shown once, when the button is released.             */
void partC2(void)
{
    uint8_t  before = 0;
    uint32_t loops = 0;
    uint32_t secondStart = HAL_GetTick();

    cCount = 0;
    showCount("C2");

    while (task == TASK_C2)
    {
        checkReset("C2");

        uint8_t now = bitTest(readButtonsHeld(), 0);
        if (now)
        {
            cCount++;
        }
        if (before && !now)                 /* just released           */
        {
            showCount("C2");
        }
        before = now;

        loops++;
        if (HAL_GetTick() - secondStart >= 1000)
        {
            cLoopsPerSec = loops;
            printf("C2: passes=%lu\r\n", cLoopsPerSec);
            loops = 0;
            secondStart += 1000;
        }
    }
}

/* C.3 (no delay) and C.5 edge version: count released -> pressed */
static void edgeCounter(uint8_t myTask, uint8_t withDelay, const char *tag)
{
    uint8_t before = readButtonsHeld();

    cCount = 0;
    showCount(tag);

    while (task == myTask)
    {
        checkReset(tag);

        uint8_t now     = readButtonsHeld();
        uint8_t pressed = now & (uint8_t)~before;   /* 0 -> 1 this pass */
        before = now;

        if (bitTest(pressed, 0))
        {
            cCount++;
            showCount(tag);
        }

        if (withDelay)
        {
            HAL_Delay(cDelayMs);
        }
    }
}

/* C.4 and C.5 debounced version: an edge counts only after the new
   button state has been stable for cDebounceMs                      */
static void debouncedCounter(uint8_t myTask, uint8_t withDelay, const char *tag)
{
    Debouncer btn = { 0 };

    cCount = 0;
    showCount(tag);

    while (task == myTask)
    {
        checkReset(tag);

        uint8_t now = bitTest(readButtonsHeld(), 0);
        if (debounceUpdate(&btn, now, HAL_GetTick(), cDebounceMs))
        {
            cCount++;
            showCount(tag);
        }

        if (withDelay)
        {
            HAL_Delay(cDelayMs);
        }
    }
}

void partC3(void)          { edgeCounter(TASK_C3, 0, "C3"); }
void partC4(void)          { debouncedCounter(TASK_C4, 0, "C4"); }
void partC5Debounced(void) { debouncedCounter(TASK_C5_DEBOUNCED, 1, "C5"); }
void partC5Edge(void)      { edgeCounter(TASK_C5_EDGE, 1, "C5"); }
