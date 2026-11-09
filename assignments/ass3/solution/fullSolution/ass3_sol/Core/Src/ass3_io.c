/**
 ******************************************************************************
 * @file    ass3_io.c
 * @brief   Assignment 3: the sensors and the servo. The IR sensor (PA7), the
 *          ultrasonic sensor in millimetres (sonar.c in interrupt mode, the
 *          factor of A2 B.1), and the servo that turns it (PCA9685, own I2C
 *          writes as in A2 C.5).
 ******************************************************************************
 */
#include "ass3.h"
#include "sonar.h"
#include <stdio.h>

/* A2 B.1 on our cart (2026-11-04): fit of 5, 10, 15 cm: 101.9 counts per mm, offset -6 mm */
#define RAW_PER_MM_X10   1019
#define SONAR_OFFSET_MM  (-6)
#define SONAR_MAX_MM     4000u      /* no echo (about 6 m of raw) counts as 4 m */

#define SERVO_CHIP       0x44u
#define SERVO_CHANNEL    0u
#define PCA9685_MODE1    0x00u
#define PCA9685_MODE2    0x01u
#define PCA9685_LED0_ON  0x06u
#define PCA9685_PRESCALE 0xFEu
#define I2C_TIMEOUT_MS   10u

uint8_t irObstacle(void)
{
    return (HAL_GPIO_ReadPin(IR_SENSOR_GPIO_Port, IR_SENSOR_Pin) == GPIO_PIN_SET) == (car->irObstacleLevel != 0u);
}

/* ------------------------------------------------------------------ */
/* ultrasonic sensor: the last three readings, in mm                   */
/* ------------------------------------------------------------------ */
static volatile uint32_t lastMm[3];
static volatile uint32_t lastTick = 0;
static volatile uint32_t readings = 0;

void sonarOn(void)
{
    readings = 0;
    lastTick = 0;
    sonarStartInterrupt();
}

void sonarOff(void)
{
    sonarStop();
}

/* called in the TIM5 capture interrupt when a new raw reading is ready */
void sonarOnReading(void)
{
    int32_t mm = (int32_t)(sonarGetRaw() * 10u / RAW_PER_MM_X10) + SONAR_OFFSET_MM;

    if (mm < 0)
    {
        mm = 0;
    }
    if ((uint32_t)mm > SONAR_MAX_MM)
    {
        mm = SONAR_MAX_MM;
    }
    lastMm[2] = lastMm[1];
    lastMm[1] = lastMm[0];
    lastMm[0] = (uint32_t)mm;
    lastTick = HAL_GetTick();
    readings++;
}

uint8_t sonarFresh(uint32_t maxAgeMs)
{
    return (readings > 0u) && (HAL_GetTick() - lastTick <= maxAgeMs);
}

uint32_t sonarMm(void)
{
    return lastMm[0];
}

/* median of three: one false reading (too short or too long) is ignored */
uint32_t sonarMedianMm(void)
{
    uint32_t a = lastMm[0], b = lastMm[1], c = lastMm[2];

    if (readings < 3u)
    {
        return a;
    }
    if ((a <= b && b <= c) || (c <= b && b <= a))
    {
        return b;
    }
    if ((b <= a && a <= c) || (c <= a && a <= b))
    {
        return a;
    }
    return c;
}

uint32_t sonarReadingCount(void)
{
    return readings;
}

/* ------------------------------------------------------------------ */
/* servo, through the PCA9685 at 50 Hz                                 */
/* ------------------------------------------------------------------ */
static void servoWrite(uint8_t reg, uint8_t *data, uint16_t n)
{
    HAL_I2C_Mem_Write(&hi2c1, (uint16_t)(SERVO_CHIP << 1), reg, I2C_MEMADD_SIZE_8BIT, data, n, I2C_TIMEOUT_MS);
}

static void servoWriteReg(uint8_t reg, uint8_t value)
{
    servoWrite(reg, &value, 1);
}

void servoInit(void)
{
    servoWriteReg(PCA9685_MODE1, 0x10);       /* sleep: PRESCALE is writable only now */
    servoWriteReg(PCA9685_PRESCALE, 121);     /* 25 MHz / (4096 * 122) = 50.03 Hz     */
    servoWriteReg(PCA9685_MODE1, 0x20);       /* awake, auto-increment                */
    HAL_Delay(1);                             /* oscillator, 500 us                   */
    servoWriteReg(PCA9685_MODE1, 0xA0);       /* restart the PWM                      */
    servoWriteReg(PCA9685_MODE2, 0x04);       /* totem pole                           */
    servoSetUs(servoAheadUs());               /* the sonar looks ahead                */
}

void servoSetUs(uint16_t us)
{
    if (us < SERVO_MIN_US)
    {
        us = SERVO_MIN_US;
    }
    if (us > SERVO_MAX_US)
    {
        us = SERVO_MAX_US;
    }
    uint16_t off = (uint16_t)(((uint32_t)us * 4096u + 10000u) / 20000u);   /* 4096 counts per 20 ms */
    uint8_t  data[4] = {0, 0, (uint8_t)(off & 0xFFu), (uint8_t)(off >> 8)};  /* ON = 0 */
    servoWrite((uint8_t)(PCA9685_LED0_ON + 4u * SERVO_CHANNEL), data, 4);
}

/* the direction the sonar looks at, in degrees from straight ahead, + left */
float servoAngleDeg(uint16_t us)
{
    return ((float)us - (float)car->servoZeroUs) * SERVO_DEG_PER_US * (float)car->servoDir;
}

/* the pulse at which the sonar looks straight ahead (0 degrees) */
uint16_t servoAheadUs(void)
{
    return car->servoZeroUs;
}

/* ------------------------------------------------------------------ */
/* task 900 (TA, cable): calibration for car_params.c. The servo follows */
/* calServoUs (write it in Live Expressions or over SWD); twice a second */
/* the IR pin level is printed.                                         */
/* ------------------------------------------------------------------ */
volatile uint16_t calServoUs = 1500u;

void calibrate(void)
{
    uint16_t now = 0;
    uint8_t  n = 0;

    while (waitGoing(TASK_CAL, 50u))
    {
        if (calServoUs != now)
        {
            now = calServoUs;
            servoSetUs(now);
        }
        if (++n >= 10u)
        {
            n = 0;
            printf("CAL: us=%u ir_pin=%u\r\n", now,
                   HAL_GPIO_ReadPin(IR_SENSOR_GPIO_Port, IR_SENSOR_Pin) == GPIO_PIN_SET);
        }
    }
}
