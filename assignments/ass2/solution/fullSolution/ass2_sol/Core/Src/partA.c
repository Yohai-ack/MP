/**
 ******************************************************************************
 * @file    partA.c
 * @brief   Assignment 2, Part A: three ways to make a PWM.
 *
 *          N steps per period, of which the first M are on.
 *          A.1: polling, step = HAL_GetTick() (1 ms)
 *          A.2: interrupt, one step per TIM7 update (100 us)
 *          A.3: hardware, TIM2 counts the steps itself (N = 10 000, 17 kHz)
 ******************************************************************************
 */
#include "main.h"
#include "ass2.h"
#include "io_board.h"
#include <stdio.h>

volatile uint32_t pwmN = 100;   /* steps per PWM period: Live Expressions */
volatile uint32_t pwmM = 50;    /* steps that are on                       */

/* the A.1/A.2 status line, once a second */
static void printNM(const char *tag, uint32_t passes)
{
    printf("%s N=%lu M=%lu passes=%lu\r\n", tag, (unsigned long)pwmN, (unsigned long)pwmM,
           (unsigned long)passes);
}

/* ------------------------------------------------------------------ */
/* A.1: polling. The step counter is the 1 ms tick, so f_PWM = 1000/N  */
/* ------------------------------------------------------------------ */
void partA1(uint16_t myTask)
{
    const char *tag = (myTask == TASK_A1_1) ? "A1.1:" : "A1.2:";
    PassCounter pc;
    uint32_t passes;

    passStart(&pc);
    while (task == myTask)
    {
        uint32_t n = (pwmN == 0) ? 1 : pwmN;         /* never divide by 0 */
        if (myTask == TASK_A1_2)
        {
            pwmM = n / 2;                            /* A.1.2: 50 % kept  */
        }
        uint32_t step = HAL_GetTick() % n;           /* 0 .. N-1          */
        HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, (step < pwmM) ? GPIO_PIN_SET : GPIO_PIN_RESET);

        if (passCount(&pc, &passes))
        {
            printNM(tag, passes);
        }
    }
}

/* ------------------------------------------------------------------ */
/* A.2: interrupt. TIM7 calls partA2Tick() every t_step               */
/* ------------------------------------------------------------------ */
static volatile uint16_t a2Task  = 0;    /* the task that owns TIM7     */
static volatile uint32_t a2Step  = 0;    /* 0 .. N-1                    */

void partA2Tick(void)
{
    if (task != a2Task)
    {
        /* the task was changed: the main loop may hardly run at 1.7 MHz,
           so the callback itself returns TIM7 to 10 kHz and stops it     */
        __HAL_TIM_SET_PRESCALER(&htim7, 169);
        __HAL_TIM_SET_AUTORELOAD(&htim7, 99);
        HAL_TIM_Base_Stop_IT(&htim7);
        TIM2->CCR1 = 0;
        return;
    }

    if (++a2Step >= pwmN)
    {
        a2Step = 0;
    }
    uint8_t on = (a2Step < pwmM);

    if (a2Task == TASK_A2_1)
    {
        LD2_GPIO_Port->BSRR = on ? LD2_Pin : ((uint32_t)LD2_Pin << 16);   /* set / reset */
    }
    else
    {
        TIM2->CCR1 = on ? PWM_FULL : 0;          /* wheel 1 fully on or off */
    }
}

void partA2(uint16_t myTask)
{
    const char *tag = (myTask == TASK_A2_1) ? "A2.1:" : (myTask == TASK_A2_2) ? "A2.2:" : "A2.3:";
    PassCounter pc;
    uint32_t passes;
    uint32_t start = HAL_GetTick();
    uint8_t fast = 0;

    wheel1Forward(1);
    a2Task = myTask;
    a2Step = 0;
    HAL_TIM_Base_Start_IT(&htim7);
    passStart(&pc);

    while (task == myTask)
    {
        if ((myTask == TASK_A2_3) && !fast && (HAL_GetTick() - start >= 2000u))
        {
            /* 17 kHz with N = 100: 1.7 MHz steps, so 170 MHz / 1.7 MHz = 100
               timer clocks per step: PSC = 0, ARR = 99                    */
            __HAL_TIM_SET_PRESCALER(&htim7, 0);
            __HAL_TIM_SET_AUTORELOAD(&htim7, 99);
            printf("A2.3: TIM7 PSC=0 ARR=99\r\n");
            fast = 1;
        }
        if (passCount(&pc, &passes))
        {
            printNM(tag, passes);
        }
    }
    a2Task = 0;
}

/* A.2.4: one RGB LED update is one I2C write, about 80 us at 400 kHz */
void partA2_4(void)
{
    uint32_t start = HAL_GetTick();
    for (uint16_t i = 0; i < 1000u; i++)
    {
        setLedChannel(PCA_RED, 0, (i & 1u) ? 40 : 0);
    }
    uint32_t ms = HAL_GetTick() - start;
    printf("A2.4: calls=1000 ms=%lu\r\n", (unsigned long)ms);
    taskDone(TASK_A2_4);
}

/* ------------------------------------------------------------------ */
/* A.3: hardware PWM on TIM2: the CPU only writes CCR1                 */
/* ------------------------------------------------------------------ */
void partA3_1(void)
{
    wheel1Forward(1);
    for (uint32_t duty = 0; duty <= 100u; duty += 5u)
    {
        wheel1Duty(duty);
        printf("A3.1: duty=%lu CCR1=%lu\r\n", (unsigned long)duty, (unsigned long)TIM2->CCR1);
        if (!waitMs(TASK_A3_1, 2000u))
        {
            return;                              /* stopped by a new task */
        }
    }
    wheel1Duty(0);
    taskDone(TASK_A3_1);
}

void partA3_2(void)
{
    PassCounter pc;
    uint32_t passes;

    setLedChannel(PCA_GREEN, 0, 64);             /* once: the chip keeps the PWM */
    wheel1Forward(1);
    wheel1Duty(50);
    passStart(&pc);
    while (task == TASK_A3_2)
    {
        if (passCount(&pc, &passes))
        {
            printf("A3.2: passes=%lu\r\n", (unsigned long)passes);
        }
    }
}
