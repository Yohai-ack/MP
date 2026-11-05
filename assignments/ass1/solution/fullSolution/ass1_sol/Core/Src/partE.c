/**
 ******************************************************************************
 * @file    partE.c
 * @brief   Assignment 1, Part E: armed state, IR sensor, brightening LEDs.
 *
 *          condition = armed AND obstacle
 *
 *          OFF     -> RISING    when condition
 *          RISING  -> DONE      at the end of the brightening (LEDs off),
 *                               whatever the condition does meanwhile
 *          DONE    -> OFF       when !condition
 *
 *          So a new brightening needs the condition to become false and
 *          then true again after the previous one ended.
 *
 *          The loop never blocks: the button and the sensor are read on
 *          every pass (button 0 arms and disarms also during a brightening),
 *          and the brightness steps are paced by HAL_GetTick.
 ******************************************************************************
 */
#include "main.h"
#include "ass1.h"
#include "bits.h"
#include "io_board.h"
#include <stdio.h>

typedef enum { E_OFF, E_RISING, E_DONE } EState;

volatile uint8_t  eArmed;                /* toggled by button 0         */
volatile uint8_t  eObstacle;             /* IR sensor sees an obstacle  */
volatile uint8_t  eCondition;            /* armed AND obstacle          */
volatile EState   eState;
volatile uint8_t  eLevel;                /* brightness 0 to 255         */
volatile uint32_t eStepMs   = 40;        /* 51 steps of 5: about 2 s    */
volatile uint8_t  eStepSize = 5;
volatile uint8_t  eUseGamma = 1;         /* 1: even steps to the eye    */

/* the value given to setRGB for the current level */
static uint8_t eValue(void)
{
    return eUseGamma ? gamma2(eLevel) : eLevel;
}

static void eShowLevel(void)
{
    uint8_t v = eValue();
    setRGB(v, v, v);
}

/* the E: line, printed when armed or the obstacle changes, and when a
   brightening reaches full brightness; v = setRGB value               */
static void ePrintLine(void)
{
    printf("E: armed=%u obstacle=%u v=%u\r\n", eArmed, eObstacle, eValue());
}

void partE(void)
{
    Debouncer btn = { 0 };
    uint32_t  lastStep = HAL_GetTick();

    eArmed = 0;
    eLevel = 0;
    eState = E_OFF;
    eShowLevel();
    eObstacle = (HAL_GPIO_ReadPin(IR_SENSOR_GPIO_Port, IR_SENSOR_Pin) == IR_OBSTACLE_LEVEL);
    printf("Button 0 arms and disarms.\r\n");
    ePrintLine();

    while (task == TASK_E)
    {
        uint32_t now = HAL_GetTick();

        /* inputs, on every pass */
        if (debounceUpdate(&btn, bitTest(readButtonsHeld(), 0), now, 20))
        {
            eArmed = !eArmed;
            HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, eArmed ? GPIO_PIN_SET : GPIO_PIN_RESET);
            ePrintLine();
        }
        uint8_t obstacle = (HAL_GPIO_ReadPin(IR_SENSOR_GPIO_Port, IR_SENSOR_Pin) == IR_OBSTACLE_LEVEL);
        if (obstacle != eObstacle)
        {
            eObstacle = obstacle;
            ePrintLine();
        }
        eCondition = eArmed && eObstacle;

        /* transitions that depend only on the condition (none in RISING) */
        if ((eState == E_OFF) && eCondition)
        {
            eState = E_RISING;
            lastStep = now;
        }
        else if ((eState == E_DONE) && !eCondition)
        {
            eState = E_OFF;                     /* ready for the next one */
        }

        /* one brightness step every eStepMs, while rising */
        if ((eState == E_RISING) && (now - lastStep >= eStepMs))
        {
            lastStep = now;
            if (eLevel == 255)                  /* full was shown: off    */
            {
                eLevel = 0;
                eState = E_DONE;
            }
            else
            {
                eLevel = (eLevel > 255 - eStepSize) ? 255 : eLevel + eStepSize;
                if (eLevel == 255)
                {
                    ePrintLine();               /* full brightness        */
                }
            }
            eShowLevel();
        }
    }
}
