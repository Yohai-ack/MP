/**
 ******************************************************************************
 * @file    car_params.h
 * @brief   Assignment 3 solution (TA): the parameters that differ from car to
 *          car. Every array of four has one element per MOTOR: [0] is motor 1.
 *
 *          Motors (the A2 table): 1 = TIM2_CH1, DIR1, encoder TIM1;
 *          2 = CH2, DIR2, TIM8; 3 = CH3, DIR3, TIM3; 4 = CH4, DIR4, TIM4.
 *          Which wheel each motor drives depends on the car's wiring:
 *          wheelMotor. Encoders in Encoder Mode TI1 and TI2.
 ******************************************************************************
 */
#ifndef CAR_PARAMS_H
#define CAR_PARAMS_H

#include <stdint.h>

typedef struct
{
    uint8_t carId;              /* the number written on the car             */

    /* wiring: the motor (1..4) that drives each wheel,
       [0] rear right, [1] rear left, [2] front left, [3] front right    */
    uint8_t wheelMotor[4];

    /* physics, per motor */
    int8_t  motorDir[4];        /* +1: DIR pin = 1 drives the wheel forward,
                                   -1: DIR pin = 1 drives it backward         */
    int8_t  encDir[4];          /* +1: the counter counts up when the wheel
                                   turns forward, -1: it counts down          */
    float   countsPerRev[4];    /* counts per wheel revolution (TI1 and TI2)  */
    float   K[4];               /* open-loop gain, rad/s per V (wheels in the
                                   air): w = K u (1 - exp(-t / tau))          */
    float   tau[4];             /* open-loop time constant, s                 */

    /* control: PI at 100 Hz, u = Kp e + Ki * (sum of e * 0.01 s), in V      */
    float   Kp[4];              /* V per rad/s                                */
    float   Ki[4];              /* V per rad                                  */
    float   maxVelCmdNoSat[4];  /* rad/s: the largest speed step from rest for
                                   which the PI output never goes beyond 90 %
                                   duty (0.9 x 12 V), transient included      */

    /* the goals Kp and Ki were designed for */
    float   designOvershoot;    /* largest peak / setpoint (1: none)          */
    float   designTauC;         /* closed-loop time constant, s (63 % of a
                                   step after tauC)                           */

    /* sensors */
    uint8_t  irObstacleLevel;   /* the IR pin (PA7) level that means an
                                   obstacle: 1 high, 0 low                    */
    uint16_t servoZeroUs;       /* the servo pulse, us, at which the sonar
                                   looks straight ahead (0 degrees); the
                                   scale, degrees per us, is the same on every
                                   car: SERVO_DEG_PER_US in ass3.h            */
    int8_t   servoDir;          /* +1 if a longer pulse turns the sonar to
                                   the LEFT, -1 if to the right (it differs
                                   between cars: the servo's mounting)        */
} CarParams;

/* the parameters of car `carId`, or NULL if it is not in car_params.c */
const CarParams *carParamsGet(uint8_t carId);

#endif /* CAR_PARAMS_H */
