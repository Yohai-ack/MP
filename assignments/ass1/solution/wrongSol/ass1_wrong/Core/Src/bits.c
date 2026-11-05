/**
 ******************************************************************************
 * @file    bits.c
 * @brief   Assignment 1, B.1: the four bit idioms (set, clear, toggle, test).
 *
 *          1UL, not 1: the mask must be 32 bits wide, so that bit 31 of a
 *          register can be reached without shifting into the sign bit.
 ******************************************************************************
 */
#include "bits.h"

/* OR with the mask: the chosen bit becomes 1, the others stay */
uint32_t bitSet(uint32_t value, uint8_t bit)
{
    return value | (1UL << bit);
}

/* AND with the inverted mask: the chosen bit becomes 0, the others stay */
uint32_t bitClear(uint32_t value, uint8_t bit)
{
    return value & ~(1UL << bit);
}

/* XOR with the mask: the chosen bit flips, the others stay */
uint32_t bitToggle(uint32_t value, uint8_t bit)
{
    return value ^ (1UL << bit);
}

/* shift the chosen bit down to bit 0, then keep only bit 0 */
uint8_t bitTest(uint32_t value, uint8_t bit)
{
    return (uint8_t)((value >> bit) & 1UL);
}
