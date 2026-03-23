/*
 * dsp.c
 *
 *  Created on: 14. mar. 2026
 *      Author: vikin
 */

#include "dsp.h"

#include "arm_math_types.h"
#include "arm_math.h"

void dsp_fill_headroom_q15(q15_t *input, uint32_t num_samples) // num_samples = actual element count
{
    q15_t    max_val;
    uint32_t max_idx;

    arm_absmax_q15(input, num_samples, &max_val, &max_idx);

    if (max_val == 0) return; // nothing to do

    // How many bits until the MSB hits the sign bit
    uint32_t headroom = __CLZ((uint32_t)(uint16_t)max_val) - 17;

    if (headroom == 0) return; // already full scale

    arm_shift_q15(input, (int8_t)headroom, input, num_samples);
}

#define dsp_real_to_complex(real_input, complex_output, fft_size) _Generic((real_input), \
    q15_t*:     dsp_real_to_complex_q15,                                                 \
    float32_t*: dsp_real_to_complex_f32                                                  \
)(real_input, complex_output, fft_size)

void dsp_real_to_complex_q15(q15_t* real_input, q15_t* complex_output, uint32_t fft_size){
	for(int i = 0; i < fft_size; i++){
		complex_output[2*i] = real_input[i];
		complex_output[2*i + 1] = 0;
	}
}

void dsp_real_to_complex_f32(float32_t* real_input, float32_t* complex_output, uint32_t fft_size){
	for(int i = 0; i < fft_size; i++){
		complex_output[2*i] = real_input[i];
		complex_output[2*i + 1] = 0;
	}
}

/**
 * @brief  Squared magnitude of Q15 complex array (no sqrt).
 *         Output is Q15 (saturated), computed as:
 *         out[n] = sat15( (real[n]^2 + imag[n]^2) >> 15 )
 *
 * @param  pSrc    Input array: [real0, imag0, real1, imag1, ...]  Q15
 * @param  pDst    Output array: squared magnitudes                 Q15
 * @param  numSamples  Number of complex samples
 */
void dsp_cmplx_mag_squared_q15(
    const q15_t * __restrict__ pSrc,
          q15_t * __restrict__ pDst,
          uint32_t             numSamples)
{
    uint32_t i = numSamples >> 2;   /* 4 samples per SIMD iteration   */

    /* ── 4-sample SIMD loop ─────────────────────────────────────── */
    while (i--)
    {
        /*
         * SMUAD  : Signed Multiply Accumulate Dual (re*re + im*im)
         *          Operates on two 16-bit lanes of a 32-bit word.
         *          Result is a 32-bit accumulator (Q30).
         */
        q31_t in1 = *__SIMD32(pSrc)++;   /* [imag0 | real0] */
        q31_t in2 = *__SIMD32(pSrc)++;   /* [imag1 | real1] */
        q31_t in3 = *__SIMD32(pSrc)++;   /* [imag2 | real2] */
        q31_t in4 = *__SIMD32(pSrc)++;   /* [imag3 | real3] */

        /* re^2 + im^2 in Q30 */
        q31_t acc0 = __SMUAD(in1, in1);
        q31_t acc1 = __SMUAD(in2, in2);
        q31_t acc2 = __SMUAD(in3, in3);
        q31_t acc3 = __SMUAD(in4, in4);

        /*
         * Shift Q30 → Q15 (drop 15 bits) then saturate to int16.
         * __SSAT saturates a 32-bit value to N bits.
         */
        *pDst++ = (q15_t)__SSAT(acc0 >> 15, 16);
        *pDst++ = (q15_t)__SSAT(acc1 >> 15, 16);
        *pDst++ = (q15_t)__SSAT(acc2 >> 15, 16);
        *pDst++ = (q15_t)__SSAT(acc3 >> 15, 16);
    }

    /* ── Scalar tail: handle remainder (0-3 samples) ────────────── */
    i = numSamples & 3u;
    while (i--)
    {
        q31_t re = *pSrc++;
        q31_t im = *pSrc++;
        q31_t acc = (re * re + im * im);   /* Q30 */
        *pDst++ = (q15_t)__SSAT(acc >> 15, 16);
    }
}

