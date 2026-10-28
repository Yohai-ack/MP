/**
 ******************************************************************************
 * @file    ass1_common.c
 * @brief   Assignment 1 reference solution: helpers shared by several parts.
 ******************************************************************************
 */
#include "main.h"
#include "ass1.h"
#include "bits.h"
#include "io_board.h"
#include <stdio.h>

/* print "0b" and the lowest nBits of value, MSB first
   (printf has no %b, so the bits are tested one by one)                */
void printBinary(uint32_t value, uint8_t nBits)
{
    printf("0b");
    for (int8_t i = (int8_t)(nBits - 1); i >= 0; i--)
    {
        putchar(bitTest(value, (uint8_t)i) ? '1' : '0');
    }
}

/* the end of a task that has an end: report it, then wait for a new choice
   (a task stopped early by a new choice is not reported as done)           */
void taskDone(uint8_t myTask)
{
    if (task == myTask)
    {
        printf("=== task %u done ===\r\n", myTask);
    }
    while (task == myTask) { }
}

/* light the RGB LEDs from a byte: LED n is white while bit n is 1 */
void showByteOnLeds(uint8_t pattern)
{
    for (uint8_t n = 0; n < N_LEDS; n++)
    {
        uint8_t v = bitTest(pattern, n) ? LED_LEVEL : 0;
        setRGB_led(n, v, v, v);
    }
}

/* C.1: the raw button byte is active-low (0 = pressed) on bits 0..3.
   Invert it, then mask off bits 4..7, which carry no button.          */
uint8_t readButtonsHeld(void)
{
    return (uint8_t)(~readButtonsRaw()) & 0x0Fu;
}

/* C.4 and E: accept a new level only after it has been stable for
   stableMs. Returns 1 once, at the moment a press (level 1) is accepted. */
uint8_t debounceUpdate(Debouncer *d, uint8_t sample, uint32_t tick, uint32_t stableMs)
{
    if (sample != d->raw)                      /* level moved: restart */
    {
        d->raw = sample;
        d->changeTick = tick;
    }
    else if ((sample != d->stable) && (tick - d->changeTick >= stableMs))
    {
        d->stable = sample;                    /* stable long enough   */
        return sample;                         /* 1 only for a press   */
    }
    return 0;
}
