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

/**
 * @brief  Filters out huge irregular spikes on the input.
 * 			Does so by taking the absolute average of an elements neighbors and checking if the element is larger than the average*threshold
 *
 * @param  signal    	Pointer to input/output array
 * @param  signal_len   Lenght of input/output array
 * @param  threshold   	How much larger than the neighborhood average a value needs to be in order to be filtered
 */
void dsp_spike_filter(float32_t *signal, uint32_t signal_len, float32_t threshold){
	const uint8_t window_radius = 3; //left and right distance
	for(int i = window_radius; i < signal_len - window_radius; i++){
		float32_t sum = 0;
		for(int j = -window_radius; j < window_radius + 1; j++){
			if(j != 0){
				sum += dsp_abs_f32(signal[i+j]);
			}
		}
		sum /= window_radius*2;
		if(dsp_abs_f32(signal[i]) > sum*threshold){
			signal[i] = sum;
		}
	}
}

/**
 * @brief  Searches from end to start for values under the threshold before returning the index
 *
 * @param  signal    	Pointer to input array
 * @param  signal_len   Lenght of input array
 * @param  threshold   	The threshold
 * @param  patience   	How many elements in a row that need to be under the threshold before it returns
 */
uint32_t dsp_rl_under_threshold_search(float32_t* signal, uint32_t signal_len, float32_t threshold, const uint32_t patience)
{
    uint32_t i = signal_len;
    uint32_t remaining_patience = patience;

    while (i--)
    {
        if (signal[i] < threshold){
        	remaining_patience--;
        }else{
        	remaining_patience = patience;
        }
        if(remaining_patience == 0){
        	return i + patience;
        }
    }

    return 0;
}

/**
 * @brief  Takes the average of the n_lowest and n_highest values of the input and returns a linear interpolation between them
 *
 * @param  signal    	Pointer to input array
 * @param  signal_len   Lenght of input array
 * @param  t   			linear interpolation value from 0 to 1 where 0 is the average of the lowest and 1 gives the average of the largest
 * @param  n_high		how many of the largest values to take the average of
 * @param  n_low		how many of the smallest value to take the average of
 */
float32_t dsp_min_max_lerp(float32_t* signal, uint32_t signal_len, float32_t t, uint32_t n_high, uint32_t n_low)
{
    /*
     * Finds the average of the bottom N and top N points in the array,
     * then returns an interpolated threshold between those two averages.
     *
     * threshold = 0.0 -> returns the low average
     * threshold = 1.0 -> returns the high average
     * threshold = 0.5 -> returns the midpoint between them
     */

    /* --- Sort a copy of the signal using an in-place insertion sort ---
     * For large arrays consider a faster algorithm, but insertion sort
     * has zero heap allocation and is fine for typical DSP frame sizes. */
    float32_t sorted[signal_len];
    arm_copy_f32(signal, sorted, signal_len);

    /* Insertion sort (ascending) */
    for (uint32_t i = 1; i < signal_len; i++)
    {
        float32_t key = sorted[i];
        int32_t j = (int32_t)i - 1;
        while (j >= 0 && sorted[j] > key)
        {
            sorted[j + 1] = sorted[j];
            j--;
        }
        sorted[j + 1] = key;
    }

    /* --- Average the N lowest values --- */
    float32_t low_mean = 0.0f;
    arm_mean_f32(sorted, n_low, &low_mean);

    /* --- Average the N highest values --- */
    float32_t high_mean = 0.0f;
    arm_mean_f32(&sorted[signal_len - n_high], n_high, &high_mean);

    /* --- Interpolate between the two averages --- */
    /* result = low + threshold * (high - low) */
    float32_t result = 0.0f;
    arm_add_f32(                          /* low + t*(high-low)          */
        &low_mean,                        /* not a vector call, so we    */
        &(float32_t){t *          		  /* use scalar arithmetic below */
            (high_mean - low_mean)},
        &result, 1);

    /* Simpler and equally valid on Cortex-M7 with FPU: */
    result = low_mean + t * (high_mean - low_mean);

    return result;
}

float32_t dsp_abs_f32(float32_t x){
	if(x >= 0){
		return x;
	}else{
		return -x;
	}
}

int32_t dsp_abs_int32(int32_t x){
	if(x >= 0){
		return x;
	}else{
		return -x;
	}
}
