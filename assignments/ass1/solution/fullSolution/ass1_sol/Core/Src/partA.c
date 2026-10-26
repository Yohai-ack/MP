/**
 ******************************************************************************
 * @file    partA.c
 * @brief   Assignment 1, Part A: variable types, signed and unsigned,
 *          wrap-around, integer division.
 *
 *          The variables are global and volatile, so that Live Expressions
 *          can show them (and change them) while the program runs.
 ******************************************************************************
 */
#include "main.h"
#include "ass1.h"
#include <stdio.h>

/* ---------------- A.1 --------------------------------------------- */
volatile uint8_t  a1U8;                  /* watch these three in Live   */
volatile int8_t   a1I8;                  /* Expressions                 */
volatile uint16_t a1U16;

volatile uint8_t  a1StartU8  = 248;      /* start close to each limit,  */
volatile int8_t   a1StartI8  = 124;      /* so the wrap comes within a  */
volatile uint16_t a1StartU16 = 65530;    /* few steps                   */
volatile uint32_t a1StepMs      = 500;   /* time between steps          */
volatile uint32_t a1WrapPauseMs = 3000;  /* extra pause after a wrap    */

/* ---------------- A.2 --------------------------------------------- */
volatile uint8_t  a2U8_7F  = 0x7F;
volatile int8_t   a2I8_7F  = 0x7F;
volatile uint8_t  a2U8_F0  = 0xF0;
volatile int8_t   a2I8_F0  = (int8_t)0xF0;     /* 240 does not fit an int8_t: */
volatile uint16_t a2U16    = 0xFFF6;           /* without the cast, GCC warns */
volatile int16_t  a2I16    = (int16_t)0xFFF6;  /* "overflow in conversion"    */

/* ---------------- A.3 --------------------------------------------- */
volatile uint8_t  a3Grade1 = 97;
volatile uint8_t  a3Grade2 = 58;
volatile uint8_t  a3AvgWrong;            /* integer division: 77        */
volatile float    a3AvgRight;            /* floating point:   77.5      */

/* one line of A.1: decimal, hex and binary of the three variables */
static void a1PrintLine(const char *mark)
{
    printf("u8 %3u 0x%02X ", a1U8, a1U8);
    printBinary(a1U8, 8);
    printf(" | i8 %4d 0x%02X ", a1I8, (uint8_t)a1I8);   /* 8-bit pattern */
    printBinary((uint8_t)a1I8, 8);
    printf(" | u16 %5u 0x%04X ", a1U16, a1U16);
    printBinary(a1U16, 16);
    printf("%s\r\n", mark);
}

void partA1(void)
{
    a1U8  = a1StartU8;
    a1I8  = a1StartI8;
    a1U16 = a1StartU16;
    a1PrintLine("");

    while (task == TASK_A1)
    {
        HAL_Delay(a1StepMs);

        a1U8++;
        a1I8++;           /* 127 + 1: the bit pattern 0x80 reads as -128 */
        a1U16++;

        /* a value that just passed its limit lands on its minimum */
        uint8_t wrapped = (a1U8 == 0) || (a1I8 == INT8_MIN) || (a1U16 == 0);

        a1PrintLine(wrapped ? "   << wrap-around" : "");
        if (wrapped)
        {
            HAL_Delay(a1WrapPauseMs);   /* time to look at it */
        }
    }
}

/* one line of A.2 for an unsigned variable */
static void a2PrintUnsigned(const char *name, uint16_t v, uint8_t nBits)
{
    printf("%-22s dec %6u   hex 0x%0*X   bin ", name, v, nBits / 4, v);
    printBinary(v, nBits);
    printf("\r\n");
}

/* one line of A.2 for a signed variable: %X of the variable itself shows
   the sign extension to 32 bits; the cast shows the stored pattern      */
static void a2PrintSigned(const char *name, int16_t v, uint8_t nBits)
{
    uint16_t pattern = (nBits == 8) ? (uint8_t)v : (uint16_t)v;

    printf("%-22s dec %6d   hex 0x%0*X   bin ", name, v, nBits / 4, pattern);
    printBinary(pattern, nBits);
    printf("   (%%X of the value itself: 0x%X)\r\n", v);
}

void partA2(void)
{
    a2PrintUnsigned("0x7F   as uint8_t", a2U8_7F, 8);
    a2PrintSigned  ("0x7F   as int8_t",  a2I8_7F, 8);
    a2PrintUnsigned("0xF0   as uint8_t", a2U8_F0, 8);
    a2PrintSigned  ("0xF0   as int8_t",  a2I8_F0, 8);
    a2PrintUnsigned("0xFFF6 as uint16_t", a2U16, 16);
    a2PrintSigned  ("0xFFF6 as int16_t", a2I16, 16);

    while (task == TASK_A2) { }
}

void partA3(void)
{
    /* the operands are integers, so the division is an integer division:
       155 / 2 = 77, and the .5 is thrown away                           */
    a3AvgWrong = (a3Grade1 + a3Grade2) / 2;

    /* 2.0f is a float, so the sum is converted to float before dividing */
    a3AvgRight = (a3Grade1 + a3Grade2) / 2.0f;

    /* printf without float support: print the tenths by hand */
    uint32_t tenths = (uint32_t)(a3AvgRight * 10.0f + 0.5f);

    printf("grades %u and %u\r\n", a3Grade1, a3Grade2);
    printf("(g1 + g2) / 2    = %u      (integer division)\r\n", a3AvgWrong);
    printf("(g1 + g2) / 2.0f = %lu.%lu    (floating point)\r\n", tenths / 10, tenths % 10);

    while (task == TASK_A3) { }
}
