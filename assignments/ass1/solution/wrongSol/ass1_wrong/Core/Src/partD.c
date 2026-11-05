/**
 ******************************************************************************
 * @file    partD.c
 * @brief   Assignment 1, Part D: LED blinking and timing.
 ******************************************************************************
 */
#include "main.h"
#include "ass1.h"
#include "io_board.h"
#include <stdio.h>

/* ---------------- D.1 --------------------------------------------- */
volatile uint32_t d1Toggles;             /* toggles done so far         */
volatile uint32_t d1TotalMs;             /* measured total time         */

/* ---------------- D.2 --------------------------------------------- */
volatile uint32_t blinkMs = 500;         /* LD2 on time = off time, ms  */

/* ---------------- D.3 --------------------------------------------- */
volatile uint32_t d3FreqMilliHz0 = 1000; /* LED 0: 1 Hz                 */
volatile uint32_t d3FreqMilliHz1 = 1300; /* LED 1: 1.3 Hz               */
volatile uint32_t d3Led0Ms;              /* last time for 10 blinks     */
volatile uint32_t d3Led1Ms;

/* D.1: toggle, print about 60 characters, wait 500 ms; 200 times */
void partD1(void)
{
    uint32_t start = HAL_GetTick();

    for (d1Toggles = 0; (d1Toggles < 200) && (task == TASK_D1); )
    {
        HAL_GPIO_TogglePin(LD2_GPIO_Port, LD2_Pin);
        d1Toggles++;
        /* about 58 to 60 characters + "\r\n" on the UART */
        printf("D1: toggle=%lu of 200, elapsed t=%6lu ms, LD2=%u, status OK\r\n",
               d1Toggles, HAL_GetTick() - start,
               (unsigned)HAL_GPIO_ReadPin(LD2_GPIO_Port, LD2_Pin));
        HAL_Delay(500);
    }
    d1TotalMs = HAL_GetTick() - start;
    printf("D1: toggles=%lu total=%lu\r\n", d1Toggles, d1TotalMs);
    taskDone(TASK_D1);
}

/* D.2: the blink rate follows blinkMs, read again before every toggle */
void partD2(void)
{
    while (task == TASK_D2)
    {
        HAL_GPIO_TogglePin(LD2_GPIO_Port, LD2_Pin);
        printf("D2: t=%lu LD2=%u\r\n", HAL_GetTick(),
               (unsigned)HAL_GPIO_ReadPin(LD2_GPIO_Port, LD2_Pin));
        HAL_Delay(blinkMs);
    }
}

/* ---------------- D.3: one blinking RGB LED ------------------------ */
typedef struct
{
    uint8_t  led;          /* RGB LED number                          */
    uint8_t  on;           /* current state                           */
    uint32_t toggles;      /* toggles since the measurement started   */
    uint32_t startTick;    /* HAL_GetTick() at toggle 1               */
} Blinker;

/* toggle the LED; toggle 1 starts blink 1, toggle 21 starts blink 11,
   so the time between them is exactly 10 blinks (10 periods)          */
static void blinkerToggle(Blinker *b, volatile uint32_t *result)
{
    uint32_t tick = HAL_GetTick();

    b->on = !b->on;
    setRGB_led(b->led, b->on ? LED_LEVEL : 0, b->on ? LED_LEVEL : 0, b->on ? LED_LEVEL : 0);

    if (b->toggles == 0)
    {
        b->startTick = tick;
    }
    b->toggles++;
    if (b->toggles == 21)
    {
        *result = tick - b->startTick;
        printf("D3: led=%u ms=%lu\r\n", b->led, *result);
        b->toggles = 1;                     /* this toggle starts the  */
        b->startTick = tick;                /* next measurement        */
    }
}

/* half period in microseconds: 1e6 us / (2 f), with f in mHz */
static uint32_t halfPeriodUs(uint32_t freqMilliHz)
{
    return 500000000UL / freqMilliHz;
}

/* D.3 with HAL_Delay: the obvious blocking version. Each LED toggles once
   per pass, and a pass lasts both delays, so the two rates are lost.    */
void partD3Delay(void)
{
    Blinker led0 = { .led = 0 };
    Blinker led1 = { .led = 1 };

    while (task == TASK_D3_DELAY)
    {
        blinkerToggle(&led0, &d3Led0Ms);
        HAL_Delay((halfPeriodUs(d3FreqMilliHz0) + 500) / 1000);   /* 500 ms */
        blinkerToggle(&led1, &d3Led1Ms);
        HAL_Delay((halfPeriodUs(d3FreqMilliHz1) + 500) / 1000);   /* 385 ms */
    }
}

/* D.3 with HAL_GetTick: each LED has its own deadline, and the loop never
   waits. The deadlines are kept in microseconds and advanced by exactly
   one half period, so 1.3 Hz (384.615 ms) does not drift by rounding.
   The differences are signed, so they stay right when the tick wraps.   */
void partD3Tick(void)
{
    Blinker  led0 = { .led = 0 };
    Blinker  led1 = { .led = 1 };
    uint32_t next0 = HAL_GetTick() * 1000u;  /* deadlines, in us        */
    uint32_t next1 = next0;

    while (task == TASK_D3_TICK)
    {
        uint32_t nowUs = HAL_GetTick() * 1000u;

        if ((int32_t)(nowUs - next0) >= 0)
        {
            blinkerToggle(&led0, &d3Led0Ms);
            next0 += halfPeriodUs(d3FreqMilliHz0);
        }
        if ((int32_t)(nowUs - next1) >= 0)
        {
            blinkerToggle(&led1, &d3Led1Ms);
            next1 += halfPeriodUs(d3FreqMilliHz1);
        }
    }
}
