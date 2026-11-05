/**
 ******************************************************************************
 * @file    partE.c
 * @brief   Assignment 2, Part E: the four wheels, step responses for MATLAB.
 *
 *          E.1 (510, our own design): the four encoders in TI1 and TI2, their
 *          counters printed when they change; every press of B1 runs the next
 *          wheel forward at 30 % for 1 s, to see which wheel it is and which
 *          way its counter goes.
 *          E.2 (521 to 524): the TIM6 interrupt (100 Hz) stores 200 speed
 *          samples (2 s) of one wheel in an array; only after the recording
 *          ends are they printed, one line per sample, for MATLAB.
 ******************************************************************************
 */
#include "main.h"
#include "ass2.h"
#include <stdio.h>

#define E_SAMPLES 200u
#define E_DUTY    50u                         /* our choice for E.2          */

/* ------------------------------------------------------------------ */
/* E.1: calibration of the four wheels                                 */
/* ------------------------------------------------------------------ */
void partE1(void)
{
    uint16_t last[4] = {0, 0, 0, 0};
    uint32_t lastPrint = HAL_GetTick();
    uint8_t  next = 0;                        /* the wheel B1 runs next      */
    uint32_t runStart = 0;
    int8_t   running = -1;                    /* the wheel that runs, or -1  */

    for (uint8_t w = 0; w < 4u; w++)
    {
        encoderSetModeOf(wheels[w].enc, TIM_ENCODERMODE_TI12);   /* CNT = 0 */
    }
    (void)b1Pressed();
    printf("E1: B1 runs the next wheel for 1 s\r\n");

    while (task == TASK_E1)
    {
        if ((running < 0) && b1Pressed())
        {
            running = (int8_t)next;
            next = (uint8_t)((next + 1u) % 4u);
            printf("E1: motor %d forward\r\n", running + 1);
            wheelRun((uint8_t)running, 30u);
            runStart = HAL_GetTick();
        }
        if ((running >= 0) && (HAL_GetTick() - runStart >= 1000u))
        {
            wheelRun((uint8_t)running, 0u);
            running = -1;
        }

        uint16_t cnt[4];
        uint8_t  changed = 0;
        for (uint8_t w = 0; w < 4u; w++)
        {
            cnt[w] = (uint16_t)wheels[w].enc->Instance->CNT;
            changed |= (cnt[w] != last[w]);
        }
        if (changed && (HAL_GetTick() - lastPrint >= 100u))
        {
            lastPrint = HAL_GetTick();
            printf("E1: enc1=%u enc2=%u enc3=%u enc4=%u\r\n", cnt[0], cnt[1], cnt[2], cnt[3]);
            for (uint8_t w = 0; w < 4u; w++)
            {
                last[w] = cnt[w];
            }
        }
    }
}

/* ------------------------------------------------------------------ */
/* E.2: the step response of one wheel                                 */
/* ------------------------------------------------------------------ */
static float             eSpeed[E_SAMPLES];  /* rad/s, written in the ISR */
static volatile uint16_t eCount = 0;          /* samples stored so far     */
static uint16_t          ePrev;               /* last counter reading      */
static uint8_t           eWheel;              /* 0..3                      */

void partE2Sample(void)
{
    if (eCount >= E_SAMPLES)
    {
        return;                               /* the recording is complete */
    }
    uint16_t now = (uint16_t)wheels[eWheel].enc->Instance->CNT;
    int16_t delta = (int16_t)(uint16_t)(now - ePrev);   /* as in D.2 */
    ePrev = now;
    eSpeed[eCount] = (float)(delta * wheels[eWheel].countSign) * 100.0f * TWO_PI / COUNTS_PER_REV;
    eCount++;
}

void partE2(uint16_t myTask)
{
    const uint8_t sub = (uint8_t)(myTask % 10u);         /* 1..4 = the wheel */

    eWheel = (uint8_t)(sub - 1u);
    encoderSetModeOf(wheels[eWheel].enc, TIM_ENCODERMODE_TI12);   /* as from D.2 on */

    /* the wheel must be at rest: wait 1 s with duty 0 */
    wheelRun(eWheel, 0u);
    if (!waitMs(myTask, 1000u))
    {
        return;
    }

    ePrev  = (uint16_t)wheels[eWheel].enc->Instance->CNT;
    eCount = 0;
    HAL_TIM_Base_Start_IT(&htim6);
    wheelRun(eWheel, E_DUTY);                             /* the step */

    while ((eCount < E_SAMPLES) && (task == myTask)) { }

    HAL_TIM_Base_Stop_IT(&htim6);
    wheelRun(eWheel, 0u);
    if (task != myTask)
    {
        return;
    }

    /* the recording is over: now there is time to print */
    for (uint16_t k = 0; k < E_SAMPLES; k++)
    {
        printf("E2.%u: t=", sub);
        printFixed2((float)(k + 1u) / 100.0f);           /* sample k at (k+1) * 10 ms */
        printf(" w=");
        printFixed2(eSpeed[k]);
        printf("\r\n");
    }
    taskDone(myTask);
}
