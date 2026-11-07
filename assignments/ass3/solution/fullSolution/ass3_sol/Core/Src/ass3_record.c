/**
 ******************************************************************************
 * @file    ass3_record.c
 * @brief   Assignment 3: the data of the last run, kept in RAM that the
 *          start-up code does not clear (.noinit, see STM32G474RETX_FLASH.ld).
 *          Plugging the USB cable back in resets the board (checked on the
 *          cart, 2026-11-07), but the RAM keeps its content while the cart is
 *          powered, so button 2 can still send the run.
 *
 *          A.2 / A.3: 200 samples (2 s at 100 Hz) of r, w1..w4, u1..u4 (float),
 *          7.2 KB. Part B: the counts of the four wheels in each 10 ms
 *          (int8_t: at most 127 counts in 10 ms, 60 rad/s, far above the
 *          speeds used) and the state, 5 bytes per sample: 12 000 samples
 *          (120 s) take 60 KB of the 128 KB of RAM.
 ******************************************************************************
 */
#include "ass3.h"
#include <stdio.h>

#define NOINIT     __attribute__((section(".noinit")))
#define REC_MAGIC  0xA3DA7A5Eu
#define REC_A_MAX  200u
#define REC_B_MAX  12000u

typedef struct
{
    uint32_t magic;          /* REC_MAGIC: the rest is valid        */
    uint8_t  kind;           /* REC_A2, REC_A3 or REC_B             */
    uint8_t  carId;
    uint16_t clipped;        /* B samples beyond +-127 counts       */
    uint32_t n;              /* samples stored                      */
} RecHead;

static RecHead head                     NOINIT;
static float   recA[REC_A_MAX][9]       NOINIT;   /* r, w1..w4, u1..u4 */
static int8_t  recB[REC_B_MAX][4]       NOINIT;   /* counts in 10 ms   */
static uint8_t recBState[REC_B_MAX]     NOINIT;

static volatile uint8_t active = REC_NONE;       /* normal RAM: 0 after a reset */

void recStart(uint8_t kind)
{
    active = REC_NONE;
    head.magic = 0;
    head.kind = kind;
    head.carId = carId;
    head.clipped = 0;
    head.n = 0;
    head.magic = REC_MAGIC;          /* valid from the first sample, so even a run
                                        cut by the emergency stop can be sent  */
    active = kind;
}

void recStop(void)
{
    active = REC_NONE;
}

uint8_t recFull(void)
{
    return head.n >= ((head.kind == REC_B) ? REC_B_MAX : REC_A_MAX);
}

/* TIM6 interrupt */
void recSampleStep(float r, const float w[4], const float u[4])
{
    if (((active != REC_A2) && (active != REC_A3)) || (head.n >= REC_A_MAX))
    {
        return;
    }
    float *s = recA[head.n];
    s[0] = r;
    for (uint8_t i = 0; i < 4u; i++)
    {
        s[1u + i] = w[i];
        s[5u + i] = u[i];
    }
    head.n++;
}

/* TIM6 interrupt */
void recSampleCourse(const int16_t dCounts[4], uint8_t state)
{
    if ((active != REC_B) || (head.n >= REC_B_MAX))
    {
        return;
    }
    for (uint8_t i = 0; i < 4u; i++)
    {
        int16_t d = dCounts[i];
        if (d > 127 || d < -127)
        {
            d = (d > 0) ? 127 : -127;
            head.clipped++;
        }
        recB[head.n][i] = (int8_t)d;
    }
    recBState[head.n] = state;
    head.n++;
}

/* the head is valid: after power-on the RAM holds random values */
static uint8_t recValid(void)
{
    if ((head.magic != REC_MAGIC) || (head.n == 0u))
    {
        return 0;
    }
    if (head.kind == REC_B)
    {
        return head.n <= REC_B_MAX;
    }
    return ((head.kind == REC_A2) || (head.kind == REC_A3)) && (head.n <= REC_A_MAX);
}

/* t = (k + 1) x 10 ms, printed as seconds with two decimals */
static void printTime(uint32_t k)
{
    printf("t=%lu.%02lu", (unsigned long)((k + 1u) / 100u), (unsigned long)((k + 1u) % 100u));
}

void recSend(void)
{
    if (!recValid())
    {
        printf("no recorded run in RAM\r\n");
        return;
    }
    printf("=== data: %s, %lu samples, car %u ===\r\n",
           (head.kind == REC_B) ? "course" : ((head.kind == REC_A3) ? "A.3" : "A.2"),
           (unsigned long)head.n, head.carId);

    if (head.kind == REC_B)
    {
        int32_t c[4] = {0, 0, 0, 0};
        for (uint32_t k = 0; k < head.n; k++)
        {
            for (uint8_t i = 0; i < 4u; i++)
            {
                c[i] += recB[k][i];
            }
            printf("B3: ");
            printTime(k);
            printf(" c1=%ld c2=%ld c3=%ld c4=%ld s=%u\r\n",
                   (long)c[0], (long)c[1], (long)c[2], (long)c[3], recBState[k]);
        }
        if (head.clipped > 0u)
        {
            printf("warning: %u samples clipped at 127 counts\r\n", head.clipped);
        }
    }
    else
    {
        const char *tag = (head.kind == REC_A3) ? "A3:" : "A2:";
        static const char *const names[9] = {"r", "w1", "w2", "w3", "w4", "u1", "u2", "u3", "u4"};
        for (uint32_t k = 0; k < head.n; k++)
        {
            printf("%s ", tag);
            printTime(k);
            for (uint8_t i = 0; i < 9u; i++)
            {
                printf(" %s=", names[i]);
                printFixed2(recA[k][i]);
            }
            printf("\r\n");
        }
    }
    printf("=== data end ===\r\n");
}

void recBootCheck(void)
{
    if (recValid())
    {
        printf("RAM holds a run (%s, %lu samples): press button 2 to send it\r\n",
               (head.kind == REC_B) ? "course" : "speed step", (unsigned long)head.n);
    }
    else
    {
        head.magic = 0;                /* after power-on: random content */
    }
}
