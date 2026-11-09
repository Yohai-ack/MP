/**
 ******************************************************************************
 * @file    car_params.c
 * @brief   Assignment 3 solution (TA): the parameters of every car we have
 *          measured. Add a car by adding one entry to carParams[].
 *
 *          How each car was measured (wheels in the air, 12 V supply):
 *          wheelMotor: each motor run alone (the same run as motorDir);
 *            Yohai says which wheel turned.
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
 *          servoZeroUs: the servo moved by hand commands (task 900) from
 *            1500 us, in steps of 50 us and then smaller, until the sonar
 *            looks straight ahead. The size of the scale (degrees per us) is
 *            the same on every car: SERVO_DEG_PER_US in ass3.h.
 *          servoDir: seen in the same search: +1 if a longer pulse turns the
 *            sonar to the left, -1 if to the right.
 *
 *          An entry whose wheels are not measured yet (Kp = 0), whose servoDir
 *          is not +1 or -1, or whose wheelMotor is not 1, 2, 3, 4 in some
 *          order, is refused by
 *          carParamsGet, so that car cannot drive with made-up values.
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
        .wheelMotor      = { 1, 2, 3, 4 },             /* measured 2026-11-07 */
        .servoZeroUs     = 1550,                       /* measured 2026-11-09 */
        .servoDir        = +1,                         /* measured 2026-11-09 */
    },

    /* car 11, measured 2026-11-09 (skl_prj_getCarParams). countsPerRev: 1320, 1320, 1320, 1320.
       (by hand, motors 1..4: 1313.7, 1316.0, 1317.3, 1321.0: hand error; the encoder gives 1320)
       Wiring unlike car 12: motor 3 is the front RIGHT wheel, motor 4 the front LEFT.
       Fit rms 0.39 to 0.67 rad/s (one count in 10 ms is 0.48 rad/s).
       Simulated with these gains: overshoot 1.000 / 1.000 / 1.000 / 1.000. */
    {
        .carId           = 11,
        .motorDir        = { +1, -1, -1, -1 },
        .encDir          = { -1, +1, -1, +1 },
        .countsPerRev    = { 1320.0f, 1320.0f, 1320.0f, 1320.0f },
        .K               = { 3.478f, 3.406f, 3.225f, 3.216f },
        .tau             = { 0.0890f, 0.1015f, 0.0790f, 0.1030f },
        .Kp              = { 0.800f, 0.931f, 0.766f, 1.001f },
        .Ki              = { 8.98f, 9.17f, 9.69f, 9.72f },
        .maxVelCmdNoSat  = { 12.14f, 10.55f, 12.52f, 9.83f },
        .designOvershoot = 1.0f,
        .designTauC      = 0.032f,
        .irObstacleLevel = 1,                          /* measured 2026-11-09 */
        .wheelMotor      = { 1, 2, 4, 3 },             /* measured 2026-11-09 */
        .servoZeroUs     = 1575,                       /* measured 2026-11-09 */
        .servoDir        = -1,                         /* measured 2026-11-09 */
    },
};

const CarParams *carParamsGet(uint8_t carId)
{
    for (size_t i = 0; i < sizeof carParams / sizeof carParams[0]; i++)
    {
        const CarParams *c = &carParams[i];
        if ((c->carId == carId) && (c->Kp[0] > 0.0f) &&        /* measured in full */
            ((c->servoDir == 1) || (c->servoDir == -1)))
        {
            uint8_t seen = 0;
            for (uint8_t p = 0; p < 4u; p++)
            {
                if ((c->wheelMotor[p] >= 1u) && (c->wheelMotor[p] <= 4u))
                {
                    seen |= (uint8_t)(1u << (c->wheelMotor[p] - 1u));
                }
            }
            return (seen == 0x0Fu) ? c : NULL;                  /* each motor once */
        }
    }
    return NULL;
}
