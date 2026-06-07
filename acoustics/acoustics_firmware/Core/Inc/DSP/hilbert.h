/*
 * hilbert.h
 *
 *  Created on: 14. mar. 2026
 *      Author: vikin
 */

#ifndef INC_DSP_HILBERT_H_
#define INC_DSP_HILBERT_H_

#include "arm_math_types.h"
#include "arm_math.h"

#ifndef HILBERT_FFT_SIZE
#define HILBERT_FFT_SIZE 512
#endif

/* ------------------------------------------------------------
 *  Q15 interface
 * ------------------------------------------------------------ */

/**
 * @brief  Initialise Hilbert transform state (call once at startup).
 */
void hilbert_init_q15(void);

/**
 * @brief  Compute the analytic signal via the Hilbert transform (Q15).
 *
 * @param  pSrc  Real input,  length HILBERT_FFT_SIZE (Q15).
 * @param  pDst  Complex output, length HILBERT_FFT_SIZE*2,
 *               interleaved Re/Im (Q15).  Suitable for arm_cmplx_mag_q15.
 */
void hilbert_transform_q15(const q15_t *pSrc,
                            q15_t       *pDst);

/**
 * @brief  Return only the imaginary part of the analytic signal (Q15).
 *
 * @param  pSrc  Real input,  length HILBERT_FFT_SIZE (Q15).
 * @param  pDst  Imaginary output, length HILBERT_FFT_SIZE (Q15).
 */
void hilbert_imag_q15(const q15_t *pSrc, q15_t *pDst);


/* ------------------------------------------------------------
 *  F32 interface
 * ------------------------------------------------------------ */

/**
 * @brief  Initialise Hilbert transform state for F32 (call once at startup,
 *         or call hilbert_init() which initialises both).
 */
void hilbert_init_f32(void);

/**
 * @brief  Compute the analytic signal via the Hilbert transform (F32).
 *
 * @param  pSrc  Real input,  length HILBERT_FFT_SIZE (float32_t).
 * @param  pDst  Complex output, length HILBERT_FFT_SIZE*2,
 *               interleaved Re/Im (float32_t).  Suitable for arm_cmplx_mag_f32.
 */
void hilbert_transform_f32(const float32_t *pSrc,
                            float32_t       *pDst);

/**
 * @brief  Return only the imaginary part of the analytic signal (F32).
 *
 * @param  pSrc  Real input,  length HILBERT_FFT_SIZE (float32_t).
 * @param  pDst  Imaginary output, length HILBERT_FFT_SIZE (float32_t).
 */
void hilbert_imag_f32(float32_t *pSrc, float32_t *pDst);

#endif /* INC_DSP_HILBERT_H_ */
