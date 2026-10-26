/**
 ******************************************************************************
 * @file    sonar.h
 * @brief   Raw reading of the HC-SR04 ultrasonic sensor (Assignment 2).
 *
 *          Hardware, as in ass2.ioc:
 *            TIM5 CH1, PWM   on PB2  = TRIG: a short pulse every 100 ms
 *            TIM5 CH2, input capture, both edges, on PC12 = ECHO
 *
 *          After every trigger pulse the sensor raises ECHO for a time that
 *          grows with the distance to the obstacle. TIM5 captures the timer
 *          count at both edges of ECHO; the difference is the raw reading.
 *
 *          The raw reading has NO physical unit. Find the factor that turns it
 *          into millimetres yourself, by measuring known distances.
 *
 *          Use ONE of the two modes:
 *            Polling   (Part B.1): sonarStartPolling(), then call sonarPoll()
 *                                  in the main loop.
 *            Interrupt (Part B.2): sonarStartInterrupt(), then call
 *                                  sonarCaptureEdge(htim) from your own
 *                                  HAL_TIM_IC_CaptureCallback().
 *
 * @course  Microprocessor-Based Product Design, Technion
 ******************************************************************************
 */
#ifndef SONAR_H
#define SONAR_H

#include "main.h"     /* HAL types, TIM_HandleTypeDef */
#include <stdint.h>

/* ECHO pin, read to tell a rising edge from a falling one */
#define SONAR_ECHO_PORT   GPIOC
#define SONAR_ECHO_PIN    GPIO_PIN_12

/* length of the trigger pulse, in TIM5 counts (about 10 us) */
#define SONAR_TRIG_COUNTS 170u

/* ------------------------------------------------------------------ */
/* Start: call ONE of these once, after the MX_..._Init() calls        */
/* ------------------------------------------------------------------ */
void     sonarStartPolling(void);    /* capture without interrupt      */
void     sonarStartInterrupt(void);  /* capture with the TIM5 interrupt */

/* ------------------------------------------------------------------ */
/* Edge handling: returns 1 when a new raw reading is ready, else 0    */
/* ------------------------------------------------------------------ */
uint8_t  sonarPoll(void);                         /* polling mode    */
uint8_t  sonarCaptureEdge(TIM_HandleTypeDef *htim); /* interrupt mode,
                                     call it from HAL_TIM_IC_CaptureCallback */

/* ------------------------------------------------------------------ */
/* Results                                                             */
/* ------------------------------------------------------------------ */
uint32_t sonarGetRaw(void);          /* last raw reading, no unit       */
uint32_t sonarGetReadingCount(void); /* readings completed since start  */
uint32_t sonarGetMissedCount(void);  /* readings lost: an edge came
                                        before the previous one was read */

#endif /* SONAR_H */
