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

#define A1_START_U8    250u              /* the start values and steps  */
#define A1_START_I8    120               /* the instructions prescribe: */
#define A1_START_U16   65530u            /* u8 and u16 wrap at step 6,  */
#define A1_STEPS       12u               /* i8 at step 8                */
#define A1_STEP_MS     500u

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

/* one line of A.1: decimal, hex and binary of the bits stored in each
   variable (the cast to uint8_t keeps the 8 bits of the int8_t)        */
static void a1PrintLine(void)
{
    printf("A1: u8=%u 0x%02X ", a1U8, a1U8);
    printBinary(a1U8, 8);
    printf(" i8=%d 0x%02X ", a1I8, (uint8_t)a1I8);
    printBinary((uint8_t)a1I8, 8);
    printf(" u16=%u 0x%04X ", a1U16, a1U16);
    printBinary(a1U16, 16);
    printf("\r\n");
}

void partA1(void)
{
    a1U8  = A1_START_U8;
    a1I8  = A1_START_I8;
    a1U16 = A1_START_U16;
    a1PrintLine();

    for (uint8_t step = 0; (step < A1_STEPS) && (task == TASK_A1); step++)
    {
        HAL_Delay(A1_STEP_MS);
        a1U8++;           /* 255 + 1 = 0                                  */
        a1I8++;           /* 127 + 1: the bit pattern 0x80 reads as -128 */
        a1U16++;          /* 65535 + 1 = 0                                */
        a1PrintLine();
    }
    taskDone(TASK_A1);
}

/* one line of A.2: hex and bin are the stored bits; raw is the variable
   given to %X as it is, so a negative value shows its sign extension to
   32 bits (0xF0 as int8_t gives raw=0xFFFFFFF0)                         */
static void a2PrintLine(const char *hexLiteral, const char *type, int32_t value,
                        uint16_t bits, uint8_t nBits)
{
    printf("A2: %s %s dec=%ld hex=0x%0*X bin=", hexLiteral, type, value, nBits / 4, bits);
    printBinary(bits, nBits);
    printf(" raw=0x%lX\r\n", (uint32_t)value);
}

void partA2(void)
{
    a2PrintLine("0x7F",   "uint8_t",  a2U8_7F, a2U8_7F,          8);
    a2PrintLine("0x7F",   "int8_t",   a2I8_7F, (uint8_t)a2I8_7F, 8);
    a2PrintLine("0xF0",   "uint8_t",  a2U8_F0, a2U8_F0,          8);
    a2PrintLine("0xF0",   "int8_t",   a2I8_F0, (uint8_t)a2I8_F0, 8);
    a2PrintLine("0xFFF6", "uint16_t", a2U16,   a2U16,            16);
    a2PrintLine("0xFFF6", "int16_t",  a2I16,   (uint16_t)a2I16,  16);
    taskDone(TASK_A2);
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
    printf("A3: wrong=%u right=%lu.%lu\r\n", a3AvgWrong, tenths / 10, tenths % 10);
    taskDone(TASK_A3);
}
