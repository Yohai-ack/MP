/**
 ******************************************************************************
 * @file    partD.c
 * @brief   Assignment 2, Part D: encoders and wheel speed.
 *
 *          D.1: the encoder mode of TIM1 is set at run time (no .ioc change);
 *               D.1.1 and D.1.2 by hand (TI1, TI1 and TI2), D.1.3 one motor
 *               run in each mode.
 *          D.2: in TI1 and TI2 (the better resolution), the 100 Hz TIM6
 *               interrupt computes the speed two ways; the main loop prints
 *               every sample.
 ******************************************************************************
 */
#include "main.h"
#include "ass2.h"
#include <stdio.h>

#define D1_RUN_MS  8000u            /* D.1.3: one run in each mode       */

/* ------------------------------------------------------------------ */
/* D.1.1, D.1.2: counts per revolution, wheel turned by hand (duty 0)   */
/* D.1.3: the same motion in both modes, wheel driven at 50 %           */
/* ------------------------------------------------------------------ */
static void partD1Hand(uint16_t myTask, uint32_t mode, uint8_t sub)
{
    uint16_t last = 0;
    uint32_t lastPrint = HAL_GetTick();

    encoderSetMode(mode);                    /* also sets the counter to 0 */
    printf("D1.%u: cnt=0\r\n", sub);
    while (task == myTask)
    {
        uint16_t cnt = (uint16_t)TIM1->CNT;
        if ((cnt != last) && (HAL_GetTick() - lastPrint >= 100u))
        {
            last = cnt;
            lastPrint = HAL_GetTick();
            printf("D1.%u: cnt=%u\r\n", sub, cnt);
        }
    }
}

/* the change of the counter in ms, summed pass by pass with the (int16_t)
   difference of D.2: at 50 % in TI1 and TI2, 8 s is about 35 000 counts,
   more than an int16_t holds, so the sum is kept in an int32_t        */
static uint8_t countFor(uint32_t ms, int32_t *change)
{
    uint32_t start = HAL_GetTick();
    uint16_t prev  = (uint16_t)TIM1->CNT;
    int32_t  sum   = 0;

    while (HAL_GetTick() - start < ms)
    {
        if (task != TASK_D1_3)
        {
            return 0;
        }
        uint16_t now = (uint16_t)TIM1->CNT;
        sum += (int16_t)(uint16_t)(now - prev);
        prev = now;
    }
    *change = sum;
    return 1;
}

static void partD1Factor(void)
{
    static const uint32_t modes[2] = {TIM_ENCODERMODE_TI1, TIM_ENCODERMODE_TI12};

    wheel1Forward(1);
    wheel1Duty(50);
    if (!waitMs(TASK_D1_3, 1000u))           /* let the speed settle */
    {
        return;
    }
    for (uint8_t i = 0; i < 2u; i++)
    {
        int32_t change;

        encoderSetMode(modes[i]);
        if (!countFor(D1_RUN_MS, &change))
        {
            return;
        }
        printf("D1.3: mode=%s counts=%ld\r\n", (modes[i] == TIM_ENCODERMODE_TI1) ? "TI1" : "TI12", (long)change);
    }
    wheel1Duty(0);
    encoderSetMode(TIM_ENCODERMODE_TI1);     /* the mode of ass2.ioc */
    taskDone(TASK_D1_3);
}

void partD1(uint16_t myTask)
{
    if (myTask == TASK_D1_1)
    {
        partD1Hand(myTask, TIM_ENCODERMODE_TI1, 1);
    }
    else if (myTask == TASK_D1_2)
    {
        partD1Hand(myTask, TIM_ENCODERMODE_TI12, 2);
    }
    else
    {
        partD1Factor();
    }
}

/* ------------------------------------------------------------------ */
/* D.2: speed and wrap-around                                          */
/* ------------------------------------------------------------------ */
static int32_t  prevNaive;          /* way 1: readings in int32_t       */
static uint16_t prevWrap;           /* way 2: readings in uint16_t      */
static volatile uint8_t  d2New = 0; /* a sample for the main loop       */
static volatile uint32_t d2Tick;
static volatile float    d2Naive, d2Wrap;
static volatile uint8_t  d2Wraps;    /* wraps seen since the task started */

/* counts per 10 ms -> rad/s, positive forward (E.1: wheel 1 counts down) */
static float toRadPerS(int32_t counts)
{
    return (float)(counts * wheels[0].countSign) * 100.0f * TWO_PI / COUNTS_PER_REV;
}

void partD2Sample(void)
{
    uint16_t now = (uint16_t)TIM1->CNT;

    int32_t naive = (int32_t)now - prevNaive;           /* jumps by 65 536 at a wrap */
    int16_t wrap  = (int16_t)(uint16_t)(now - prevWrap); /* modulo 2^16: always right */
    if ((naive > 32767) || (naive < -32767))           /* passed between 65 535 and 0 */
    {
        d2Wraps++;
    }
    prevNaive = (int32_t)now;
    prevWrap  = now;

    d2Naive = toRadPerS(naive);
    d2Wrap  = toRadPerS(wrap);
    d2Tick  = HAL_GetTick();
    d2New   = 1;
}

void partD2(void)
{
    uint32_t start;
    uint32_t lastWrap = 0;                  /* time of the second wrap */
    uint8_t  wrapsDone = 0;

    encoderSetMode(TIM_ENCODERMODE_TI12);   /* the better resolution of D.1; CNT = 0, so
                                               a wheel that counts down wraps at once,
                                               one that counts up after 65 536 counts */
    prevNaive = 0;
    prevWrap  = 0;
    d2Wraps = 0;
    d2New = 0;

    wheel1Forward(1);
    wheel1Duty(50);
    HAL_TIM_Base_Start_IT(&htim6);
    start = HAL_GetTick();

    while (task == TASK_D2)
    {
        if (!wrapsDone && (d2Wraps >= 2u))
        {
            wrapsDone = 1;                   /* then one more second */
            lastWrap = HAL_GetTick();
        }
        if (wrapsDone && (HAL_GetTick() - lastWrap >= 1000u))
        {
            break;
        }
        if (HAL_GetTick() - start >= 300000u)
        {
            printf("D.2: no second wrap in 5 min\r\n");   /* not a tagged line */
            break;
        }
        if (d2New)
        {
            d2New = 0;
            printf("D2: t=%lu naive=", (unsigned long)(d2Tick - start));
            printFixed2(d2Naive);
            printf(" wrap=");
            printFixed2(d2Wrap);
            printf("\r\n");
        }
    }

    HAL_TIM_Base_Stop_IT(&htim6);
    wheel1Duty(0);
    if (task == TASK_D2)
    {
        taskDone(TASK_D2);
    }
}
