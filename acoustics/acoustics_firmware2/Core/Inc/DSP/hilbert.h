/*
 * hilbert.h
 *
 *  Created on: 14. mar. 2026
 *      Author: vikin
 */

#ifndef INC_DSP_HILBERT_H_
#define INC_DSP_HILBERT_H_

#include "arm_math_types.h"

#ifndef HILBERT_FFT_SIZE
#define HILBERT_FFT_SIZE 256
#endif

void hilbert_init();
void hilbert_transform_q15(const q15_t *pSrc,
                           q15_t       *pDst);
void hilbert_imag_q15(const q15_t *pSrc, q15_t *pDst);

#endif /* INC_DSP_HILBERT_H_ */
