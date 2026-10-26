/**
 ******************************************************************************
 * @file    partC.c
 * @brief   Assignment 1, Part C: counting presses of button 0, four ways.
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
#include <stdio.h>

volatile uint32_t cCount;                /* presses counted             */
volatile uint32_t cLoopsPerSec;          /* C.1: main-loop passes / s   */
volatile uint8_t  cReset;                /* write 1: count back to 0    */
volatile uint32_t cDebounceMs = 20;      /* C.3: stable time            */
volatile uint32_t cDelayMs    = 150;     /* C.4: the "other task"       */

static void showCount(void)
{
    showByteOnLeds((uint8_t)cCount);
    printf("count = %lu\r\n", cCount);
}

static void checkReset(void)
{
    if (cReset)
    {
        cReset = 0;
        cCount = 0;
        printf("== new round ==\r\n");
        showCount();
    }
}

/* C.1: level-based. The count grows on every pass while the button is
   held, so it is shown once, when the button is released.             */
void partC1(void)
{
    uint8_t  before = 0;
    uint32_t loops = 0;
    uint32_t secondStart = HAL_GetTick();

    cCount = 0;
    showCount();

    while (task == TASK_C1)
    {
        checkReset();

        uint8_t now = bitTest(readButtonsHeld(), 0);
        if (now)
        {
            cCount++;
        }
        if (before && !now)                 /* just released           */
        {
            showCount();
        }
        before = now;

        loops++;
        if (HAL_GetTick() - secondStart >= 1000)
        {
            cLoopsPerSec = loops;
            printf("loop passes per second: %lu\r\n", cLoopsPerSec);
            loops = 0;
            secondStart += 1000;
        }
    }
}

/* C.2 (no delay) and C.4 edge version: count released -> pressed */
static void edgeCounter(uint8_t myTask, uint8_t withDelay)
{
    uint8_t before = readButtonsHeld();

    cCount = 0;
    showCount();

    while (task == myTask)
    {
        checkReset();

        uint8_t now     = readButtonsHeld();
        uint8_t pressed = now & (uint8_t)~before;   /* 0 -> 1 this pass */
        before = now;

        if (bitTest(pressed, 0))
        {
            cCount++;
            showCount();
        }

        if (withDelay)
        {
            HAL_Delay(cDelayMs);
        }
    }
}

/* C.3 and C.4 debounced version: an edge counts only after the new
   button state has been stable for cDebounceMs                      */
static void debouncedCounter(uint8_t myTask, uint8_t withDelay)
{
    Debouncer btn = { 0 };

    cCount = 0;
    showCount();

    while (task == myTask)
    {
        checkReset();

        uint8_t now = bitTest(readButtonsHeld(), 0);
        if (debounceUpdate(&btn, now, HAL_GetTick(), cDebounceMs))
        {
            cCount++;
            showCount();
        }

        if (withDelay)
        {
            HAL_Delay(cDelayMs);
        }
    }
}

void partC2(void)          { edgeCounter(TASK_C2, 0); }
void partC3(void)          { debouncedCounter(TASK_C3, 0); }
void partC4Debounced(void) { debouncedCounter(TASK_C4_DEBOUNCED, 1); }
void partC4Edge(void)      { edgeCounter(TASK_C4_EDGE, 1); }
