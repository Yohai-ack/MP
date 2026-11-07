/**
 ******************************************************************************
 * @file    partB.c
 * @brief   Assignment 3, Part B: the obstacle course, and the sensor task.
 *
 *          The course (button 1), a state machine. The cart moves only in
 *          straight lines (forward, sideways), all wheels at +-vCmd, and
 *          changes motion only through a stop:
 *
 *            WAIT 5 s -> FORWARD --obstacle ahead--> stop, obstacle n
 *              n = 3: DONE
 *              else SCAN: the servo turns the sonar from one end to the other;
 *                the free side that needs the smaller sideways move wins;
 *                if the obstacle fills the whole servo range: PROBE (move a
 *                little to each side and scan again, further each time)
 *              -> SIDESTEP to the chosen lateral position
 *              -> PASS: forward, the sonar turned towards the obstacle, until
 *                 it is no longer seen, plus the length of the car
 *              -> RETURN sideways to the original line -> FORWARD
 *
 *          Sensor fusion (FORWARD): an obstacle is ahead if the IR sensor sees
 *          it OR the median of the last three sonar readings is within
 *          STOP_MM. The median ignores one false reading; the IR sensor is
 *          immediate and covers the time when no fresh sonar reading exists
 *          (a missed echo), and very short distances. During PASS the sonar
 *          looks sideways, so only the IR sensor watches ahead.
 *
 *          Task 220 (cable): prints the IR level and the sonar distance five
 *          times a second, to measure each sensor's range and reliability.
 ******************************************************************************
 */
#include "ass3.h"
#include <math.h>
#include <stdio.h>

enum
{
    ST_WAIT = 1, ST_FORWARD, ST_SCAN, ST_PROBE, ST_SIDESTEP, ST_PASS, ST_RETURN, ST_DONE, ST_ABORT
};

#define START_DELAY_MS  5000u
#define STOP_MM         80u        /* sonar median at or below this: stop (Yohai) */
#define FRESH_MS        300u       /* a sonar reading older than this is missing */
#define SETTLE_MS       400u       /* after every stop (about 12 tau_c)       */
#define SEEN_MM         450u       /* scan: the obstacle, if this near or nearer */
#define SIDE_SEEN_MM    400u       /* pass: the obstacle beside the cart      */
#define SIDE_MARGIN_MM  60.0f      /* between the car and the obstacle        */
#define PASS_EXTRA_MM   (CAR_LENGTH_MM + 50.0f)
#define PASS_MAX_MM     1500.0f    /* never longer: something is wrong        */
#define PROBE_MM        150.0f     /* first sideways probe; then 2x, 3x       */
#define PROBE_MAX       3
#define SCAN_N          13u        /* directions, over the calibrated servo range */

volatile uint8_t courseState = 0;

static uint32_t scanMm[SCAN_N];

/* the scan's directions: SCAN_N pulses evenly from car->servoUs[0] to [2] */
static uint16_t scanUs(uint8_t i)
{
    return (uint16_t)(car->servoUs[0] + (uint32_t)(car->servoUs[2] - car->servoUs[0]) * i / (SCAN_N - 1u));
}

/* +1 if a longer pulse turns the sonar to the left (from the calibration) */
static int8_t usLeft(void)
{
    return (car->servoDeg[2] > car->servoDeg[0]) ? 1 : -1;
}

/* the scan direction closest to straight ahead */
static uint8_t scanCentre(void)
{
    uint8_t best = 0;
    for (uint8_t i = 1; i < SCAN_N; i++)
    {
        if (fabsf(servoAngleDeg(scanUs(i))) < fabsf(servoAngleDeg(scanUs(best))))
        {
            best = i;
        }
    }
    return best;
}

/* the pulse that turns the sonar as far as it goes to side s (+1 left) */
static uint16_t sideUs(int8_t s)
{
    return (s * usLeft() > 0) ? scanUs(SCAN_N - 1u) : scanUs(0);
}

static void setState(uint8_t s)
{
    static const uint8_t rgb[][3] =
    {
        {0, 0, 0},    {60, 60, 0}, {0, 60, 0},  {0, 0, 60},  {30, 0, 60},
        {0, 60, 60},  {60, 0, 60}, {0, 60, 60}, {60, 60, 60}, {80, 20, 0}
    };
    courseState = s;
    setStatusLed(rgb[s][0], rgb[s][1], rgb[s][2]);
    printf("B: state %u at x=%d y=%d mm\r\n", s, (int)odoForwardMm(), (int)odoLeftMm());
}

/* sensor fusion, see the file header */
static uint8_t obstacleAhead(uint8_t sonarAhead)
{
    if (irObstacle())
    {
        return 1;
    }
    return sonarAhead && sonarFresh(FRESH_MS) && (sonarMedianMm() <= STOP_MM);
}

static uint8_t stopAndSettle(void)
{
    driveForward(0);
    return waitGoing(TASK_IDLE, SETTLE_MS);
}

/* wait for n new sonar readings (or the timeout); 0 if stopped */
static uint8_t waitReadings(uint8_t n, uint32_t timeoutMs)
{
    uint32_t c0 = sonarReadingCount();
    uint32_t start = HAL_GetTick();
    while ((sonarReadingCount() - c0 < n) && (HAL_GetTick() - start < timeoutMs))
    {
        if (!keepGoing(TASK_IDLE))
        {
            return 0;
        }
    }
    return 1;
}

/* look in 13 directions; the reading of each after the servo has turned */
static uint8_t scan(void)
{
    for (uint8_t i = 0; i < SCAN_N; i++)
    {
        servoSetUs(scanUs(i));
        if (!waitReadings(2, 400u))          /* the first may have started before the turn */
        {
            return 0;
        }
        scanMm[i] = sonarMm();
    }
    servoSetUs(servoAheadUs());
    printf("B: scan");
    for (uint8_t i = 0; i < SCAN_N; i++)
    {
        printf(" %lu", (unsigned long)scanMm[i]);
    }
    printf("\r\n");
    return waitReadings(3, 500u);            /* the median is from straight ahead again */
}

/* the sideways move (mm) that takes the car past the obstacle on side s,
   from the last scan; -1 if the obstacle is still seen at the end of the
   servo's range on that side                                             */
static float sideNeed(int8_t s)
{
    int8_t  step = (int8_t)(s * usLeft());   /* index direction towards side s */
    uint8_t c = scanCentre();
    float   lastMm = (scanMm[c] <= SEEN_MM) ? (float)scanMm[c] : (float)STOP_MM;

    for (int8_t i = (int8_t)(c + step); (i >= 0) && (i < (int8_t)SCAN_N); i = (int8_t)(i + step))
    {
        if (scanMm[i] > SEEN_MM)             /* the first free direction: the edge is before it */
        {
            float a = fabsf(servoAngleDeg(scanUs((uint8_t)i))) * (TWO_PI / 360.0f);
            return lastMm * sinf(a) + CAR_HALF_WIDTH_MM + SIDE_MARGIN_MM;
        }
        lastMm = (float)scanMm[i];
    }
    return -1.0f;
}

/* sideways to the lateral position y (mm, + left of the original line) */
static uint8_t strafeTo(float y)
{
    int8_t   dir = (y > odoLeftMm()) ? 1 : -1;
    float    mmPerS = wheelsVCmd() * WHEEL_RADIUS_M * 1000.0f;
    uint32_t limitMs = (uint32_t)(2000.0f * fabsf(y - odoLeftMm()) / mmPerS) + 2000u;
    uint32_t start = HAL_GetTick();

    driveSideways(dir);
    while ((dir > 0) ? (odoLeftMm() < y) : (odoLeftMm() > y))
    {
        if (!keepGoing(TASK_IDLE) || (HAL_GetTick() - start > limitMs))
        {
            break;
        }
    }
    return stopAndSettle();
}

/* choose the side and the lateral target; 1 chosen, 2 no way found, 0 stopped */
static uint8_t chooseSide(int8_t *side, float *targetY)
{
    if (!scan())
    {
        return 0;
    }
    float nl = sideNeed(+1);
    float nr = sideNeed(-1);
    if ((nl >= 0.0f) || (nr >= 0.0f))
    {
        *side = ((nr < 0.0f) || ((nl >= 0.0f) && (nl <= nr))) ? +1 : -1;
        *targetY = odoLeftMm() + (float)(*side) * ((*side > 0) ? nl : nr);
        return 1;
    }

    /* the obstacle fills the servo's whole range: move a little to each side,
       scan again, and go to the first side where its end is found          */
    setState(ST_PROBE);
    for (int8_t k = 1; k <= PROBE_MAX; k++)
    {
        for (int8_t s = 1; s >= -1; s = (int8_t)(s - 2))
        {
            if (!strafeTo((float)(k * s) * PROBE_MM) || !scan())
            {
                return 0;
            }
            float n = sideNeed(s);
            if (n >= 0.0f)
            {
                *side = s;
                *targetY = odoLeftMm() + (float)s * n;
                return 1;
            }
        }
    }
    return 2;
}

/* forward past the obstacle, which is on side -s; 1 passed, 2 something
   ahead, 0 stopped                                                       */
static uint8_t passObstacle(int8_t s)
{
    float    x0 = odoForwardMm();
    float    xClear = -1.0f;
    uint8_t  seen = 0, clearN = 0;
    uint32_t count = sonarReadingCount();

    servoSetUs(sideUs((int8_t)(-s)));        /* look at the obstacle */
    driveForward(+1);
    while (1)
    {
        if (!keepGoing(TASK_IDLE))
        {
            return 0;
        }
        if (obstacleAhead(0))                /* the sonar looks sideways: IR only */
        {
            stopAndSettle();
            return 2;
        }
        float x = odoForwardMm();
        if (sonarReadingCount() != count)    /* a new side reading */
        {
            count = sonarReadingCount();
            if (sonarMm() <= SIDE_SEEN_MM)
            {
                seen = 1;
                clearN = 0;
            }
            else if (seen || (x - x0 > CAR_LENGTH_MM))
            {
                if ((++clearN >= 3u) && (xClear < 0.0f))
                {
                    xClear = x;              /* the obstacle ends here */
                }
            }
        }
        if (((xClear >= 0.0f) && (x >= xClear + PASS_EXTRA_MM)) || (x - x0 > PASS_MAX_MM))
        {
            break;
        }
    }
    servoSetUs(servoAheadUs());
    return stopAndSettle();
}

void partB(void)
{
    uint8_t obstacles = 0;
    uint8_t end = ST_ABORT;
    int8_t  side = 1;
    float   targetY = 0.0f;

    odoReset();
    setState(ST_WAIT);
    servoSetUs(servoAheadUs());
    sonarOn();
    if (!waitGoing(TASK_IDLE, START_DELAY_MS))
    {
        goto finish;
    }
    wheelsStart();
    recStart(REC_B);

    while (1)
    {
        setState(ST_FORWARD);
        driveForward(+1);
        while (!obstacleAhead(1))
        {
            if (!keepGoing(TASK_IDLE))
            {
                goto finish;
            }
        }
        if (!stopAndSettle())
        {
            goto finish;
        }
        obstacles++;
        printf("B: obstacle %u, %lu mm ahead (IR %u)\r\n", obstacles, (unsigned long)sonarMedianMm(), irObstacle());
        if (obstacles == 3u)
        {
            end = ST_DONE;
            break;
        }

        setState(ST_SCAN);
        uint8_t r = chooseSide(&side, &targetY);
        if (r != 1u)
        {
            if (r == 2u)
            {
                printf("B: no free side found\r\n");
            }
            goto finish;
        }
        printf("B: going %s, to y=%d mm\r\n", (side > 0) ? "left" : "right", (int)targetY);

        setState(ST_SIDESTEP);
        if (!strafeTo(targetY))
        {
            goto finish;
        }
        setState(ST_PASS);
        r = passObstacle(side);
        if (r != 1u)
        {
            if (r == 2u)
            {
                printf("B: something ahead while passing\r\n");
            }
            goto finish;
        }
        setState(ST_RETURN);
        if (!strafeTo(0.0f) || !waitReadings(3, 500u))   /* back on the line, sonar ahead */
        {
            goto finish;
        }
    }

finish:
    wheelsStop();
    recStop();
    sonarOff();
    servoSetUs(servoAheadUs());
    if (!stopped())
    {
        setState(end);
        printf("B: end, %u obstacles, x=%d y=%d mm; data in RAM: button 2\r\n",
               obstacles, (int)odoForwardMm(), (int)odoLeftMm());
        (void)waitGoing(TASK_IDLE, 2000u);   /* show the end colour (button 3 still read) */
    }
}

void partB2(void)
{
    servoSetUs(servoAheadUs());
    sonarOn();
    while (waitGoing(TASK_B2, 200u))
    {
        printf("B2: ir=%u mm=%lu median=%lu fresh=%u\r\n", irObstacle(), (unsigned long)sonarMm(),
               (unsigned long)sonarMedianMm(), sonarFresh(FRESH_MS));
    }
    sonarOff();
}
