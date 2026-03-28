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
#include "arm_const_structs.h"

#include <stdlib.h>
#include <string.h>

/* ============================================================
 *  Q15 state  (unchanged)
 * ============================================================ */

static PLACE_IN_DTCM q15_t hilbert_fft_input_q15[HILBERT_FFT_SIZE];
static PLACE_IN_DTCM q15_t hilbert_fft_output_q15[HILBERT_FFT_SIZE * 2];
static PLACE_IN_DTCM arm_rfft_instance_q15  hilbert_rfft_q15;
static PLACE_IN_DTCM arm_cfft_instance_q15  hilbert_cfft_q15;

/* ============================================================
 *  F32 state
 *
 *  arm_rfft_fast_f32 (forward):
 *    input  : HILBERT_FFT_SIZE real samples
 *    output : HILBERT_FFT_SIZE floats in "packed" format
 *             [Re0, Re(N/2), Re1, Im1, Re2, Im2, ..., Re(N/2-1), Im(N/2-1)]
 *
 *  We unpack that into a full complex array of length HILBERT_FFT_SIZE*2
 *  before manipulating the spectrum, then use arm_cfft_f32 for the IFFT.
 * ============================================================ */

static PLACE_IN_DTCM float32_t hilbert_fft_input_f32[HILBERT_FFT_SIZE];
static PLACE_IN_DTCM float32_t hilbert_fft_output_f32[HILBERT_FFT_SIZE * 2];
static PLACE_IN_DTCM float32_t hilbert_cfft_buf[HILBERT_FFT_SIZE * 2];
static PLACE_IN_DTCM arm_rfft_fast_instance_f32 hilbert_rfft_f32;
static PLACE_IN_DTCM const arm_cfft_instance_f32 *hilbert_cfft_f32;


/* ============================================================
 *  Init
 * ============================================================ */

void hilbert_init_q15(void)
{
    /* Q15 */
    arm_rfft_init_q15(&hilbert_rfft_q15, HILBERT_FFT_SIZE, 0, 1);
    arm_cfft_init_q15(&hilbert_cfft_q15, HILBERT_FFT_SIZE);
}

void hilbert_init_f32(void)
{
    arm_rfft_fast_init_f32(&hilbert_rfft_f32, HILBERT_FFT_SIZE);

    /* arm_cfft_init_f32 is available from CMSIS-DSP 1.9+.
     * If your version is older, use the table-lookup form instead:
     *   hilbert_cfft_f32 = arm_cfft_sR_f32_lenXXX;  (e.g. arm_cfft_sR_f32_len256)
     * For portability both paths are shown; comment out whichever does not apply. */
#if defined(ARM_MATH_MVEF) || defined(ARM_MATH_HELIUM) || (defined(ARM_DSP_CONFIG_TABLES))
    /* CMSIS-DSP >= 1.9 with dynamic init */
    arm_cfft_init_f32(hilbert_cfft_f32, HILBERT_FFT_SIZE);
#else
    /* Static table lookup — adjust the symbol for your FFT size */
#if   HILBERT_FFT_SIZE == 64
    hilbert_cfft_f32 = &arm_cfft_sR_f32_len64;
#elif HILBERT_FFT_SIZE == 128
    hilbert_cfft_f32 = &arm_cfft_sR_f32_len128;
#elif HILBERT_FFT_SIZE == 256
    hilbert_cfft_f32 = &arm_cfft_sR_f32_len256;
#elif HILBERT_FFT_SIZE == 512
    hilbert_cfft_f32 = &arm_cfft_sR_f32_len512;
#elif HILBERT_FFT_SIZE == 1024
    hilbert_cfft_f32 = &arm_cfft_sR_f32_len1024;
#elif HILBERT_FFT_SIZE == 2048
    hilbert_cfft_f32 = &arm_cfft_sR_f32_len2048;
#elif HILBERT_FFT_SIZE == 4096
    hilbert_cfft_f32 = &arm_cfft_sR_f32_len4096;
#else
#error "Unsupported HILBERT_FFT_SIZE for F32 CFFT table lookup"
#endif
#endif /* ARM_DSP_CONFIG_TABLES */
}


/* ============================================================
 *  Internal helper: build the one-sided (analytic) spectrum
 *  from the packed rfft output.
 *
 *  arm_rfft_fast_f32 produces a "packed" array of length N:
 *    buf[0]   = Re[0]      (DC,  purely real)
 *    buf[1]   = Re[N/2]    (Nyquist, purely real)
 *    buf[2k]  = Re[k]      for k = 1 … N/2-1
 *    buf[2k+1]= Im[k]      for k = 1 … N/2-1
 *
 *  We expand to a full complex array [Re0,Im0, Re1,Im1, …, Re(N-1),Im(N-1)],
 *  apply the one-sided Hilbert filter in-place, then hand it to arm_cfft_f32
 *  for the inverse transform.
 * ============================================================ */
static void hilbert_build_analytic_f32(float32_t *buf)
{
    const uint32_t N    = HILBERT_FFT_SIZE;
    const uint32_t half = N / 2;

    /* --- Unpack the rfft "packed" output into full complex form ---
     *
     * We do this in-place, working from the highest index downward so
     * we never overwrite a source value before reading it.
     *
     * After unpacking:
     *   buf[2k]   = Re[k]
     *   buf[2k+1] = Im[k]
     * for k = 0 … N-1  (negative-frequency bins will be zeroed below).
     */

    /* DC bin */
    /* buf[0] already holds Re[0]; set Im[0] = 0 — DC is purely real */
    /* buf[1] holds Re[N/2] (Nyquist); we will overwrite it below     */

    /* Positive bins k = N/2-1 down to 1 */
    for (int32_t k = (int32_t)half - 1; k >= 1; k--)
    {
        buf[2 * k]     = buf[2 * k];      /* Re[k] — already in place */
        buf[2 * k + 1] = buf[2 * k + 1];  /* Im[k] — already in place */
        /* (no-op written explicitly so the layout is visible) */
    }

    /* Fix DC */
    buf[0] = buf[0];   /* Re[0] unchanged */
    buf[1] = 0.0f;     /* Im[0] = 0       */

    /* Nyquist bin — zero it (as in the q15 version) */
    buf[N]     = 0.0f;
    buf[N + 1] = 0.0f;

    /* Zero all negative-frequency bins */
    for (uint32_t k = half + 1; k < N; k++)
    {
        buf[2 * k]     = 0.0f;
        buf[2 * k + 1] = 0.0f;
    }

    /* --- Apply -j rotation to positive-frequency bins ---
     *
     * Multiplying by -j:   Re' = +Im,   Im' = -Re
     * This is equivalent to a 90-degree phase shift (Hilbert rotation).
     */

    /* Zero DC bin (no contribution from DC in Hilbert transform) */
    buf[0] = 0.0f;
    buf[1] = 0.0f;

    for (uint32_t k = 1; k < half; k++)
    {
        float32_t re = buf[2 * k];
        float32_t im = buf[2 * k + 1];

        buf[2 * k]     =  im;   /* Re' = +Im */
        buf[2 * k + 1] = -re;   /* Im' = -Re */
    }
}


/* ============================================================
 *  F32 public API
 * ============================================================ */

/**
 * hilbert_transform_f32
 *
 * Computes the analytic signal: pDst[i] = { Re[i], Im[i] }
 * in interleaved format, length HILBERT_FFT_SIZE*2 floats.
 * Pass pDst directly to arm_cmplx_mag_f32 for envelope detection.
 */
void hilbert_transform_f32(const float32_t *pSrc,
                            float32_t       *pDst)
{
    const uint32_t N = HILBERT_FFT_SIZE;

    /* Copy input (arm_rfft_fast_f32 writes to a separate output buffer,
     * but we keep a clean copy in case we need to reuse pSrc) */
    memcpy(hilbert_fft_input_f32, pSrc, N * sizeof(float32_t));

    /* Forward real FFT — output written into hilbert_fft_output_f32 in
     * "packed" format (length N floats) */
    arm_rfft_fast_f32(&hilbert_rfft_f32,
                      hilbert_fft_input_f32,
                      hilbert_fft_output_f32,
                      0 /* ifftFlag = 0 → forward */);

    /* Expand packed rfft output to full complex, zero negative frequencies,
     * and apply -j rotation to positive frequencies */
    hilbert_build_analytic_f32(hilbert_fft_output_f32);

    /* Inverse complex FFT — in-place, bit-reversal enabled */
    arm_cfft_f32(hilbert_cfft_f32,
                 hilbert_fft_output_f32,
                 1 /* ifftFlag = 1 → inverse */,
                 1 /* bitReverseFlag */);

    /* Scale by 1/N to obtain a properly normalised IFFT.
     * arm_cfft_f32 does NOT apply the 1/N factor automatically. */
    const float32_t scale = 1.0f / (float32_t)N;
    arm_scale_f32(hilbert_fft_output_f32, scale, hilbert_fft_output_f32, N * 2);

    /* Copy interleaved Re/Im to output */
    memcpy(pDst, hilbert_fft_output_f32, N * 2 * sizeof(float32_t));
}


/**
 * hilbert_imag_f32
 *
 * Returns only the imaginary part of the analytic signal —
 * the classical Hilbert transform output, length HILBERT_FFT_SIZE floats.
 */
void hilbert_imag_f32(const float32_t *pSrc, float32_t *pDst)
{
    const uint32_t N    = HILBERT_FFT_SIZE;
    const uint32_t half = N / 2;

    /* Step 1: Real FFT (packed output) */
    arm_rfft_fast_f32(&hilbert_rfft_f32,
                      pSrc,
                      hilbert_fft_output_f32,
                      0);

    /* Step 2: Build analytic signal spectrum in-place
     *
     * Packed format:
     *   __buf__[0] = Re[0]
     *   __buf__[1] = Re[N/2]
     *   __buf__[2k], __buf__[2k+1] = Re/__Im__[k]
     */

    /* --- DC → 0 --- */
    hilbert_fft_output_f32[0] = 0.0f;

    /* --- __Nyquist__ → 0 --- */
    hilbert_fft_output_f32[1] = 0.0f;

    /* --- Positive frequencies: apply __Hilbert__ transform ---
     * Multiply by (-j * 2)
     */
    for (uint32_t k = 1; k < half; k++)
    {
        float32_t re = hilbert_fft_output_f32[2 * k];
        float32_t im = hilbert_fft_output_f32[2 * k + 1];

        hilbert_fft_output_f32[2 * k]     =  2.0f * im;   /* Re' */
        hilbert_fft_output_f32[2 * k + 1] = -2.0f * re;   /* __Im__' */
    }

    /* NOTE:
     * Negative frequencies are implicitly zero in RFFT representation.
     * No need to explicitly zero them — inverse RFFT handles it.
     */

    /* Step 3: Inverse real FFT */
    arm_rfft_fast_f32(&hilbert_rfft_f32,
                      hilbert_fft_output_f32,
                      pDst,
                      1);

    /* Step 4: Normalize (CMSIS FFT is __unscaled__) */
    arm_scale_f32(pDst, 1.0f / (float32_t)N, pDst, N);
}


/* ============================================================
 *  Q15 functions  (unchanged from original)
 * ============================================================ */

void hilbert_transform_q15(const q15_t *pSrc,
                            q15_t       *pDst)
{
    arm_copy_q15((q15_t *)pSrc, hilbert_fft_input_q15, HILBERT_FFT_SIZE);

    dsp_fill_headroom_q15(hilbert_fft_input_q15, HILBERT_FFT_SIZE);

    arm_rfft_q15(&hilbert_rfft_q15, hilbert_fft_input_q15, hilbert_fft_output_q15);

    dsp_fill_headroom_q15(hilbert_fft_output_q15, HILBERT_FFT_SIZE * 2);

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

    dsp_fill_headroom_q15(hilbert_fft_output_q15, HILBERT_FFT_SIZE * 2);

    /* Copy interleaved Re/Im directly to pDst for arm_cmplx_mag_q15 */
    for (uint32_t i = 0; i < HILBERT_FFT_SIZE; i++)
    {
        pDst[2 * i]     = hilbert_fft_output_q15[2 * i];
        pDst[2 * i + 1] = hilbert_fft_output_q15[2 * i + 1];
    }
}

void hilbert_imag_q15(const q15_t *pSrc, q15_t *pDst)
{
    arm_copy_q15((q15_t *)pSrc, hilbert_fft_input_q15, HILBERT_FFT_SIZE);

    dsp_fill_headroom_q15(hilbert_fft_input_q15, HILBERT_FFT_SIZE);

    arm_rfft_q15(&hilbert_rfft_q15, hilbert_fft_input_q15, hilbert_fft_output_q15);

    dsp_fill_headroom_q15(hilbert_fft_output_q15, HILBERT_FFT_SIZE * 2);

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

    dsp_fill_headroom_q15(hilbert_fft_output_q15, HILBERT_FFT_SIZE * 2);

    /* Imaginary part only */
    for (uint32_t i = 0; i < HILBERT_FFT_SIZE; i++)
        pDst[i] = hilbert_fft_output_q15[2 * i];
}
