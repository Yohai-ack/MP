/**
 ******************************************************************************
 * @file    partA.c
 * @brief   Assignment 3, Part A: the PI speed step of all four wheels.
 *
 *          A.2 (task 120, wheels in the air): from rest, all four setpoints
 *          step to vCmd; 2 s at 100 Hz are recorded (r, w1..w4, u1..u4) in the
 *          TIM6 interrupt, and printed only after the recording (A2: lines).
 *          A.3 (button 0, on the floor, no cable): the same, 1 s after the
 *          press; the data stays in RAM until button 2 (A3: lines).
 *
 *          vCmd = 0.9 x the smallest maxVelCmdNoSat of the car, so no wheel's
 *          PI saturates during the step: the response is the linear design.
 ******************************************************************************
 */
#include "ass3.h"
#include <stdio.h>

/* the step, recorded as `kind`; returns 0 if it was cut short */
static uint8_t stepRun(uint8_t kind, uint16_t myTask)
{
    const float v = wheelsVCmd();
    const float r[4] = {v, v, v, v};
    const float zero[4] = {0.0f, 0.0f, 0.0f, 0.0f};

    wheelsStart();                           /* setpoints 0, PI on, TIM6 on */
    __disable_irq();
    wheelsSetRef(r);                         /* the step ...                */
    recStart(kind);                          /* ... from the same sample on */
    __enable_irq();

    while (!recFull() && keepGoing(myTask)) { }

    recStop();
    wheelsSetRef(zero);                      /* brake under control, then off */
    uint8_t ok = waitGoing(myTask, 500u);
    wheelsStop();
    return ok && recFull();
}

void partA2(void)
{
    printf("A.2: step of all four wheels to %d.%02d rad/s, wheels in the air\r\n",
           (int)wheelsVCmd(), (int)(wheelsVCmd() * 100.0f) % 100);
    setStatusLed(0, 0, 60);
    if (stepRun(REC_A2, TASK_A2))
    {
        recSend();                           /* the A2: lines */
    }
}

void partA3(void)
{
    setStatusLed(60, 60, 0);                 /* yellow: about to move */
    if (!waitGoing(TASK_IDLE, 1000u))        /* time to take the hand away */
    {
        return;
    }
    setStatusLed(0, 0, 60);
    if (stepRun(REC_A3, TASK_IDLE))
    {
        printf("A.3 recorded: connect the cable and press button 2\r\n");
    }
}
