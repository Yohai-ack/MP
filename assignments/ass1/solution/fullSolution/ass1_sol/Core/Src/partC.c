/**
 ******************************************************************************
 * @file    partC.c
 * @brief   Assignment 1, Part C: reading the buttons (C.1), and counting
 *          presses of button 0 four ways (C.2 to C.5).
 *
 *          C.2 to C.5 count in rounds: button 1 ends a round, prints its
 *          result line and starts the next one. Nothing else is printed
 *          while counting (a printed line takes milliseconds), and the LEDs
 *          are not used, so a pass of the loop is one button read (one I2C
 *          read for all 4 buttons, about 100 us).
 ******************************************************************************
 */
#include "main.h"
#include "ass1.h"
#include "bits.h"
#include "io_board.h"
#include <stdio.h>

#define ROUND_IGNORE_MS  500u            /* button 1 bounces too        */

volatile uint8_t  c1Raw;                 /* C.1: raw byte (active-low)  */
volatile uint8_t  c1Held;                /* C.1: bit n = 1 while held   */
volatile uint8_t  c1Count;               /* C.1: buttons held (0 to 4)  */
volatile uint32_t cCount;                /* presses counted this round  */
volatile uint32_t cLoopsPerSec;          /* C.2: main-loop passes / s   */
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

/* ------------------------------------------------------------------ */
/* button 1: the end of a round                                        */
/* ------------------------------------------------------------------ */
typedef struct
{
    uint8_t  before;        /* button 1 held on the previous pass       */
    uint32_t lastEnd;       /* HAL_GetTick() of the last accepted press */
} RoundButton;

static void roundStart(RoundButton *b, uint8_t held, const char *tag)
{
    b->before  = bitTest(held, 1);
    b->lastEnd = HAL_GetTick();
    cCount = 0;
    printf("(%s) press button 0, then button 1 to end the round\r\n", tag);   /* not a tagged line */
}

/* 1 once per press of button 1 (released -> pressed); after it, button 1
   is ignored for ROUND_IGNORE_MS, so that its bounces print nothing      */
static uint8_t roundEnded(RoundButton *b, uint8_t held, uint32_t tick)
{
    uint8_t now = bitTest(held, 1);
    uint8_t pressed = now && !b->before;
    b->before = now;
    if (pressed && (tick - b->lastEnd >= ROUND_IGNORE_MS))
    {
        b->lastEnd = tick;
        return 1;
    }
    return 0;
}

/* C.2: level-based. The count grows on every pass while button 0 is held.
   The result line also gives the loop passes per second over the round. */
void partC2(void)
{
    RoundButton b;
    uint32_t loops = 0;
    uint8_t  held  = readButtonsHeld();
    uint32_t start;

    roundStart(&b, held, "C2");
    start = HAL_GetTick();
    while (task == TASK_C2)
    {
        held = readButtonsHeld();
        if (bitTest(held, 0))
        {
            cCount++;
        }
        loops++;

        uint32_t tick = HAL_GetTick();
        if (roundEnded(&b, held, tick))
        {
            uint32_t ms = (tick > start) ? (tick - start) : 1u;
            cLoopsPerSec = (uint32_t)((uint64_t)loops * 1000u / ms);
            printf("C2: count=%lu passes=%lu\r\n", cCount, cLoopsPerSec);
            cCount = 0;                     /* the next round           */
            loops  = 0;
            start  = HAL_GetTick();
        }
    }
}

/* C.3 (no delay) and C.5 edge version: count released -> pressed */
static void edgeCounter(uint8_t myTask, uint8_t withDelay, const char *tag)
{
    RoundButton b;
    uint8_t before = readButtonsHeld();

    roundStart(&b, before, tag);
    while (task == myTask)
    {
        uint8_t now     = readButtonsHeld();
        uint8_t pressed = now & (uint8_t)~before;   /* 0 -> 1 this pass */
        before = now;

        if (bitTest(pressed, 0))
        {
            cCount++;
        }
        if (roundEnded(&b, now, HAL_GetTick()))
        {
            printf("%s: count=%lu\r\n", tag, cCount);
            cCount = 0;
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
    RoundButton b;
    Debouncer btn = { 0 };
    uint8_t held = readButtonsHeld();

    roundStart(&b, held, tag);
    while (task == myTask)
    {
        uint32_t tick = HAL_GetTick();
        held = readButtonsHeld();

        if (debounceUpdate(&btn, bitTest(held, 0), tick, cDebounceMs))
        {
            cCount++;
        }
        if (roundEnded(&b, held, tick))
        {
            printf("%s: count=%lu\r\n", tag, cCount);
            cCount = 0;
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
