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

/* B.1: one step on LD2, then read the bit back and print it */
static void b1Step(const char *name, uint32_t odr)
{
    GPIOA->ODR = odr;
    b1Result = bitTest(GPIOA->ODR, LD2_BIT);
    printf("B1: %s LD2=%u\r\n", name, b1Result);
}

/* B.1: drive LD2 through GPIOA->ODR with the bit functions, no HAL */
void partB1(void)
{
    b1Step("start", bitClear(GPIOA->ODR, LD2_BIT));           /* LED off */
    HAL_Delay(1000);
    b1Step("set", bitSet(GPIOA->ODR, LD2_BIT));
    HAL_Delay(1000);
    b1Step("clear", bitClear(GPIOA->ODR, LD2_BIT));
    HAL_Delay(1000);
    b1Step("toggle", bitToggle(GPIOA->ODR, LD2_BIT));

    /* show the last result on LED 0 of the IO board */
    uint8_t v = b1Result ? LED_LEVEL : 0;
    setRGB_led(0, v, v, v);
    printf("B1: LED0=%u\r\n", b1Result);
    taskDone(TASK_B1);
}

/* B.2 and B.3: rotate the pattern left by one place every second.
   (p << 1) moves every bit up, and bit 7 drops out of the uint8_t;
   (p >> 7) brings that same bit 7 down to bit 0; OR joins the two.  */
static void walkPattern(uint8_t start, uint8_t myTask, const char *tag)
{
    b2Pattern = start;
    while (task == myTask)
    {
        showByteOnLeds(b2Pattern);          /* LEDs from the bits only */
        printf("%s: ", tag);
        printBinary(b2Pattern, 8);
        printf("\r\n");
        HAL_Delay(1000);
        b2Pattern = (uint8_t)((b2Pattern << 1) | (b2Pattern >> 7));
    }
}

void partB2(void)
{
    walkPattern(0x01, TASK_B2, "B2");       /* LED 0 lit               */
}

void partB3(void)
{
    walkPattern(0x03, TASK_B3, "B3");       /* LEDs 0 and 1 lit        */
}
