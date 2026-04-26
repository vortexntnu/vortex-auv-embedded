/*
 * utils.c
 *
 *  Created on: 7. mar. 2026
 *      Author: vikin
 */


#include "utils.h"

#include <stdio.h>
#include "stm32h7xx_hal.h"

#include "arm_math.h"

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

float32_t utils_abs_f32(float32_t x){
	if(x >= 0){
		return x;
	}else{
		return -x;
	}
}

int32_t utils_abs_int32(int32_t x){
	if(x >= 0){
		return x;
	}else{
		return -x;
	}
}

float32_t utils_distance_3d(float32_t vec1[3], float32_t vec2[3])
{
    float32_t diff[3];
    float32_t dot;
    float32_t result;

    arm_sub_f32(vec1, vec2, diff, 3);
    arm_dot_prod_f32(diff, diff, 3, &dot);
    arm_sqrt_f32(dot, &result);

    return result;
}

void utils_clear_array_q15(q15_t* arr, int len){
	for(int i = 0; i < len; i++) arr[i] = 0;
}

void utils_clear_array_f32(float32_t* arr, int len){
	for(int i = 0; i < len; i++) arr[i] = 0;
}
