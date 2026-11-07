/**
 ******************************************************************************
 * @file    car_params.c
 * @brief   Assignment 3 solution (TA): the parameters of every car we have
 *          measured. Add a car by adding one entry to carParams[].
 *
 *          How each car was measured (wheels in the air, 12 V supply):
 *          motorDir, encDir: each motor run with its DIR pin = 1 at 30 % for
 *            1 s; the wheel's direction seen, the counter change read.
 *          countsPerRev: each wheel turned 3 turns by hand.
 *          K, tau: a step of 50 % (u = 6 V) for 2 s, the speed sampled at
 *            100 Hz, and w = K u (1 - exp(-t / tau)) fitted to it.
 *          Kp, Ki: the PI zero on the motor pole, Ki = Kp / tau, and
 *            Kp = tau / (K designTauC): the closed loop is then first order,
 *            with no overshoot and the time constant designTauC, the same for
 *            all four wheels, so they respond alike. Checked in a simulation
 *            of the real, linear loop (PI every 10 ms, speed = counter change
 *            over 10 ms, output held 10 ms, no voltage limit). Linear, so it
 *            holds for any step size up to maxVelCmdNoSat.
 *          maxVelCmdNoSat: from the same simulation; the peak output is in the
 *            first sample after the step, (Kp + Ki x 0.01 s) per rad/s, so the
 *            step may be at most 0.9 x 12 V / that.
 *          irObstacleLevel: the IR pin read with and without an object in
 *            front of the sensor (task 900).
 *          servoDeg: the servo set to 900, 1500 and 2100 us (task 900) and the
 *            sonar's direction at each read off by eye, + to the left.
 ******************************************************************************
 */
#include <stddef.h>
#include "car_params.h"

static const CarParams carParams[] =
{
    /* car 12, measured 2026-11-07 (skl_prj_getCarParams). countsPerRev: 1320, 1320, 1320, 1320.
       (by hand 1318, 1323, 1320, 1320: hand error; the encoder gives 1320 = 11 lines x 30:1 x 4 edges)
       Fit rms 0.45 to 0.54 rad/s (one count in 10 ms is 0.48 rad/s).
       Simulated with these gains: overshoot 1.000 / 1.000 / 1.000 / 1.000. */
    {
        .carId           = 12,
        .motorDir        = { -1, +1, +1, -1 },
        .encDir          = { +1, -1, -1, +1 },
        .countsPerRev    = { 1320.0f, 1320.0f, 1320.0f, 1320.0f },
        .K               = { 2.764f, 1.568f, 2.486f, 2.229f },
        .tau             = { 0.0815f, 0.0905f, 0.0865f, 0.1035f },
        .Kp              = { 0.921f, 1.803f, 1.087f, 1.451f },
        .Ki              = { 11.31f, 19.93f, 12.57f, 14.02f },
        .maxVelCmdNoSat  = { 10.43f, 5.39f, 8.90f, 6.78f },
        .designOvershoot = 1.0f,
        .designTauC      = 0.032f,
        .irObstacleLevel = 1,                          /* measured 2026-11-07 */
        .servoUs         = { 900, 1500, 2100 },
        .servoDeg        = { 62.0f, 5.0f, -62.0f },
    },
};

const CarParams *carParamsGet(uint8_t carId)
{
    for (size_t i = 0; i < sizeof carParams / sizeof carParams[0]; i++)
    {
        if (carParams[i].carId == carId)
        {
            return &carParams[i];
        }
    }
    return NULL;
}
