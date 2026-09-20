/*
 * dsp.h
 *
 *  Created on: 14. mar. 2026
 *      Author: vikin
 */

#ifndef INC_DSP_DSP_H_
#define INC_DSP_DSP_H_
#include "arm_math_types.h"

void dsp_fill_headroom_q15(q15_t* input ,uint32_t fft_size);

void dsp_real_to_complex_q15(q15_t* real_input, q15_t* complex_output, uint32_t fft_size);
void dsp_real_to_complex_f32(float32_t* real_input, float32_t* complex_output, uint32_t fft_size);

void dsp_cmplx_mag_squared_q15(
    const q15_t * __restrict__ pSrc,
          q15_t * __restrict__ pDst,
          uint32_t             numSamples);

void dsp_spike_filter(float32_t *signal, uint32_t signal_len, float32_t threshold);
uint32_t dsp_rl_under_threshold_search(float32_t* signal, uint32_t signal_len, float32_t threshold, const uint32_t patience);
float32_t dsp_min_max_lerp(float32_t* signal, uint32_t signal_len, float32_t t, uint32_t n_high, uint32_t n_low);
float32_t dsp_abs_f32(float32_t x);
int32_t dsp_abs_int32(int32_t x);

#endif /* INC_DSP_DSP_H_ */
