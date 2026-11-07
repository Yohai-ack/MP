/**
 ******************************************************************************
 * @file    ass3_wheels.c
 * @brief   Assignment 3: the four wheels. Motor outputs, encoder speeds, the
 *          PI speed controller at 100 Hz (TIM6), the Mecanum motions without
 *          turning, and the odometry. Every car-specific number comes from
 *          `car` (car_params.c).
 *
 *          Wheels: 1 rear right, 2 rear left, 3 front left, 4 front right.
 *          Speeds in rad/s, positive when the wheel turns forward.
 ******************************************************************************
 */
#include "ass3.h"
#include <math.h>

typedef struct
{
    volatile uint32_t *ccr;          /* TIM2->CCR1 .. CCR4 */
    GPIO_TypeDef      *dirPort;
    uint16_t           dirPin;
    TIM_HandleTypeDef *enc;          /* its encoder timer  */
} WheelHw;

static const WheelHw hw[4] =
{
    {&TIM2->CCR1, DIR1_GPIO_Port, DIR1_Pin, &htim1},
    {&TIM2->CCR2, DIR2_GPIO_Port, DIR2_Pin, &htim8},
    {&TIM2->CCR3, DIR3_GPIO_Port, DIR3_Pin, &htim3},
    {&TIM2->CCR4, DIR4_GPIO_Port, DIR4_Pin, &htim4},
};

static volatile uint8_t ctrlOn = 0;
static float    ref[4];              /* setpoints, rad/s                    */
static float    integ[4];            /* integral of the error, rad          */
static float    speed[4];            /* last measured speeds, rad/s         */
static float    uOut[4];             /* last control efforts, V             */
static uint16_t prevCnt[4];          /* encoder counter at the last sample  */
static volatile int32_t pos[4];      /* counts since odoReset, + forward    */
static float    radPerCount[4];

/* one motor: u in V, + forward; the DIR level comes from car->motorDir */
static void motorOut(uint8_t w, float u)
{
    uint8_t forward = (u >= 0.0f);
    uint8_t dirHigh = (forward == (car->motorDir[w] > 0));   /* DIR = 1 drives it this way */
    float   duty = fabsf(u) / V_SUPPLY;

    if (duty > 1.0f)
    {
        duty = 1.0f;
    }
    HAL_GPIO_WritePin(hw[w].dirPort, hw[w].dirPin, dirHigh ? GPIO_PIN_SET : GPIO_PIN_RESET);
    *hw[w].ccr = (uint32_t)(duty * (float)PWM_FULL);
}

void wheelsInit(void)
{
    HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1);
    HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_2);
    HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_3);
    HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_4);
    for (uint8_t w = 0; w < 4u; w++)
    {
        *hw[w].ccr = 0;
        HAL_TIM_Encoder_Start(hw[w].enc, TIM_CHANNEL_ALL);   /* TI1 and TI2, set in ass3.ioc */
        radPerCount[w] = TWO_PI / car->countsPerRev[w];
    }
}

void wheelsStart(void)
{
    for (uint8_t w = 0; w < 4u; w++)
    {
        ref[w] = 0.0f;
        integ[w] = 0.0f;
        prevCnt[w] = (uint16_t)hw[w].enc->Instance->CNT;
    }
    ctrlOn = 1;
    HAL_TIM_Base_Start_IT(&htim6);
}

void wheelsStop(void)
{
    ctrlOn = 0;
    HAL_TIM_Base_Stop_IT(&htim6);
    for (uint8_t w = 0; w < 4u; w++)
    {
        ref[w] = 0.0f;
        *hw[w].ccr = 0;
    }
}

/* TIM6, every 10 ms: measure the four speeds, run the four PIs, record */
void wheelsSample(void)
{
    int16_t d[4];

    for (uint8_t w = 0; w < 4u; w++)
    {
        uint16_t now = (uint16_t)hw[w].enc->Instance->CNT;
        d[w] = (int16_t)((int16_t)(uint16_t)(now - prevCnt[w]) * car->encDir[w]);   /* wrap-safe, + forward */
        prevCnt[w] = now;
        pos[w] += d[w];
        speed[w] = (float)d[w] * radPerCount[w] / CTRL_DT;
    }

    if (ctrlOn)
    {
        for (uint8_t w = 0; w < 4u; w++)
        {
            float e = ref[w] - speed[w];
            float u = car->Kp[w] * e + car->Ki[w] * (integ[w] + e * CTRL_DT);
            /* anti-windup: integrate only while the output is inside the supply,
               or when the error drives it back inside                          */
            if ((fabsf(u) <= V_SUPPLY) || ((u > 0.0f) != (e > 0.0f)))
            {
                integ[w] += e * CTRL_DT;
            }
            u = car->Kp[w] * e + car->Ki[w] * integ[w];
            if (u > V_SUPPLY)
            {
                u = V_SUPPLY;
            }
            else if (u < -V_SUPPLY)
            {
                u = -V_SUPPLY;
            }
            motorOut(w, u);
            uOut[w] = u;
        }
    }

    recSampleStep(ref[0], speed, uOut);        /* each records only while its run is on */
    recSampleCourse(d, courseState);
}

void wheelsSetRef(const float r[4])
{
    uint32_t pm = __get_PRIMASK();             /* all four change in the same sample */
    __disable_irq();
    for (uint8_t w = 0; w < 4u; w++)
    {
        ref[w] = r[w];
    }
    __set_PRIMASK(pm);
}

void wheelsGetSpeed(float w[4])
{
    for (uint8_t i = 0; i < 4u; i++)
    {
        w[i] = speed[i];
    }
}

/* the largest step from rest that no wheel's PI turns into saturation, x 0.9 */
float wheelsVCmd(void)
{
    float m = car->maxVelCmdNoSat[0];
    for (uint8_t w = 1; w < 4u; w++)
    {
        if (car->maxVelCmdNoSat[w] < m)
        {
            m = car->maxVelCmdNoSat[w];
        }
    }
    return SPEED_FACTOR * m;
}

/* ------------------------------------------------------------------ */
/* Mecanum motions without turning. Wheel speeds for a cart speed vx   */
/* (forward) and vy (left), no rotation:                               */
/*   rear right (1) and front left (3): (vx - vy) / r                  */
/*   rear left  (2) and front right (4): (vx + vy) / r                 */
/* Each motion puts every wheel at +-vCmd, so a motion that starts     */
/* from rest never saturates. Change motion only through a stop.       */
/* ------------------------------------------------------------------ */
void driveForward(int8_t dir)
{
    float v = (float)dir * wheelsVCmd();
    float r[4] = {v, v, v, v};
    wheelsSetRef(r);
}

void driveSideways(int8_t dir)
{
    float v = (float)(dir * STRAFE_SIGN) * wheelsVCmd();
    float r[4] = {-v, v, -v, v};
    wheelsSetRef(r);
}

/* odometry from the same kinematics: x = r (q1 + q2 + q3 + q4) / 4,
   y = r (-q1 + q2 - q3 + q4) / 4, q = wheel angle since odoReset      */
static float wheelAngle(uint8_t w)
{
    return (float)pos[w] * radPerCount[w];
}

float odoForwardMm(void)
{
    return 1000.0f * WHEEL_RADIUS_M *
           (wheelAngle(0) + wheelAngle(1) + wheelAngle(2) + wheelAngle(3)) / 4.0f;
}

float odoLeftMm(void)
{
    return (float)STRAFE_SIGN * 1000.0f * WHEEL_RADIUS_M *
           (-wheelAngle(0) + wheelAngle(1) - wheelAngle(2) + wheelAngle(3)) / 4.0f;
}

void odoReset(void)
{
    uint32_t pm = __get_PRIMASK();
    __disable_irq();
    for (uint8_t w = 0; w < 4u; w++)
    {
        pos[w] = 0;
    }
    __set_PRIMASK(pm);
}
