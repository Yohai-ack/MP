/**
 ******************************************************************************
 * @file    partE.c
 * @brief   Assignment 1, Part E: armed state, IR sensor, brightening LEDs.
 *
 *          condition = armed AND obstacle
 *
 *          OFF     -> RISING    when condition
 *          RISING  -> DONE      when full brightness was reached (LEDs off)
 *          RISING  -> FALLING   when !condition
 *          FALLING -> OFF       when the level reaches 0
 *          FALLING -> RISING    when condition (from the current level)
 *          DONE    -> OFF       when !condition
 *
 *          The loop never blocks: the button and the sensor are read on
 *          every pass, and the brightness steps are paced by HAL_GetTick.
 ******************************************************************************
 */
#include "main.h"
#include "ass1.h"
#include "bits.h"
#include "io_board.h"
#include <stdio.h>

typedef enum { E_OFF, E_RISING, E_DONE, E_FALLING } EState;

static const char *const stateName[] = { "OFF", "RISING", "DONE", "FALLING" };

volatile uint8_t  eArmed;                /* toggled by button 0         */
volatile uint8_t  eObstacle;             /* IR sensor sees an obstacle  */
volatile uint8_t  eCondition;            /* armed AND obstacle          */
volatile EState   eState;
volatile uint8_t  eLevel;                /* brightness 0 to 255         */
volatile uint32_t eStepMs   = 40;        /* 51 steps of 5: about 2 s    */
volatile uint8_t  eStepSize = 5;
volatile uint8_t  eUseGamma = 1;         /* 1: even steps to the eye    */

static void eShowLevel(void)
{
    uint8_t v = eUseGamma ? gamma2(eLevel) : eLevel;
    setRGB(v, v, v);
}

static void eSetState(EState s)
{
    if (s != eState)
    {
        eState = s;
        printf("armed %u  obstacle %u  -> %s (level %u)\r\n",
               eArmed, eObstacle, stateName[s], eLevel);
    }
}

void partE(void)
{
    Debouncer btn = { 0 };
    uint32_t  lastStep = HAL_GetTick();

    eArmed = 0;
    eLevel = 0;
    eState = E_OFF;
    eShowLevel();
    printf("E: disarmed. Button 0 arms and disarms.\r\n");

    while (task == TASK_E)
    {
        uint32_t now = HAL_GetTick();

        /* inputs, on every pass */
        if (debounceUpdate(&btn, bitTest(readButtonsHeld(), 0), now, 20))
        {
            eArmed = !eArmed;
            HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, eArmed ? GPIO_PIN_SET : GPIO_PIN_RESET);
            printf("%s\r\n", eArmed ? "armed (LD2 on)" : "disarmed");
        }
        eObstacle  = (HAL_GPIO_ReadPin(IR_SENSOR_GPIO_Port, IR_SENSOR_Pin) == GPIO_PIN_RESET);
        eCondition = eArmed && eObstacle;

        /* transitions that depend only on the condition */
        switch (eState)
        {
            case E_OFF:     if (eCondition)  { eSetState(E_RISING); lastStep = now; } break;
            case E_RISING:  if (!eCondition) { eSetState(E_FALLING); }                break;
            case E_FALLING: if (eCondition)  { eSetState(E_RISING); }                 break;
            case E_DONE:    if (!eCondition) { eSetState(E_OFF); }                    break;
        }

        /* one brightness step every eStepMs */
        if (now - lastStep >= eStepMs)
        {
            lastStep = now;

            if (eState == E_RISING)
            {
                if (eLevel == 255)              /* full was shown: off  */
                {
                    eLevel = 0;
                    eSetState(E_DONE);
                }
                else
                {
                    eLevel = (eLevel > 255 - eStepSize) ? 255 : eLevel + eStepSize;
                }
                eShowLevel();
            }
            else if (eState == E_FALLING)
            {
                eLevel = (eLevel < eStepSize) ? 0 : eLevel - eStepSize;
                if (eLevel == 0)
                {
                    eSetState(E_OFF);
                }
                eShowLevel();
            }
        }
    }
}
