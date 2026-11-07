/**
 ******************************************************************************
 * @file    ass3_tasks.c
 * @brief   Assignment 3: the dispatcher, the buttons, the emergency stop and
 *          the HAL callbacks.
 *
 *          Idle (task = 0): buttons 0, 1 and 2 start A.3, the course, and the
 *          sending of the recorded data. A task number in `task` (cable
 *          connected) runs that task. Button 3 is read on every pass of every
 *          loop (keepGoing, at most every 10 ms): it stops the wheels at once
 *          and ends the program; until RESET only button 2 still works, so the
 *          data of the stopped run can be sent.
 ******************************************************************************
 */
#include "ass3.h"
#include "io_board.h"
#include "sonar.h"
#include <stdio.h>

#define KEYS_ADDR         0x71u     /* PCA9538A */
#define KEYS_INPUT        0x00u
#define BUTTON_PERIOD_MS  10u
#define I2C_ERRORS_STOP   3u        /* this many failed button reads in a row = emergency stop */

const CarParams *car = NULL;

static volatile uint8_t estopped = 0;
static uint8_t  btnPrev = 0;        /* buttons held at the last read (1 = pressed) */
static uint8_t  btnPending = 0;     /* new presses not yet taken by buttonsNew     */
static uint32_t btnTick = 0;
static uint8_t  i2cErrors = 0;

/* ------------------------------------------------------------------ */
/* emergency stop                                                      */
/* ------------------------------------------------------------------ */
static void emergencyStop(void)
{
    wheelsStop();                            /* every PWM duty to 0, TIM6 off */
    sonarOff();
    recStop();                               /* the data so far is kept      */
    estopped = 1;
    setStatusLed(255, 0, 0);
    printf("=== EMERGENCY STOP: only button 2 works until RESET ===\r\n");
}

uint8_t stopped(void)
{
    return estopped;
}

/* read the four buttons (1 = pressed); a bus error counts, and three in a
   row stop everything: a broken bus must not hide button 3              */
static void buttonsRead(void)
{
    uint8_t raw;

    if (HAL_I2C_Mem_Read(&hi2c1, (uint16_t)(KEYS_ADDR << 1), KEYS_INPUT, I2C_MEMADD_SIZE_8BIT, &raw, 1, 5) != HAL_OK)
    {
        if (++i2cErrors >= I2C_ERRORS_STOP && !estopped)
        {
            printf("button chip not answering\r\n");
            emergencyStop();
        }
        return;
    }
    i2cErrors = 0;

    uint8_t now = (uint8_t)(~raw) & 0x0Fu;  /* active-low, bits 0..3 */
    btnPending |= (uint8_t)(now & ~btnPrev);
    btnPrev = now;
    if ((now & BTN_STOP) && !estopped)
    {
        emergencyStop();
    }
}

uint8_t buttonsNew(void)
{
    buttonsRead();
    uint8_t b = btnPending;
    btnPending = 0;
    return b;
}

uint8_t keepGoing(uint16_t myTask)
{
    if (HAL_GetTick() - btnTick >= BUTTON_PERIOD_MS)
    {
        btnTick = HAL_GetTick();
        buttonsRead();
    }
    return !estopped && (task == myTask);
}

uint8_t waitGoing(uint16_t myTask, uint32_t ms)
{
    uint32_t start = HAL_GetTick();
    while (HAL_GetTick() - start < ms)
    {
        if (!keepGoing(myTask))
        {
            return 0;
        }
    }
    return 1;
}

/* ------------------------------------------------------------------ */
/* helpers                                                             */
/* ------------------------------------------------------------------ */
void setStatusLed(uint8_t r, uint8_t g, uint8_t b)
{
    setRGB(r, g, b);
}

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

static void idleLed(void)
{
    setStatusLed(0, 20, 0);                  /* dim green: idle, ready */
}

static void printMenu(void)
{
    printf("\r\nAssignment 3 solution, car %u, speed command %d.%02d rad/s\r\n", carId,
           (int)wheelsVCmd(), (int)(wheelsVCmd() * 100.0f) % 100);
    printf("cable:    task 120 A.2 step in the air, task 220 B.2 sensors\r\n");
    printf("buttons:  0 A.3 step on the floor, 1 course (after 5 s), 2 send data, 3 EMERGENCY STOP\r\n\r\n");
}

/* ------------------------------------------------------------------ */
/* start-up and the main loop                                          */
/* ------------------------------------------------------------------ */
void ass3Init(void)
{
    HAL_NVIC_DisableIRQ(EXTI9_5_IRQn);       /* the IR sensor is polled */
    car = carParamsGet(carId);
    if (car == NULL)
    {
        printf("\r\ncar %u is not in car_params.c: nothing runs\r\n", carId);
        while (1) { }                        /* motors were never started */
    }
    ioBoardInit();
    servoInit();
    wheelsInit();
    printMenu();
    recBootCheck();
    idleLed();
}

void ass3Run(void)
{
    if (estopped)
    {
        if (buttonsNew() & BTN_SEND)
        {
            recSend();
        }
        return;
    }

    uint16_t current = task;
    if (current == TASK_IDLE)
    {
        uint8_t b = buttonsNew();
        if (b & BTN_A3)
        {
            partA3();
        }
        else if (b & BTN_COURSE)
        {
            partB();
        }
        else if (b & BTN_SEND)
        {
            setStatusLed(0, 0, 60);
            recSend();
        }
        if (!estopped)
        {
            idleLed();
        }
        return;
    }

    printf("=== task %u ===\r\n", current);
    switch (current)
    {
        case TASK_A2: partA2(); break;
        case TASK_B2: partB2(); break;
        case TASK_CAL: calibrate(); break;
        default:      printf("no task %u\r\n", current); break;
    }
    wheelsStop();
    sonarOff();
    servoSetUs(servoAheadUs());
    if (!estopped)
    {
        idleLed();
        if (task == current)
        {
            printf("=== task %u done ===\r\n", current);
        }
    }
    while (keepGoing(current)) { }           /* wait until `task` changes */
}

/* ------------------------------------------------------------------ */
/* HAL callbacks                                                       */
/* ------------------------------------------------------------------ */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM6)
    {
        wheelsSample();                      /* 100 Hz: speeds, PI, record */
    }
}

void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim)
{
    if (sonarCaptureEdge(htim))              /* 1: a reading is ready */
    {
        sonarOnReading();
    }
}
