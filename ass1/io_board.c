/**
 ******************************************************************************
 * @file    io_board.c
 * @brief   Driver for the course I2C IO board (see io_board.h).
 *
 *          Built from the course test program main.c - same addresses,
 *          same registers, same behaviour, packaged as a reusable layer.
 ******************************************************************************
 */
#include "main.h"          /* HAL, hi2c1 handle via extern below */
#include "io_board.h"
#include <stdio.h>

extern I2C_HandleTypeDef hi2c1;    /* created by CubeMX in main.c */

/* ---------------- PCA9634 registers (LED chips) ------------------- */
#define PCA9634_MODE1    0x00
#define PCA9634_MODE2    0x01
#define PCA9634_PWM0     0x02      /* PWM0..PWM7 = 0x02..0x09        */
#define PCA9634_LEDOUT0  0x0C
#define PCA9634_LEDOUT1  0x0D

/* ---------------- PCA9538A registers (button chip) ---------------- */
#define KEYS_INPUT       0x00
#define KEYS_POLARITY    0x02
#define KEYS_CONFIG      0x03

/* ================= low-level I2C plumbing ========================= */

static void writeReg(uint8_t addr, uint8_t reg, uint8_t value)
{
    HAL_I2C_Mem_Write(&hi2c1,
                      (uint16_t)(addr << 1),   /* HAL wants the 8-bit form */
                      reg,
                      I2C_MEMADD_SIZE_8BIT,
                      &value, 1, HAL_MAX_DELAY);
}

static uint8_t readReg(uint8_t addr, uint8_t reg)
{
    uint8_t value = 0xFF;

    if (HAL_I2C_Mem_Read(&hi2c1,
                         (uint16_t)(addr << 1),
                         reg,
                         I2C_MEMADD_SIZE_8BIT,
                         &value, 1, HAL_MAX_DELAY) != HAL_OK)
    {
        return 0xFF;
    }
    return value;
}

bool checkI2C(uint8_t addr)
{
    return HAL_I2C_IsDeviceReady(&hi2c1, (uint16_t)(addr << 1), 3, 100) == HAL_OK;
}

/* ================= LED chips (PCA9634) ============================ */

static void pca9634Init(uint8_t addr)
{
    writeReg(addr, PCA9634_MODE1, 0x20);     /* auto-increment, awake  */
    writeReg(addr, PCA9634_MODE2, 0x04);     /* totem-pole outputs     */

    writeReg(addr, PCA9634_LEDOUT0, 0xAA);   /* all 8 channels in      */
    writeReg(addr, PCA9634_LEDOUT1, 0xAA);   /* individual-PWM mode    */

    for (uint8_t ch = 0; ch < N_LEDS; ch++)
    {
        writeReg(addr, PCA9634_PWM0 + ch, COMMON_ANODE_INVERT ? 255 : 0);
    }
}

void setLedChannel(uint8_t chip, uint8_t ch, uint8_t value)
{
    if (ch >= N_LEDS) return;                /* bounds check - C won't */

    if (COMMON_ANODE_INVERT) value = 255 - value;

    writeReg(chip, PCA9634_PWM0 + ch, value);
}

void setAllLedChannels(uint8_t chip, uint8_t value)
{
    for (uint8_t ch = 0; ch < N_LEDS; ch++)
        setLedChannel(chip, ch, value);
}

void setRGB(uint8_t r, uint8_t g, uint8_t b)
{
    setAllLedChannels(PCA_RED,   r);
    setAllLedChannels(PCA_GREEN, g);
    setAllLedChannels(PCA_BLUE,  b);
}

void setRGB_led(uint8_t led, uint8_t r, uint8_t g, uint8_t b)
{
    setLedChannel(PCA_RED,   led, r);
    setLedChannel(PCA_GREEN, led, g);
    setLedChannel(PCA_BLUE,  led, b);
}

void setLedMask(uint8_t chip, uint8_t mask, uint8_t brightness)
{
    for (uint8_t ch = 0; ch < N_LEDS; ch++)
        if (mask & (1u << ch))
            setLedChannel(chip, ch, brightness);
}

void allRgbOff(void)
{
    setRGB(0, 0, 0);
}

uint8_t gamma2(uint8_t x)
{
    /* perceived brightness ~ value^2; cast BEFORE multiplying:       */
    /* 255*255 = 65025 overflows uint8_t, fits uint16_t (lesson 1!)   */
    return (uint8_t)(((uint16_t)x * x) / 255u);
}

/* ================= button chip (PCA9538A) ========================= */

static void keysInit(void)
{
    writeReg(PCA_KEYS, KEYS_CONFIG,   0xFF); /* all pins are inputs    */
    writeReg(PCA_KEYS, KEYS_POLARITY, 0x00); /* no inversion in chip   */
}

uint8_t readButtonsRaw(void)
{
    return readReg(PCA_KEYS, KEYS_INPUT);
}

uint8_t readButtonsPressed(void)
{
    /* active-low: invert, keep the 4 button bits (lesson 2 idioms)   */
    return (uint8_t)((~readButtonsRaw()) & BTN_MASK);
}

void printByte(uint8_t v)
{
    for (int i = 7; i >= 0; i--)
        printf("%u", (unsigned)((v >> i) & 1u));
}

void printButtons(uint8_t raw, uint8_t pressed)
{
    printf("KEYS RAW=0b");
    printByte(raw);
    printf(" HEX=0x%02X pressed: ", raw);

    if (pressed & 0x01) printf("P0 ");
    if (pressed & 0x02) printf("P1 ");
    if (pressed & 0x04) printf("P2 ");
    if (pressed & 0x08) printf("P3 ");
    if (pressed == 0)   printf("none");
    printf("\r\n");
}

/* ================= one-call setup ================================= */

void ioBoardInit(void)
{
    printf("\r\n--- IO board init ---\r\n");
    printf(checkI2C(PCA_RED)   ? "RED   found (0x01)\r\n" : "RED   MISSING (0x01)!\r\n");
    printf(checkI2C(PCA_GREEN) ? "GREEN found (0x02)\r\n" : "GREEN MISSING (0x02)!\r\n");
    printf(checkI2C(PCA_BLUE)  ? "BLUE  found (0x07)\r\n" : "BLUE  MISSING (0x07)!\r\n");
    printf(checkI2C(PCA_KEYS)  ? "KEYS  found (0x71)\r\n" : "KEYS  MISSING (0x71)!\r\n");

    pca9634Init(PCA_RED);
    pca9634Init(PCA_GREEN);
    pca9634Init(PCA_BLUE);
    keysInit();
    allRgbOff();

    printf("KEYS CONFIG=0x%02X INPUT=0x%02X\r\n",
           readReg(PCA_KEYS, KEYS_CONFIG),
           readReg(PCA_KEYS, KEYS_INPUT));
    printf("--- init done ---\r\n\r\n");
}
