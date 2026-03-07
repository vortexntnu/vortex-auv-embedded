/*
 * utils.c
 *
 *  Created on: 7. mar. 2026
 *      Author: vikin
 */

#include <stdio.h>
#include "utils.h"
#include "stm32h7xx_hal.h"

void utils_delay(volatile uint32_t count)
{
    while(count--) __NOP();
}

void utils_DWT_init(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk; // Enable DWT
    DWT->CYCCNT = 0;                                // Reset cycle counter
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;             // Enable cycle counter
}

void utils_DWT_delay_ms(uint32_t ms)
{
    uint32_t start = DWT->CYCCNT;
    uint32_t cycles = (SystemCoreClock / 1000) * ms;

    while ((DWT->CYCCNT - start) < cycles);
}

void utils_DWT_delay_us(uint32_t us)
{
    uint32_t start = DWT->CYCCNT;
    uint32_t cycles = (SystemCoreClock / 1000000) * us;

    while ((DWT->CYCCNT - start) < cycles);
}
