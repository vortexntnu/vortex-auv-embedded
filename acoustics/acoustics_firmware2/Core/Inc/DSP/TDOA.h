/*
 * tdoa.h
 *
 *  Created on: 22. mar. 2026
 *      Author: vikin
 */

#ifndef INC_DSP_TDOA_H_
#define INC_DSP_TDOA_H_

#include "arm_math.h"
#include <stdint.h>

/* ---------------------------------------------------------------
 * USER-TUNABLE CONSTANTS
 * TDOA_SCALE : real-world unit that maps to q15 value 1.0
 *              e.g. if coordinates are in metres and span ±2 m,
 *              set TDOA_SCALE 2.0f
 * TDOA_REG   : Tikhonov regularisation added to diagonal of M
 *              (equivalent to 1e-6 in the Python version, scaled)
 * --------------------------------------------------------------- */
#define TDOA_N_RECEIVERS   4          /* number of receivers        */
#define TDOA_SCALE         2.0f       /* metres per full-scale unit  */
#define TDOA_REG_F         1e-4f      /* regularisation (float)      */

/* Convert float <-> Q15 using TDOA_SCALE */
#define FLOAT_TO_Q15(x)  ((q15_t)((x) / (TDOA_SCALE) * 32767.0f))
#define Q15_TO_FLOAT(x)  ((float)(x) / 32767.0f * (TDOA_SCALE))

/* ---------------------------------------------------------------
 * Public API
 *
 * r   : receiver positions, row-major [N][3], in Q15
 * t   : time-of-arrival differences (t[i] - t[0]) already
 *       multiplied by speed-of-sound c, in Q15 (i.e. range diff)
 *       t[0] should be 0 (reference receiver)
 * p   : output direction vector [3], in Q15
 *
 * Returns 0 on success, -1 if Cholesky fails (non-SPD matrix).
 * --------------------------------------------------------------- */
int32_t TDOA_direction_solve_q15(const q15_t r[][3],
                                  const q15_t t[],
                                  q15_t       p[3]);

#endif /* INC_DSP_TDOA_H_ */
