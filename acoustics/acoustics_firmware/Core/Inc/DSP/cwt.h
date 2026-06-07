/*
 * cwt.h
 *
 *  Created on: 14. mar. 2026
 *      Author: vikin
 */

#ifndef INC_DSP_CWT_H_
#define INC_DSP_CWT_H_
#include "arm_math_types.h"

#ifndef CWT_FFT_SIZE
#define CWT_FFT_SIZE 512
#endif

void cwt_init_q15(float32_t target_frequency, float32_t sampling_frequency, float32_t cycles);
void cwt_morlet_magnitude_q15(const q15_t *input, q15_t *magnitude_output);

void cwt_init_f32(float32_t target_frequency, float32_t sampling_frequency, float32_t cycles);
void cwt_morlet_magnitude_f32(const float32_t *input, float32_t *magnitude_output);


#endif /* INC_DSP_CWT_H_ */
