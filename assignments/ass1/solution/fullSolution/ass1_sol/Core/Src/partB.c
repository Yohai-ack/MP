/**
 ******************************************************************************
 * @file    partB.c
 * @brief   Assignment 1, Part B: bit operations and shifting.
 ******************************************************************************
 */
#include "main.h"
#include "ass1.h"
#include "bits.h"
#include "io_board.h"
#include <stdio.h>

#define LD2_BIT   5u                     /* LD2 = PA5 = bit 5 of GPIOA  */

volatile uint8_t b1Result;               /* bitTest of LD2 at the end   */
volatile uint8_t b2Pattern;              /* B.2 / B.3 LED state         */
volatile uint8_t b4Raw;                  /* B.4: raw byte (active-low)  */
volatile uint8_t b4Held;                 /* B.4: bit n = 1 while held   */
volatile uint8_t b4Count;                /* B.4: buttons held (0 to 4)  */

/* B.1: drive LD2 through GPIOA->ODR with the bit functions, no HAL */
void partB1(void)
{
    GPIOA->ODR = bitClear(GPIOA->ODR, LD2_BIT);       /* start from off */
    printf("B.1: LD2 off\r\n");

    HAL_Delay(1000);
    GPIOA->ODR = bitSet(GPIOA->ODR, LD2_BIT);
    printf("set    -> LD2 on\r\n");

    HAL_Delay(1000);
    GPIOA->ODR = bitClear(GPIOA->ODR, LD2_BIT);
    printf("clear  -> LD2 off\r\n");

    HAL_Delay(1000);
    GPIOA->ODR = bitToggle(GPIOA->ODR, LD2_BIT);
    printf("toggle -> LD2 on\r\n");

    /* read the bit back and show it on LED 0 of the IO board */
    b1Result = bitTest(GPIOA->ODR, LD2_BIT);
    setRGB_led(0, b1Result ? LED_LEVEL : 0, b1Result ? LED_LEVEL : 0, b1Result ? LED_LEVEL : 0);
    printf("bitTest = %u -> LED 0 %s\r\n", b1Result, b1Result ? "lit" : "off");

    while (task == TASK_B1) { }                      /* keep the result */
}

/* B.2 and B.3: rotate the pattern left by one place every second.
   (p << 1) moves every bit up, and bit 7 drops out of the uint8_t;
   (p >> 7) brings that same bit 7 down to bit 0; OR joins the two.  */
static void walkPattern(uint8_t start, uint8_t myTask)
{
    b2Pattern = start;

    while (task == myTask)
    {
        showByteOnLeds(b2Pattern);          /* LEDs from the bits only */
        printf("0b");
        printBinary(b2Pattern, 8);
        printf("\r\n");

        HAL_Delay(1000);
        b2Pattern = (uint8_t)((b2Pattern << 1) | (b2Pattern >> 7));
    }
}

void partB2(void)
{
    walkPattern(0x01, TASK_B2);             /* LED 0 lit               */
}

void partB3(void)
{
    walkPattern(0x03, TASK_B3);             /* LEDs 0 and 1 lit        */
}

/* B.4: count the buttons held, print the count when it changes */
void partB4(void)
{
    uint8_t lastCount = 0xFF;               /* forces the first print  */

    while (task == TASK_B4)
    {
        b4Raw  = readButtonsRaw();
        b4Held = (uint8_t)(~b4Raw) & 0x0Fu; /* invert, mask bits 4..7  */

        b4Count = 0;
        for (uint8_t n = 0; n < 4; n++)
        {
            b4Count += bitTest(b4Held, n);
        }

        if (b4Count != lastCount)
        {
            printf("raw 0b");
            printBinary(b4Raw, 8);
            printf("  held 0b");
            printBinary(b4Held, 8);
            printf("  -> %u button(s) held\r\n", b4Count);
            lastCount = b4Count;
        }
    }
}
