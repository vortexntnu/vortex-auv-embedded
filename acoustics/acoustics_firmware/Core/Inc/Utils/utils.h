/*
 * utils.h
 *
 *  Created on: 7. mar. 2026
 *      Author: vikin
 */

#ifndef INC_UTILS_UTILS_H_
#define INC_UTILS_UTILS_H_

#include <stdint.h>
#include "arm_math.h"

void utils_delay(volatile uint32_t count);
void utils_DWT_init(void);
void utils_DWT_delay_ms(uint32_t ms);
void utils_DWT_delay_us(uint32_t us);

float32_t utils_abs_f32(float32_t x);
int32_t utils_abs_int32(int32_t x);

float32_t utils_distance_3d(float32_t vec1[3], float32_t vec2[3]);

void utils_clear_array_q15(q15_t* arr, int len);
void utils_clear_array_f32(float32_t* arr, int len);

#endif /* INC_UTILS_UTILS_H_ */
