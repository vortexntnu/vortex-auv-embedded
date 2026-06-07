/*
 * tdoa.c
 *
 *  Created on: 22. mar. 2026
 *      Author: vikin
 */

#include "arm_math.h"
#include <string.h>
#include "tdoa.h"

/* ---------------------------------------------------------------
 * Internal helpers — 3x3 linear algebra in float32.
 * Uses the M7 FPU via arm_math.h intrinsics where available.
 * --------------------------------------------------------------- */

/**
 * @brief  Cholesky decomposition of a 3x3 SPD matrix.
 *         Computes lower-triangular L such that M = L * L^T.
 *
 * @param  M    Input  3x3 matrix (row-major, float32)
 * @param  L    Output 3x3 lower-triangular matrix (row-major, float32)
 * @return 0 on success, -1 if matrix is not positive-definite
 */
static int32_t cholesky3x3(const float32_t M[3][3], float32_t L[3][3])
{
    memset(L, 0, sizeof(float32_t) * 9);

    for (int32_t i = 0; i < 3; i++)
    {
        for (int32_t j = 0; j <= i; j++)
        {
            float32_t sum = M[i][j];

            for (int32_t k = 0; k < j; k++)
            {
                sum -= L[i][k] * L[j][k];
            }

            if (i == j)
            {
                if (sum <= 0.0f)
                {
                    return -1; /* not positive-definite */
                }
                arm_sqrt_f32(sum, &L[i][j]);
            }
            else
            {
                L[i][j] = sum / L[j][j];
            }
        }
    }
    return 0;
}

/**
 * @brief  Forward substitution: solve L * x = y for x.
 *         L is 3x3 lower-triangular (row-major, float32).
 */
static void fwd_sub3(const float32_t L[3][3],
                     const float32_t y[3],
                     float32_t       x[3])
{
    for (int32_t i = 0; i < 3; i++)
    {
        float32_t sum = y[i];
        for (int32_t k = 0; k < i; k++)
        {
            sum -= L[i][k] * x[k];
        }
        x[i] = sum / L[i][i];
    }
}

/**
 * @brief  Back substitution: solve L^T * p = x for p.
 *         L is 3x3 lower-triangular, so L^T is upper-triangular.
 */
static void back_sub3(const float32_t L[3][3],
                      const float32_t x[3],
                      float32_t       p[3])
{
    for (int32_t i = 2; i >= 0; i--)
    {
        float32_t sum = x[i];
        for (int32_t k = i + 1; k < 3; k++)
        {
            sum -= L[k][i] * p[k];   /* L^T[i][k] == L[k][i] */
        }
        p[i] = sum / L[i][i];
    }
}

/* ---------------------------------------------------------------
 * Public API — float32 version with validity mask
 * --------------------------------------------------------------- */

/**
 * @brief  Solve for acoustic source direction using TDOA (float32).
 *
 * Filters the input arrays using @p valid[], compacts the active
 * receivers into a local working set, then runs the least-squares
 * normal-equations solver (A^T A + reg*I) via Cholesky.
 *
 * The first valid receiver is used as the reference (TDOA = 0).
 * At least 2 valid receivers are required (yielding >= 1 equation).
 * At least 4 are recommended for a well-conditioned 3-D solution.
 *
 * @param  r          Receiver positions, shape [n_receivers][3] (metres)
 * @param  t          Time-of-arrival × speed-of-sound (range), shape [n_receivers] (metres)
 *                    Entries where valid[i] == 0 are ignored.
 * @param  valid      Boolean validity mask, shape [n_receivers].
 *                    Non-zero = use this receiver, 0 = skip.
 * @param  n_receivers Total number of entries in r[], t[], and valid[].
 * @param  p_out      Output direction vector [3] (unit-less, same units as r/t)
 *
 * @return  0  Success.
 *         -1  Fewer than 2 valid receivers — cannot form any equation.
 *         -2  Cholesky failed (A^T A not positive-definite; geometry degenerate).
 */
int32_t TDOA_direction_solve_f32(const float32_t  r[][3],
                                  const float32_t  t[],
                                  const uint8_t    valid[],
                                  uint32_t         n_receivers,
                                  float32_t        p_out[3])
{
    /* ---- 0. Compact valid receivers into local arrays ---------- */
    /*
     * Working arrays sized to the maximum possible active count.
     * TDOA_MAX_RECEIVERS caps stack usage; raise it in the header
     * if you ever exceed 16 hydrophones.
     */
    float32_t rf[TDOA_MAX_RECEIVERS][3];
    float32_t tf[TDOA_MAX_RECEIVERS];
    uint32_t  n_valid = 0;

    for (uint32_t i = 0; i < n_receivers; i++)
    {
        if (valid[i] && n_valid < TDOA_MAX_RECEIVERS)
        {
            tf[n_valid]    = t[i];
            rf[n_valid][0] = r[i][0];
            rf[n_valid][1] = r[i][1];
            rf[n_valid][2] = r[i][2];
            n_valid++;
        }
    }

    /* Need at least 2 valid receivers to form 1 equation */
    if (n_valid < 2u)
    {
        return -1;
    }

    const uint32_t n_eq = n_valid - 1u;   /* number of TDOA equations */

    /* ---- 1. Build A (n_eq × 3) and b (n_eq) ------------------- */
    /*
     * Reference receiver is rf[0] / tf[0].
     * Row i:  A[i] = r[0] - r[i+1]
     *         b[i] = t[i+1] - t[0]      (t already = c * TOA)
     */
    float32_t A[TDOA_MAX_RECEIVERS - 1][3];
    float32_t b_vec[TDOA_MAX_RECEIVERS - 1];

    for (uint32_t i = 0; i < n_eq; i++)
    {
        b_vec[i]  = tf[i + 1] - tf[0];
        A[i][0]   = rf[0][0] - rf[i + 1][0];
        A[i][1]   = rf[0][1] - rf[i + 1][1];
        A[i][2]   = rf[0][2] - rf[i + 1][2];
    }

    /* ---- 2. Form M = A^T * A + reg*I  and  y = A^T * b -------- */
    float32_t M[3][3] = {{0}};
    float32_t y[3]    = {0};

    for (int32_t i = 0; i < 3; i++)
    {
        for (int32_t j = 0; j < 3; j++)
        {
            float32_t sum = 0.0f;
            for (uint32_t k = 0; k < n_eq; k++)
            {
                sum += A[k][i] * A[k][j];
            }
            M[i][j] = sum;
        }
        M[i][i] += TDOA_REG_F;   /* Tikhonov regularisation */

        float32_t ys = 0.0f;
        for (uint32_t k = 0; k < n_eq; k++)
        {
            ys += A[k][i] * b_vec[k];
        }
        y[i] = ys;
    }

    /* ---- 3. Cholesky decomposition of M ----------------------- */
    float32_t L[3][3];
    if (cholesky3x3(M, L) != 0)
    {
        return -2;   /* geometry degenerate / non-SPD */
    }

    /* ---- 4. Forward substitution: L * x = y ------------------- */
    float32_t x_vec[3];
    fwd_sub3(L, y, x_vec);

    /* ---- 5. Back substitution: L^T * p = x -------------------- */
    back_sub3(L, x_vec, p_out);


    float32_t norm;
    arm_dot_prod_f32(p_out, p_out, 3, &norm);  /* norm = p · p */
    arm_sqrt_f32(norm, &norm);                  /* norm = |p|   */

    if (norm > 1e-6f)                           /* guard against zero vector */
    {
        p_out[0] /= norm;
        p_out[1] /= norm;
        p_out[2] /= norm;
    }
    else
    {
        return -3;   /* degenerate — zero-length solution vector */
    }

    return 0;
}


/* ---------------------------------------------------------------
 * Q15 version (unchanged from original, kept for compatibility)
 * --------------------------------------------------------------- */
int32_t TDOA_direction_solve_q15(const q15_t    r[][3],
                                  const q15_t    t[],
                                  const uint8_t  valid[],
                                  uint32_t       n_receivers,
                                  q15_t          p[3])
{
    /* ---- 0. Expand Q15 inputs → float and compact valid set ---- */
    float32_t rf[TDOA_MAX_RECEIVERS][3];
    float32_t tf[TDOA_MAX_RECEIVERS];
    uint32_t  n_valid = 0;

    for (uint32_t i = 0; i < n_receivers; i++)
    {
        if (valid[i] && n_valid < TDOA_MAX_RECEIVERS)
        {
            tf[n_valid]    = Q15_TO_FLOAT(t[i]);
            rf[n_valid][0] = Q15_TO_FLOAT(r[i][0]);
            rf[n_valid][1] = Q15_TO_FLOAT(r[i][1]);
            rf[n_valid][2] = Q15_TO_FLOAT(r[i][2]);
            n_valid++;
        }
    }

    if (n_valid < 2u)
    {
        return -1;
    }

    const uint32_t n_eq = n_valid - 1u;

    /* ---- 1. Build A and b ------------------------------------- */
    float32_t A[TDOA_MAX_RECEIVERS - 1][3];
    float32_t b_vec[TDOA_MAX_RECEIVERS - 1];

    for (uint32_t i = 0; i < n_eq; i++)
    {
        b_vec[i]  = tf[i + 1] - tf[0];
        A[i][0]   = rf[0][0] - rf[i + 1][0];
        A[i][1]   = rf[0][1] - rf[i + 1][1];
        A[i][2]   = rf[0][2] - rf[i + 1][2];
    }

    /* ---- 2. Form M = A^T * A + reg*I  and  y = A^T * b -------- */
    float32_t M[3][3] = {{0}};
    float32_t y[3]    = {0};

    for (int32_t i = 0; i < 3; i++)
    {
        for (int32_t j = 0; j < 3; j++)
        {
            float32_t sum = 0.0f;
            for (uint32_t k = 0; k < n_eq; k++)
            {
                sum += A[k][i] * A[k][j];
            }
            M[i][j] = sum;
        }
        M[i][i] += TDOA_REG_F;

        float32_t ys = 0.0f;
        for (uint32_t k = 0; k < n_eq; k++)
        {
            ys += A[k][i] * b_vec[k];
        }
        y[i] = ys;
    }

    /* ---- 3. Cholesky ------------------------------------------ */
    float32_t L[3][3];
    if (cholesky3x3(M, L) != 0)
    {
        return -2;
    }

    /* ---- 4 & 5. Forward + back substitution ------------------- */
    float32_t x_vec[3];
    fwd_sub3(L, y, x_vec);

    float32_t p_f[3];
    back_sub3(L, x_vec, p_f);

    /* ---- 6. Saturate and convert result back to Q15 ------------ */
    for (int32_t i = 0; i < 3; i++)
    {
        float32_t normalised = p_f[i] / TDOA_SCALE;

        if      (normalised >  1.0f) normalised =  1.0f;
        else if (normalised < -1.0f) normalised = -1.0f;

        arm_float_to_q15(&normalised, &p[i], 1);
    }

    return 0;
}
