/*
 * cwt.c
 *
 *  Created on: 14. mar. 2026
 *      Author: vikin
 */
#include "cwt.h"

#include "memory_placement.h"
#include "dsp.h"

#include "arm_math.h"
#include "arm_math_types.h"

#include <stdlib.h>

/* private function declarations start */
static void cwt_build_morlet_kernel_q15(float32_t target_freq, float32_t fs, float32_t f0);
static void cwt_build_morlet_kernel_f32(float32_t target_freq, float32_t fs, float32_t f0);
/* private function declarations end */

/* private variable definitions start */
static PLACE_IN_DTCM arm_cfft_instance_q15 cwt_cfft_instance_q15;

static PLACE_IN_AXI_SRAM q15_t cwt_kernel_q15[CWT_FFT_SIZE * 2] = {0};
static PLACE_IN_AXI_SRAM q15_t cwt_fft_input_q15[CWT_FFT_SIZE * 2] = {0};
static PLACE_IN_AXI_SRAM q15_t cwt_fft_output_q15[CWT_FFT_SIZE * 2] = {0};

static PLACE_IN_DTCM arm_cfft_instance_f32 cwt_cfft_instance_f32;

static PLACE_IN_AXI_SRAM float32_t cwt_kernel_f32[CWT_FFT_SIZE * 2] = {0};
static PLACE_IN_AXI_SRAM float32_t cwt_fft_input_f32[CWT_FFT_SIZE * 2] = {0};
static PLACE_IN_AXI_SRAM float32_t cwt_fft_output_f32[CWT_FFT_SIZE * 2] = {0};

/* private variable definitions end */


/* q15_t function definitions start */
void cwt_init_q15(float32_t target_frequency, float32_t sampling_frequency, float32_t cycles)
{
	arm_cfft_init_q15(&cwt_cfft_instance_q15, CWT_FFT_SIZE); // complex
    cwt_build_morlet_kernel_q15(target_frequency, sampling_frequency,cycles);
}


static void cwt_build_morlet_kernel_q15(float32_t target_freq, float32_t fs, float32_t f0)
{
    // f0: center frequency parameter (cycles) — controls bandwidth.
    // Higher f0 = narrower bandwidth, better freq resolution, worse time resolution.
    // Typical range: 5.0 to 8.0. Default 5.0 is standard.

    float32_t omega0 = 2.0f * PI * f0;
    float32_t scale  = f0 * fs / target_freq;

    // Positive frequencies
    for (uint32_t k = 0; k < CWT_FFT_SIZE / 2 + 1; k++)
    {
        float32_t omega = 2.0f * PI * (float32_t)k / (float32_t)CWT_FFT_SIZE;
        float32_t arg   = (scale * omega - omega0);
        float32_t val   = expf(-0.5f * arg * arg);

        cwt_kernel_q15[2 * k]     = (q15_t)(val * 32767.0f);
        cwt_kernel_q15[2 * k + 1] = 0;
    }

    // Zero negative frequencies — analytic (one-sided)
    for (uint32_t k = CWT_FFT_SIZE / 2 + 1; k < CWT_FFT_SIZE; k++)
    {
        cwt_kernel_q15[2 * k]     = 0;
        cwt_kernel_q15[2 * k + 1] = 0;
    }
}



void cwt_morlet_magnitude_q15(const q15_t *input, q15_t *magnitude_output)
{
    // 1. Copy and 0 pad input (arm_rfft_q15 modifies in-place)
	dsp_real_to_complex_q15(input, cwt_fft_input_q15, CWT_FFT_SIZE);

    // 2. Forward FFT  →  cwt_time_domain_q15 is complex interleaved, length PROCESSING_FFT_SIZE*2
    arm_cfft_q15(&cwt_cfft_instance_q15, cwt_fft_input_q15, 0, 1);

    // Scale to avoid precision loss
    dsp_fill_headroom_q15(cwt_fft_input_q15, CWT_FFT_SIZE*2);

    // 3. Complex multiply: fft_output * cwt_kernel → cwt_product
    //    arm_cmplx_mult_cmplx_q15 saturates and right-shifts by 1 internally
    arm_cmplx_mult_cmplx_q15(cwt_fft_input_q15, cwt_kernel_q15, cwt_fft_output_q15, CWT_FFT_SIZE);

    // Scale to avoid precision loss
    dsp_fill_headroom_q15(cwt_fft_output_q15, CWT_FFT_SIZE*2);

    // 4. Inverse FFT  →  cwt_result is complex interleaved
    arm_cfft_q15(&cwt_cfft_instance_q15, cwt_fft_output_q15, 1, 1);
    //    NOTE: CMSIS arm_rfft_q15 does not support in-place IFFT natively;
    //    if your version lacks IFFT, use arm_cfft_q15 instead (see note below)

    // Scale to avoid precision loss
    dsp_fill_headroom_q15(cwt_fft_output_q15, CWT_FFT_SIZE*2);

    // 5. Complex magnitude of result → envelope of CWT at target frequency
    arm_cmplx_mag_q15(cwt_fft_output_q15, magnitude_output, CWT_FFT_SIZE);
    //    out_mag[n] = sqrt(re^2 + im^2) — this is your time-domain energy envelope

    // Scale to avoid precision loss
    dsp_fill_headroom_q15(magnitude_output, CWT_FFT_SIZE);
}

/* q15_t function definitions end */



void cwt_init_f32(float32_t target_frequency, float32_t sampling_frequency, float32_t cycles)
{
    arm_cfft_init_f32(&cwt_cfft_instance_f32, CWT_FFT_SIZE);

    cwt_build_morlet_kernel_f32(target_frequency,
    							sampling_frequency,
								cycles);
}



static void cwt_build_morlet_kernel_f32(float32_t target_freq, float32_t fs, float32_t f0)
{
	float32_t omega0 = 2.0f * PI * f0;                                        // was: 2πf0 — wrong, cycles is not a frequency
	float32_t scale  = f0 * fs / target_freq;                                           // was: f0/target_freq — wrong scale factor


    // Positive frequencies
    for (uint32_t k = 0; k < CWT_FFT_SIZE / 2; k++)
    {
    	float32_t omega = 2.0f * PI * (float32_t)k / (float32_t)CWT_FFT_SIZE;   // was: missing fs — wrong axis
        float32_t arg   = (scale * omega - omega0);
        float32_t val   = expf(-0.5f * arg * arg);

        cwt_kernel_f32[2 * k]     = val;  // real
        cwt_kernel_f32[2 * k + 1] = 0.0f; // imag
    }

    // Zero negative frequencies — analytic (one-sided)
    for (uint32_t k = CWT_FFT_SIZE / 2; k < CWT_FFT_SIZE; k++)
    {
        cwt_kernel_f32[2 * k]     = 0.0f;
        cwt_kernel_f32[2 * k + 1] = 0.0f;
    }
}


void cwt_morlet_magnitude_f32(const float32_t *input, float32_t *magnitude_output)
{
	dsp_real_to_complex_f32(input, cwt_fft_input_f32, CWT_FFT_SIZE);

    // Note: if your input buffer is already 2*N interleaved, just arm_copy_f32 directly

    // 2. Forward FFT
    arm_cfft_f32(&cwt_cfft_instance_f32, cwt_fft_input_f32, 0, 1);

    // 3. Complex multiply: fft * kernel → product
    arm_cmplx_mult_cmplx_f32(cwt_fft_input_f32, cwt_kernel_f32, cwt_fft_output_f32, CWT_FFT_SIZE);

    // 4. Inverse FFT
    arm_cfft_f32(&cwt_cfft_instance_f32, cwt_fft_output_f32, 1, 1);

    // 5. Complex magnitude → time-domain energy envelope
    arm_cmplx_mag_f32(cwt_fft_output_f32, magnitude_output, CWT_FFT_SIZE);
}




