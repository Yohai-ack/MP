/**
 ******************************************************************************
 * @file    bits.h
 * @brief   Assignment 1, B.1: the four bit operations, as functions.
 ******************************************************************************
 */
#ifndef BITS_H
#define BITS_H

#include <stdint.h>

uint32_t bitSet(uint32_t value, uint8_t bit);     /* bit -> 1          */
uint32_t bitClear(uint32_t value, uint8_t bit);   /* bit -> 0          */
uint32_t bitToggle(uint32_t value, uint8_t bit);  /* bit -> !bit       */
uint8_t  bitTest(uint32_t value, uint8_t bit);    /* returns 0 or 1    */

#endif /* BITS_H */
