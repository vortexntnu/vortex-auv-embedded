/*
 * hilbert.c
 *
 *  Created on: 14. mar. 2026
 *      Author: vikin
 */

#include "hilbert.h"

#include "memory_placement.h"
#include "dsp.h"

#include "arm_math.h"
#include "arm_math_types.h"

#include <stdlib.h>

// Hilbert specific
static PLACE_IN_DTCM q15_t hilbert_fft_input_q15[HILBERT_FFT_SIZE];
static PLACE_IN_DTCM q15_t hilbert_fft_output_q15[HILBERT_FFT_SIZE*2];
static PLACE_IN_DTCM arm_rfft_instance_q15 hilbert_rfft_q15;
static PLACE_IN_DTCM arm_cfft_instance_q15 hilbert_cfft_q15;


void hilbert_init(){
	arm_rfft_init_q15(&hilbert_rfft_q15, HILBERT_FFT_SIZE, 0, 1);
    arm_cfft_init_q15(&hilbert_cfft_q15,HILBERT_FFT_SIZE);
}

void hilbert_transform_q15(const q15_t *pSrc,
                           q15_t       *pDst)  // length HILBERT_FFT_SIZE*2, interleaved Re/Im
{
    arm_copy_q15((q15_t *)pSrc, hilbert_fft_input_q15, HILBERT_FFT_SIZE);

    dsp_fill_headroom_q15(hilbert_fft_input_q15, HILBERT_FFT_SIZE);

    arm_rfft_q15(&hilbert_rfft_q15, hilbert_fft_input_q15, hilbert_fft_output_q15);

    dsp_fill_headroom_q15(hilbert_fft_output_q15, HILBERT_FFT_SIZE*2);

    /* Zero DC bin */
    hilbert_fft_output_q15[0] = 0;
    hilbert_fft_output_q15[1] = 0;

    /* Positive frequency bins: apply -j rotation */
    for (uint32_t k = 1; k < HILBERT_FFT_SIZE / 2; k++)
    {
        q15_t re = hilbert_fft_output_q15[2 * k];
        q15_t im = hilbert_fft_output_q15[2 * k + 1];

        hilbert_fft_output_q15[2 * k]     =  im;
        hilbert_fft_output_q15[2 * k + 1] = -re;
    }

    /* Zero Nyquist bin */
    hilbert_fft_output_q15[HILBERT_FFT_SIZE]     = 0;
    hilbert_fft_output_q15[HILBERT_FFT_SIZE + 1] = 0;

    /* Zero all negative frequency bins */
    for (uint32_t k = HILBERT_FFT_SIZE / 2 + 1; k < HILBERT_FFT_SIZE; k++)
    {
        hilbert_fft_output_q15[2 * k]     = 0;
        hilbert_fft_output_q15[2 * k + 1] = 0;
    }
    arm_cfft_q15(&hilbert_cfft_q15, hilbert_fft_output_q15, 1, 1);

    dsp_fill_headroom_q15(hilbert_fft_output_q15,HILBERT_FFT_SIZE*2);

    /* Copy interleaved Re/Im directly to pDst for arm_cmplx_mag_q15 */
    for (uint32_t i = 0; i < HILBERT_FFT_SIZE; i++)
    {
        pDst[2 * i]     = hilbert_fft_output_q15[2 * i];      // Re
        pDst[2 * i + 1] = hilbert_fft_output_q15[2 * i + 1];  // Im
    }
}

void hilbert_imag_q15(const q15_t *pSrc, q15_t *pDst)
{
    arm_copy_q15((q15_t *)pSrc, hilbert_fft_input_q15, HILBERT_FFT_SIZE);

    dsp_fill_headroom_q15(hilbert_fft_input_q15,HILBERT_FFT_SIZE);

    arm_rfft_q15(&hilbert_rfft_q15, hilbert_fft_input_q15, hilbert_fft_output_q15);

    dsp_fill_headroom_q15(hilbert_fft_output_q15,HILBERT_FFT_SIZE*2);

    hilbert_fft_output_q15[0] = 0;
    hilbert_fft_output_q15[1] = 0;

    for (uint32_t k = 1; k < HILBERT_FFT_SIZE / 2; k++)
    {
        q15_t re = hilbert_fft_output_q15[2 * k];
        q15_t im = hilbert_fft_output_q15[2 * k + 1];
        hilbert_fft_output_q15[2 * k]     =  im;
        hilbert_fft_output_q15[2 * k + 1] = -re;
    }

    hilbert_fft_output_q15[HILBERT_FFT_SIZE]     = 0;
    hilbert_fft_output_q15[HILBERT_FFT_SIZE + 1] = 0;

    for (uint32_t k = HILBERT_FFT_SIZE / 2 + 1; k < HILBERT_FFT_SIZE; k++)
    {
        hilbert_fft_output_q15[2 * k]     = 0;
        hilbert_fft_output_q15[2 * k + 1] = 0;
    }

    arm_cfft_q15(&hilbert_cfft_q15, hilbert_fft_output_q15, 1, 1);

    dsp_fill_headroom_q15(hilbert_fft_output_q15,HILBERT_FFT_SIZE*2);

    /* Imaginary part only */
    for (uint32_t i = 0; i < HILBERT_FFT_SIZE; i++)
        pDst[i] = hilbert_fft_output_q15[2 * i];
}
