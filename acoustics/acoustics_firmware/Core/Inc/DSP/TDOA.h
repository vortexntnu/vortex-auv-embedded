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
 *
 * TDOA_MAX_RECEIVERS : hard upper bound on how many receivers the
 *                      working arrays can hold.  Raise this if you
 *                      ever add more than 16 hydrophones.  It only
 *                      affects stack usage inside the solver.
 *
 * TDOA_SCALE         : real-world unit that maps to q15 value 1.0.
 *                      e.g. if coordinates are in metres and span
 *                      ±2 m, set TDOA_SCALE 2.0f.
 *                      (Used only by the Q15 API.)
 *
 * TDOA_REG_F         : Tikhonov regularisation added to the diagonal
 *                      of the normal-equation matrix M = A^T A.
 *                      Keeps the solve stable when receivers are
 *                      nearly co-planar.  Equivalent to 1e-4 in
 *                      physical units (metres²).
 * --------------------------------------------------------------- */
#define TDOA_MAX_RECEIVERS  16         /* stack-allocation cap        */
#define TDOA_SCALE          2.0f       /* metres per full-scale unit  */
#define TDOA_REG_F          1e-5f      /* regularisation (float)      */

/* Convert float <-> Q15 using TDOA_SCALE (Q15 API only) */
#define FLOAT_TO_Q15(x)  ((q15_t)((x) / (TDOA_SCALE) * 32767.0f))
#define Q15_TO_FLOAT(x)  ((float32_t)(x) / 32767.0f * (TDOA_SCALE))

/* ---------------------------------------------------------------
 * Public API — float32
 *
 * r            Receiver positions, shape [n_receivers][3], in metres.
 * t            Time-of-arrival × speed-of-sound (range in metres),
 *              shape [n_receivers].  Entries where valid[i]==0 are
 *              ignored.  The first *valid* entry is used as the
 *              reference (its TDOA is implicitly 0).
 * valid        Boolean mask, shape [n_receivers].
 *              Non-zero = use this receiver.  Zero = skip.
 * n_receivers  Total length of r[], t[], and valid[].
 * p_out        Output direction vector [3], same units as r/t.
 *
 * Returns
 *    0   Success.
 *   -1   Fewer than 2 valid receivers (no equations can be formed).
 *   -2   Cholesky failed — geometry is degenerate (e.g. all
 *        receivers collinear, or fewer than 3 independent equations).
 *   -3	  degenerate output vector could not be normalized
 * --------------------------------------------------------------- */
int32_t TDOA_direction_solve_f32(const float32_t  r[][3],
                                  const float32_t  t[],
                                  const uint8_t    valid[],
                                  uint32_t         n_receivers,
                                  float32_t        p_out[3]);

/* ---------------------------------------------------------------
 * Public API — Q15
 *
 * Same semantics as the f32 version.  r[] and t[] are Q15-encoded
 * using TDOA_SCALE (see macros above).  p[] is returned in Q15.
 *
 * Returns
 *    0   Success.
 *   -1   Fewer than 2 valid receivers.
 *   -2   Cholesky failed.
 * --------------------------------------------------------------- */
int32_t TDOA_direction_solve_q15(const q15_t    r[][3],
                                  const q15_t    t[],
                                  const uint8_t  valid[],
                                  uint32_t       n_receivers,
                                  q15_t          p[3]);

#endif /* INC_DSP_TDOA_H_ */
