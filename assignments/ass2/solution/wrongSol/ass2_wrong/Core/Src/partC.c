/**
 ******************************************************************************
 * @file    partC.c
 * @brief   Assignment 2, Part C: our own I2C driver (io_board.c not used).
 *
 *          HAL addresses are the 7-bit address shifted left by one.
 *          PCA9634 (LEDs):    MODE1 0x00, MODE2 0x01, PWM0..7 0x02..0x09,
 *                             LEDOUT0/1 0x0C/0x0D. The top 3 bits of the
 *                             register byte (control register) choose the
 *                             auto-increment: 0x80 | reg = all registers.
 *          PCA9538A (keys):   INPUT 0x00, POLARITY 0x02, CONFIG 0x03.
 *          PCA9685 (servo):   MODE1 0x00 (bit 5 = auto-increment, bit 4 =
 *                             sleep), LEDn_ON_L 0x06 + 4n, PRESCALE 0xFE.
 ******************************************************************************
 */
#include "main.h"
#include "ass2.h"
#include <stdio.h>

#define I2C_TIMEOUT_MS   10u

#define LED_RED          0x01u
#define LED_GREEN        0x02u
#define LED_BLUE         0x07u
#define KEYS             0x71u
#define SERVO_CHIP       0x44u
#define NO_CHIP          0x10u

#define PCA9634_MODE1    0x00u
#define PCA9634_MODE2    0x01u
#define PCA9634_PWM0     0x02u
#define PCA9634_LEDOUT0  0x0Cu
#define PCA9634_LEDOUT1  0x0Du
#define PCA9634_AI_ALL   0x80u      /* control register: AI2 AI1 AI0 = 100 */

#define KEYS_INPUT       0x00u
#define KEYS_POLARITY    0x02u
#define KEYS_CONFIG      0x03u

#define PCA9685_MODE1    0x00u
#define PCA9685_MODE2    0x01u
#define PCA9685_LED0_ON  0x06u
#define PCA9685_PRESCALE 0xFEu
#define SERVO_CHANNEL    0u         /* confirmed on our cart (2026-11-04) */

/* ------------------------------------------------------------------ */
/* the driver: one register, or several from `reg` on                  */
/* ------------------------------------------------------------------ */
static HAL_StatusTypeDef i2cWrite(uint8_t addr, uint8_t reg, uint8_t *data, uint16_t n)
{
    return HAL_I2C_Mem_Write(&hi2c1, (uint16_t)(addr << 1), reg, I2C_MEMADD_SIZE_8BIT, data, n,
                             I2C_TIMEOUT_MS);
}

static HAL_StatusTypeDef i2cWriteReg(uint8_t addr, uint8_t reg, uint8_t value)
{
    return i2cWrite(addr, reg, &value, 1);
}

static HAL_StatusTypeDef i2cReadReg(uint8_t addr, uint8_t reg, uint8_t *value)
{
    return HAL_I2C_Mem_Read(&hi2c1, (uint16_t)(addr << 1), reg, I2C_MEMADD_SIZE_8BIT, value, 1,
                            I2C_TIMEOUT_MS);
}

/* PCA9634: awake, totem-pole outputs, every channel on its own PWM */
static void ledChipInit(uint8_t addr)
{
    i2cWriteReg(addr, PCA9634_MODE1, 0x00);     /* SLEEP = 0: oscillator on   */
    i2cWriteReg(addr, PCA9634_MODE2, 0x04);     /* OUTDRV = 1: totem pole     */
    i2cWriteReg(addr, PCA9634_LEDOUT0, 0xAA);   /* LDRx = 10: PWMx controls  */
    i2cWriteReg(addr, PCA9634_LEDOUT1, 0xAA);   /*  channel x (4 per register) */
    HAL_Delay(1);                               /* oscillator start-up, 500 us */
}

static void ledSetChannel(uint8_t addr, uint8_t channel, uint8_t value)
{
    i2cWriteReg(addr, (uint8_t)(PCA9634_PWM0 + channel), value);
}

/* ------------------------------------------------------------------ */
/* C.1: scan the bus                                                   */
/* ------------------------------------------------------------------ */
void partC1(void)
{
    uint8_t count = 0;
    for (uint8_t addr = 0x01; addr <= 0x7F; addr++)
    {
        if (HAL_I2C_IsDeviceReady(&hi2c1, (uint16_t)(addr << 1), 1, 2) == HAL_OK)   /* ACK */
        {
            printf("C1: found=0x%02X\r\n", addr);
            count++;
        }
    }
    printf("C1: count=%u\r\n", count);
    taskDone(TASK_C1);
}

/* ------------------------------------------------------------------ */
/* C.2: a colour on each LED                                           */
/* ------------------------------------------------------------------ */
void partC2(void)
{
    static const uint8_t colour[8][3] = {
        {255, 0, 0}, {0, 255, 0}, {0, 0, 255}, {255, 255, 0},
        {0, 255, 255}, {255, 0, 255}, {255, 128, 0}, {255, 255, 255}
    };

    ledChipInit(LED_RED);
    ledChipInit(LED_GREEN);
    ledChipInit(LED_BLUE);
    for (uint8_t led = 0; led < 8u; led++)
    {
        /* values scaled down: full brightness is blinding */
        uint8_t r = colour[led][0] / 4u, g = colour[led][1] / 4u, b = colour[led][2] / 4u;
        ledSetChannel(LED_RED, led, r);
        ledSetChannel(LED_GREEN, led, g);
        ledSetChannel(LED_BLUE, led, b);
        printf("C2: led=%u r=%u g=%u b=%u\r\n", led, r, g, b);
    }
    taskDone(TASK_C2);
}

/* ------------------------------------------------------------------ */
/* C.3: 8 single writes vs one write of 8 bytes (auto-increment)       */
/* ------------------------------------------------------------------ */
void partC3(void)
{
    uint8_t values[8] = {0, 0, 0, 0, 0, 0, 0, 0};

    ledChipInit(LED_RED);
    uint32_t t0 = HAL_GetTick();
    for (uint16_t i = 0; i < 1000u; i++)
    {
        for (uint8_t ch = 0; ch < 8u; ch++)
        {
            ledSetChannel(LED_RED, ch, values[ch]);       /* 8 transactions */
        }
    }
    uint32_t single = HAL_GetTick() - t0;

    t0 = HAL_GetTick();
    for (uint16_t i = 0; i < 1000u; i++)
    {
        i2cWrite(LED_RED, PCA9634_AI_ALL | PCA9634_PWM0, values, 8);   /* 1 transaction */
    }
    uint32_t burst = HAL_GetTick() - t0;

    printf("C3: single=%lu burst=%lu\r\n", (unsigned long)single, (unsigned long)burst);
    taskDone(TASK_C3);
}

/* ------------------------------------------------------------------ */
/* C.4: buttons, and every HAL return value                            */
/* ------------------------------------------------------------------ */
void partC4(void)
{
    uint8_t value = 0;
    HAL_StatusTypeDef st = i2cReadReg(NO_CHIP, KEYS_INPUT, &value);   /* nobody ACKs */
    printf("C4: addr=0x%02X status=%u\r\n", NO_CHIP, st);

    st = i2cWriteReg(KEYS, KEYS_CONFIG, 0xFF);          /* all 8 pins inputs */
    if (st != HAL_OK) printf("C4 config write status=%u\r\n", st);
    st = i2cWriteReg(KEYS, KEYS_POLARITY, 0x00);        /* no inversion      */
    if (st != HAL_OK) printf("C4 polarity write status=%u\r\n", st);

    uint8_t last = 0xFF;                                /* not a 4-bit value */
    while (task == TASK_C4)
    {
        st = i2cReadReg(KEYS, KEYS_INPUT, &value);
        uint8_t buttons = value & 0x0Fu;                /* bits 0..3, active-low */
        if ((buttons != last) || (st != HAL_OK))
        {
            last = buttons;
            printf("C4: buttons=");
            printBinary(buttons, 8);
            printf(" status=%u\r\n", st);
            if (st != HAL_OK)
            {
                HAL_Delay(500);                         /* don't flood PuTTY */
            }
        }
    }
}

/* ------------------------------------------------------------------ */
/* C.5: servo at 50 Hz                                                 */
/* ------------------------------------------------------------------ */
static void servoInit(void)
{
    i2cWriteReg(SERVO_CHIP, PCA9685_MODE1, 0x10);       /* sleep: PRESCALE is writable only now */
    i2cWriteReg(SERVO_CHIP, PCA9685_PRESCALE, 121);     /* 25 MHz / (4096 * 122) = 50.03 Hz */
    i2cWriteReg(SERVO_CHIP, PCA9685_MODE1, 0x20);       /* awake, auto-increment */
    HAL_Delay(1);                                       /* oscillator, 500 us   */
    i2cWriteReg(SERVO_CHIP, PCA9685_MODE1, 0xA0);       /* restart the PWM      */
    i2cWriteReg(SERVO_CHIP, PCA9685_MODE2, 0x04);       /* totem pole           */
}

/* pulse in us -> OFF count: 4096 counts per 20 ms frame */
static uint16_t servoPulse(uint16_t us)
{
    uint16_t off = (uint16_t)(((uint32_t)us * 4096u) / 19000u);   /* PLANTED ERROR 3: wrong divisor */
    uint8_t data[4] = {0, 0, (uint8_t)(off & 0xFFu), (uint8_t)(off >> 8)};   /* ON = 0 */
    i2cWrite(SERVO_CHIP, (uint8_t)(PCA9685_LED0_ON + 4u * SERVO_CHANNEL), data, 4);
    return off;
}

void partC5(void)
{
    static const uint16_t pulses[3] = {900, 1500, 2100};

    servoInit();
    while (task == TASK_C5)
    {
        for (uint8_t i = 0; i < 3u; i++)
        {
            uint16_t off = servoPulse(pulses[i]);
            printf("C5: us=%u off=%u\r\n", pulses[i], off);
            if (!waitMs(TASK_C5, 1000u))
            {
                break;
            }
        }
    }
    servoPulse(1500);                                   /* park at the centre */
}
