/**
 ******************************************************************************
 * @file    ass2_tasks.c
 * @brief   Assignment 2 reference solution: start-up, the task dispatcher and
 *          the HAL callbacks.
 *
 *          Write a task number (see ass2.h or the menu printed at start-up)
 *          into `task` in Live Expressions. The running task notices the
 *          change, returns, everything is switched off, and the dispatcher
 *          starts the new one. To restart a task, write 0 and its number.
 *
 *          An interrupt is on only during the tasks that use it; the
 *          callbacks below pass each event to the part that owns it.
 ******************************************************************************
 */
#include "main.h"
#include "ass2.h"
#include "io_board.h"
#include "sonar.h"
#include <stdio.h>

static void printMenu(void)
{
    printf("\r\nAssignment 2 solution. Write a number into `task` (Live Expressions):\r\n");
    printf("   0 idle\r\n");
    printf(" 111 A.1.1 polling PWM, LD2     112 A.1.2 the same, M = N/2\r\n");
    printf(" 121 A.2.1 TIM7 PWM, LD2        122 A.2.2 TIM7 PWM, wheel 1\r\n");
    printf(" 123 A.2.3 TIM7 at 1.7 MHz      124 A.2.4 1000 LED writes\r\n");
    printf(" 131 A.3.1 TIM2 duty sweep      132 A.3.2 PCA9634 PWM + passes\r\n");
    printf(" 210 B.1 ultrasonic raw\r\n");
    printf(" 221/222 B.2 IR, polling/interrupts  231/232 B.3 ultrasonic, polling/interrupts\r\n");
    printf(" 310 C.1 scan  320 C.2 LEDs  330 C.3 timing  340 C.4 buttons  350 C.5 servo\r\n");
    printf(" 411 D.1.1 by hand, TI1  412 D.1.2 by hand, TI1 and TI2  413 D.1.3 motor, both modes\r\n");
    printf(" 420 D.2 speed and wrap-around\r\n");
    printf(" 510 E.1 four wheels   521-524 E.2 step response of wheels 1-4\r\n\r\n");
}

/* everything off: wheels stopped, interrupts off, LEDs off */
static void allOff(void)
{
    HAL_TIM_Base_Stop_IT(&htim7);
    __HAL_TIM_SET_PRESCALER(&htim7, 169);        /* back to 10 kHz       */
    __HAL_TIM_SET_AUTORELOAD(&htim7, 99);
    HAL_TIM_Base_Stop_IT(&htim6);
    sonarStop();
    HAL_NVIC_DisableIRQ(EXTI9_5_IRQn);           /* IR interrupt off     */

    TIM2->CCR1 = 0;                              /* all 4 wheels stopped */
    TIM2->CCR2 = 0;
    TIM2->CCR3 = 0;
    TIM2->CCR4 = 0;
    for (uint8_t w = 0; w < 4u; w++)             /* the .ioc setting     */
    {
        encoderSetModeOf(wheels[w].enc, TIM_ENCODERMODE_TI1);
    }

    HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_RESET);
    allRgbOff();
}

void ass2Init(void)
{
    ioBoardInit();                               /* LED chips, buttons   */
    HAL_NVIC_DisableIRQ(EXTI9_5_IRQn);           /* enabled only in 222, 232 */

    /* the 4 motor PWM channels run from now on, at duty 0 */
    HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1);
    HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_2);
    HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_3);
    HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_4);
    allOff();                                    /* also starts the 4 encoders */
    printMenu();
}

/* run the selected task until `task` changes, then tidy up */
void ass2Run(void)
{
    uint16_t current = task;      /* a local copy for this pass */

    if (current != TASK_IDLE)
    {
        printf("=== task %u ===\r\n", current);
    }

    switch (current)
    {
        case TASK_IDLE: while (task == TASK_IDLE) { } break;   /* wait for a choice */

        case TASK_A1_1:
        case TASK_A1_2: partA1(current);  break;
        case TASK_A2_1:
        case TASK_A2_2:
        case TASK_A2_3: partA2(current);  break;
        case TASK_A2_4: partA2_4();       break;
        case TASK_A3_1: partA3_1();       break;
        case TASK_A3_2: partA3_2();       break;

        case TASK_B1:   partB1();         break;
        case TASK_B2_1:
        case TASK_B2_2:
        case TASK_B3_1:
        case TASK_B3_2: partB2B3(current); break;

        case TASK_C1:   partC1();         break;
        case TASK_C2:   partC2();         break;
        case TASK_C3:   partC3();         break;
        case TASK_C4:   partC4();         break;
        case TASK_C5:   partC5();         break;

        case TASK_D1_1:
        case TASK_D1_2:
        case TASK_D1_3: partD1(current);  break;
        case TASK_D2:   partD2();         break;

        case TASK_E1:   partE1();         break;
        case TASK_E2_1:
        case TASK_E2_2:
        case TASK_E2_3:
        case TASK_E2_4: partE2(current);  break;

        default:
            printf("no task %u\r\n", current);
            printMenu();
            while (task == current) { }
            break;
    }

    allOff();                     /* a known state for the next task */
}

/* ------------------------------------------------------------------ */
/* HAL callbacks: each interrupt goes to the part that started it      */
/* ------------------------------------------------------------------ */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM7)
    {
        partA2Tick();                            /* A.2 software PWM     */
    }
    else if (htim->Instance == TIM6)             /* 100 Hz samples       */
    {
        if (task == TASK_D2)
        {
            partD2Sample();
        }
        else
        {
            partE2Sample();
        }
    }
}

void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim)
{
    if (sonarCaptureEdge(htim))                  /* 1: a reading is ready */
    {
        partBSonarReading();
    }
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == IR_SENSOR_Pin)               /* B1 (PC13) is ignored */
    {
        partBIrEdge();
    }
}
