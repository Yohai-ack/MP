/**
 ******************************************************************************
 * @file    ass2_common.c
 * @brief   Assignment 2 reference solution: helpers shared by several parts.
 ******************************************************************************
 */
#include "main.h"
#include "ass2.h"
#include <stdio.h>

/* the end of a task that has an end: report it, then wait for a new choice
   (a task stopped early by a new choice is not reported as done)           */
void taskDone(uint16_t myTask)
{
    if (task == myTask)
    {
        printf("=== task %u done ===\r\n", myTask);
    }
    while (task == myTask) { }
}

/* wait ms milliseconds without missing a change of `task`;
   returns 1 after the full time, 0 as soon as the task was changed */
uint8_t waitMs(uint16_t myTask, uint32_t ms)
{
    uint32_t start = HAL_GetTick();
    while (HAL_GetTick() - start < ms)
    {
        if (task != myTask)
        {
            return 0;
        }
    }
    return 1;
}

/* print a value with two decimals (12.34, -0.50) without the float
   support of printf: whole part and hundredths as two integers         */
void printFixed2(float value)
{
    int32_t hundredths = (int32_t)(value * 100.0f + (value < 0 ? -0.5f : 0.5f));
    if (hundredths < 0)
    {
        putchar('-');
        hundredths = -hundredths;
    }
    printf("%ld.%02ld", (long)(hundredths / 100), (long)(hundredths % 100));
}

/* print "0b" and the lowest nBits of value, MSB first */
void printBinary(uint32_t value, uint8_t nBits)
{
    printf("0b");
    for (int8_t i = (int8_t)(nBits - 1); i >= 0; i--)
    {
        putchar(((value >> i) & 1u) ? '1' : '0');
    }
}

void passStart(PassCounter *p)
{
    p->passes = 0;
    p->startTick = HAL_GetTick();
}

/* one main-loop pass; once a second, return 1 with the count in *n */
uint8_t passCount(PassCounter *p, uint32_t *n)
{
    p->passes++;
    if (HAL_GetTick() - p->startTick >= 1000u)
    {
        *n = p->passes;
        p->passes = 0;
        p->startTick += 1000u;
        return 1;
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/* wheel 1: TIM2 CH1 (PA0) sets the speed, DIR1 (PC2) the direction    */
/* ------------------------------------------------------------------ */
void wheel1Forward(uint8_t forward)
{
    /* SET is forward for motors 1 and 4 (the two sides are mirrored) */
    HAL_GPIO_WritePin(DIR1_GPIO_Port, DIR1_Pin, forward ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

void wheel1Duty(uint32_t percent)
{
    TIM2->CCR1 = percent * PWM_FULL / 100u;      /* duty = CCR / 10 000 */
}

/* D.1: switch the encoder mode of an encoder timer while the program runs */
void encoderSetModeOf(TIM_HandleTypeDef *htim, uint32_t mode)
{
    HAL_TIM_Encoder_Stop(htim, TIM_CHANNEL_ALL);
    htim->Instance->SMCR = (htim->Instance->SMCR & ~TIM_SMCR_SMS) | mode;
    htim->Instance->CNT  = 0;
    HAL_TIM_Encoder_Start(htim, TIM_CHANNEL_ALL);
}

void encoderSetMode(uint32_t mode)
{
    encoderSetModeOf(&htim1, mode);
}

/* ------------------------------------------------------------------ */
/* the four wheels, as found in E.1 on our cart                        */
/* ------------------------------------------------------------------ */
const Wheel wheels[4] =
{
    /* the DIR levels and signs measured in E.1 on our cart (the second cart, 2026-11-05):
       forward is SET for motor 1 and RESET for motors 2, 3 and 4 (motor 4 differs from
       the configuration section, which says SET: check your cart)                  */
    {&TIM2->CCR1, DIR1_GPIO_Port, DIR1_Pin, GPIO_PIN_SET,   &htim1, -1},   /* counts down forward */
    {&TIM2->CCR2, DIR2_GPIO_Port, DIR2_Pin, GPIO_PIN_RESET, &htim8, +1},   /* counts up forward   */
    {&TIM2->CCR3, DIR3_GPIO_Port, DIR3_Pin, GPIO_PIN_RESET, &htim3, -1},   /* counts down forward */
    {&TIM2->CCR4, DIR4_GPIO_Port, DIR4_Pin, GPIO_PIN_RESET, &htim4, +1},   /* counts up forward   */
};

/* run wheel w (0..3) forward at percent duty (0 stops it) */
void wheelRun(uint8_t w, uint32_t percent)
{
    HAL_GPIO_WritePin(wheels[w].dirPort, wheels[w].dirPin, wheels[w].forwardLevel);
    *wheels[w].ccr = percent * PWM_FULL / 100u;
}

/* B1 (PC13) reads HIGH while pressed; 1 once per press, 0 otherwise */
uint8_t b1Pressed(void)
{
    static uint8_t before = 0;
    uint8_t now = (HAL_GPIO_ReadPin(B1_GPIO_Port, B1_Pin) == GPIO_PIN_SET);
    uint8_t press = now && !before;
    before = now;
    return press;
}
