/**
 ******************************************************************************
 * @file    io_board.h
 * @brief   Driver API for the course I2C IO board (lesson 3).
 *
 *          8 RGB LEDs  : 3x PCA9634 (one chip per colour, channel n = LED n)
 *          4 buttons   : PCA9538A at 0x71, active-low on bits 0..3
 *
 *          I2C is a BLACK BOX until session 5 - use these functions.
 *
 *          Assignment 1 version: functions that would solve parts of the
 *          assignment for you are left out on purpose.
 *
 * @course  Microprocessor-Based Product Design - Prof. Izhak Bucher, Technion
 ******************************************************************************
 */
#ifndef IO_BOARD_H
#define IO_BOARD_H

#include <stdint.h>
#include <stdbool.h>

/* ------------------------------------------------------------------ */
/* I2C 7-bit addresses of the chips on the board                       */
/* ------------------------------------------------------------------ */
#define PCA_RED        0x01   /* PCA9634: red   channel of LEDs 0..7  */
#define PCA_GREEN      0x02   /* PCA9634: green channel of LEDs 0..7  */
#define PCA_BLUE       0x07   /* PCA9634: blue  channel of LEDs 0..7  */
#define PCA_KEYS       0x71   /* PCA9538A: 4 push buttons             */

#define N_LEDS         8u

/* set true if the LEDs are wired common-anode (values inverted)      */
#define COMMON_ANODE_INVERT  false

/* ------------------------------------------------------------------ */
/* one-call setup: init the 3 LED chips + the button chip,             */
/* prints a sanity report over UART, leaves all LEDs off               */
/* ------------------------------------------------------------------ */
void    ioBoardInit(void);

/* ------------------------------------------------------------------ */
/* LEDs                                                                */
/* ------------------------------------------------------------------ */
void    setLedChannel(uint8_t chip, uint8_t ch, uint8_t value);
void    setAllLedChannels(uint8_t chip, uint8_t value);
void    setRGB(uint8_t r, uint8_t g, uint8_t b);          /* ALL leds  */
void    setRGB_led(uint8_t led, uint8_t r, uint8_t g, uint8_t b);
void    allRgbOff(void);
uint8_t gamma2(uint8_t x);          /* perceptual brightness helper    */

/* ------------------------------------------------------------------ */
/* Buttons                                                             */
/* ------------------------------------------------------------------ */
uint8_t readButtonsRaw(void);       /* raw byte, ACTIVE-LOW            */

/* ------------------------------------------------------------------ */
/* Plumbing (used by ioBoardInit; handy for experiments)               */
/* ------------------------------------------------------------------ */
bool    checkI2C(uint8_t addr);     /* does a chip answer?             */

#endif /* IO_BOARD_H */
