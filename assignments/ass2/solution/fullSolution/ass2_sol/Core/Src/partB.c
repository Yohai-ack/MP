/**
 ******************************************************************************
 * @file    partB.c
 * @brief   Assignment 2, Part B: polling vs. interrupts.
 *
 *          B.1: ultrasonic calibration (raw readings and their averages).
 *          B.2 / B.3: wheel 1 at 40 % stops on an obstacle and restarts after
 *          the sensor was clear for 1 s. B.2 uses only the IR sensor, B.3 only
 *          the ultrasonic one (below 150 mm), each by polling (x.1) and by
 *          interrupts (x.2, the callback stops the wheel). LEDs and the status
 *          line change only with the status; every pass of the main loop also
 *          calls HAL_Delay(150). B1 ends the task.
 ******************************************************************************
 */
#include "main.h"
#include "ass2.h"
#include "io_board.h"
#include "sonar.h"
#include <stdio.h>

/* TIM5 counts at 170 MHz / 10 = 17 MHz. Round trip at 343 m/s:
   raw = 2 d / 343 000 mm/s * 17e6 /s = 99.1 counts per mm (theory;
   replace with the factor measured in B.1)                             */
#define RAW_PER_MM_X10   1019u      /* B.1 on our cart (2026-11-04): fit of 5, 10, 15 cm (theory 991) */
#define NEAR_MM          150u
#define NEAR_RAW         (NEAR_MM * RAW_PER_MM_X10 / 10u)   /* 15 285 */
#define RUN_DUTY         40u
#define CLEAR_MS         1000u
#define WORK_MS          150u

/* shared with the callbacks, so volatile */
static volatile uint8_t  bTask       = 0;   /* the B task that runs          */
static volatile uint8_t  newReading  = 0;   /* B.1: a reading is ready       */
static volatile uint8_t  irObstacle  = 0;   /* IR sees an obstacle           */
static volatile uint8_t  sonarNear   = 0;   /* last reading below 150 mm     */
static volatile uint8_t  wheelOn     = 0;   /* wheel 1 is turning            */
static volatile uint32_t stopCount   = 0;   /* stops since the task started  */
static volatile uint32_t lastSeen    = 0;   /* tick of the last obstacle     */

/* stop the wheel (from the main loop with polling, from a callback with interrupts) */
static void stopWheel(void)
{
    TIM2->CCR1 = 0;                          /* the only work done in an ISR */
    if (wheelOn)
    {
        wheelOn = 0;
        stopCount++;
    }
}

/* ------------------------------------------------------------------ */
/* callbacks (B.1, 222 and 232 only: the interrupts are off in 221, 231) */
/* ------------------------------------------------------------------ */
void partBSonarReading(void)
{
    newReading = 1;
    if (bTask == 3)
    {
        sonarNear = (sonarGetRaw() < NEAR_RAW);
        if (sonarNear)
        {
            stopWheel();
            lastSeen = HAL_GetTick();
        }
    }
}

void partBIrEdge(void)
{
    /* both edges: LOW = obstacle appeared, HIGH = it left */
    irObstacle = (HAL_GPIO_ReadPin(IR_SENSOR_GPIO_Port, IR_SENSOR_Pin) == IR_OBSTACLE_LEVEL);
    if (irObstacle)
    {
        stopWheel();
    }
    lastSeen = HAL_GetTick();                /* "clear" counts from now      */
}

/* ------------------------------------------------------------------ */
/* B.1: calibration                                                    */
/* ------------------------------------------------------------------ */
void partB1(void)
{
    uint32_t sum = 0;
    uint8_t  n = 0;

    bTask = 1;
    newReading = 0;
    sonarStartInterrupt();
    while (task == TASK_B1)
    {
        if (newReading)
        {
            newReading = 0;
            uint32_t raw = sonarGetRaw();
            printf("B1: raw=%lu\r\n", (unsigned long)raw);
            sum += raw;
            if (++n == 10u)
            {
                printf("B1: avg=%lu\r\n", (unsigned long)(sum / 10u));
                sum = 0;
                n = 0;
            }
        }
    }
    bTask = 0;
}


/* ------------------------------------------------------------------ */
/* B.2 (IR hand passes) and B.3 (ultrasonic hand passes): the same     */
/* control in all four tasks, with one sensor each, by polling (x.1)   */
/* or interrupts (x.2)                                                 */
/* ------------------------------------------------------------------ */
void partB2B3(uint16_t myTask)
{
    const uint8_t useIrq = ((myTask % 10u) == 2u);       /* 222, 232          */
    const uint8_t item   = (uint8_t)((myTask / 10u) % 10u);   /* 2 or 3       */
    const uint8_t sub    = (uint8_t)(myTask % 10u);       /* 1 or 2            */
    const uint8_t useIr  = (item == 2u);                  /* B.2: IR, B.3: ultrasonic */
    uint32_t start       = HAL_GetTick();
    uint32_t printedStops = 0;
    uint32_t periodStart = start;
    uint32_t passes      = 0;
    uint32_t period      = 0;                             /* ms, last second   */
    uint8_t  shown       = 0xFF;                          /* last status shown */

    bTask      = useIrq ? 3 : 2;                          /* 3: callbacks act  */
    stopCount  = 0;
    sonarNear  = 0;                                       /* the unused sensor */
    irObstacle = 0;                                       /* stays 0           */
    lastSeen   = start;

    if (useIr)
    {
        irObstacle = (HAL_GPIO_ReadPin(IR_SENSOR_GPIO_Port, IR_SENSOR_Pin) == IR_OBSTACLE_LEVEL);
        if (useIrq)
        {
            __HAL_GPIO_EXTI_CLEAR_IT(IR_SENSOR_Pin);       /* an old edge */
            HAL_NVIC_EnableIRQ(EXTI9_5_IRQn);
        }
    }
    else if (useIrq)
    {
        sonarStartInterrupt();
    }
    else
    {
        sonarStartPolling();                               /* no TIM5 IRQ */
    }
    wheel1Forward(1);
    wheelOn = 0;
    (void)b1Pressed();                                     /* forget an old press */

    while (task == myTask)
    {
        uint32_t now = HAL_GetTick();

        /* --- sensors: polled once per pass in x.1 --- */
        if (!useIrq)
        {
            if (useIr)
            {
                irObstacle = (HAL_GPIO_ReadPin(IR_SENSOR_GPIO_Port, IR_SENSOR_Pin) == IR_OBSTACLE_LEVEL);
            }
            else if (sonarPoll())
            {
                sonarNear = (sonarGetRaw() < NEAR_RAW);
            }
            if (irObstacle || sonarNear)
            {
                stopWheel();
            }
        }

        /* --- restart after 1 s with the sensor clear (the other one stays 0) --- */
        if (irObstacle || sonarNear)
        {
            lastSeen = now;
        }
        else if (!wheelOn && (now - lastSeen >= CLEAR_MS))
        {
            wheelOn = 1;
            wheel1Duty(RUN_DUTY);
        }

        /* --- LEDs and status line, only when the status changes --- */
        uint8_t status = (uint8_t)((wheelOn << 2) | (irObstacle << 1) | sonarNear);
        if (status != shown)
        {
            if ((shown == 0xFF) || ((status ^ shown) & 0x04u))
            {
                if (wheelOn) setRGB(0, 60, 0);             /* green: turns */
                else         setRGB(60, 0, 0);             /* red: stopped */
            }
            shown = status;
            if (useIr) printf("status wheel=%u ir=%u\r\n", wheelOn, irObstacle);
            else       printf("status wheel=%u near=%u\r\n", wheelOn, sonarNear);
        }

        /* --- the other work of the program --- */
        HAL_Delay(WORK_MS);

        /* --- the loop period, averaged over each second --- */
        passes++;
        uint32_t elapsed = HAL_GetTick() - periodStart;
        if (elapsed >= 1000u)
        {
            period = elapsed / passes;
            periodStart += elapsed;
            passes = 0;
        }

        /* --- a result line at every stop --- */
        if (stopCount != printedStops)
        {
            printedStops = stopCount;
            printf("B%u.%u: stops=%lu period=%lu\r\n", item, sub, (unsigned long)printedStops,
                   (unsigned long)(period ? period : (HAL_GetTick() - start) / passes));
        }

        /* --- B1 ends the task --- */
        if (b1Pressed())
        {
            break;
        }
    }

    TIM2->CCR1 = 0;
    bTask = 0;
    if (task != myTask)
    {
        return;                                            /* left early      */
    }
    if (item == 3u)
    {
        printf("B3.%u: readings=%lu missed=%lu ms=%lu\r\n", sub,
               (unsigned long)sonarGetReadingCount(), (unsigned long)sonarGetMissedCount(),
               (unsigned long)(HAL_GetTick() - start));
    }
    taskDone(myTask);
}
