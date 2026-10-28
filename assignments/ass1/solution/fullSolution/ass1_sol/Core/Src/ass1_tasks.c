/**
 ******************************************************************************
 * @file    ass1_tasks.c
 * @brief   Assignment 1 reference solution: the task dispatcher.
 *
 *          Write a task number (see ass1.h or the menu printed at start-up)
 *          into `task` in Live Expressions. The running task notices the
 *          change, returns, and the dispatcher starts the new one. To
 *          restart the same task, write 0 and then its number again.
 ******************************************************************************
 */
#include "main.h"
#include "ass1.h"
#include "io_board.h"
#include <stdio.h>

volatile uint8_t task = 0;   /* the task that runs: change it in Live Expressions */

void printMenu(void)
{
    printf("\r\nAssignment 1 solution. Write a number into `task` (Live Expressions):\r\n");
    printf("  0 idle\r\n");
    printf(" 11 A.1 wrap-around     12 A.2 hex values     13 A.3 average\r\n");
    printf(" 21 B.1 bit functions   22 B.2 one LED walks  23 B.3 two LEDs walk\r\n");
    printf(" 31 C.1 buttons held    32 C.2 level count    33 C.3 edge count\r\n");
    printf(" 34 C.4 debounced count 35 C.5 debounced+delay 36 C.5 edge+delay\r\n");
    printf(" 41 D.1 200 toggles     42 D.2 LD2 rate       43 D.3 HAL_Delay\r\n");
    printf(" 44 D.3 HAL_GetTick\r\n");
    printf(" 51 E   armed / IR / brightening\r\n\r\n");
}

/* run the selected task until `task` changes, then tidy up */
void ass1Run(void)
{
    uint8_t current = task;

    if (current != TASK_IDLE)
    {
        printf("=== task %u ===\r\n", current);
    }

    switch (current)
    {
        case TASK_IDLE:         while (task == TASK_IDLE) { } break;  /* wait for a choice */
        case TASK_A1:           partA1();          break;
        case TASK_A2:           partA2();          break;
        case TASK_A3:           partA3();          break;
        case TASK_B1:           partB1();          break;
        case TASK_B2:           partB2();          break;
        case TASK_B3:           partB3();          break;
        case TASK_C1:           partC1();          break;
        case TASK_C2:           partC2();          break;
        case TASK_C3:           partC3();          break;
        case TASK_C4:           partC4();          break;
        case TASK_C5_DEBOUNCED: partC5Debounced(); break;
        case TASK_C5_EDGE:      partC5Edge();      break;
        case TASK_D1:           partD1();          break;
        case TASK_D2:           partD2();          break;
        case TASK_D3_DELAY:     partD3Delay();     break;
        case TASK_D3_TICK:      partD3Tick();      break;
        case TASK_E:            partE();           break;

        default:
            printf("no task %u\r\n", current);
            printMenu();
            while (task == current) { }
            break;
    }

    /* leave the board in a known state for the next task */
    allRgbOff();
    HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_RESET);
}
