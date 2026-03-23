/*
 * tdoa.c
 *
 *  Created on: 22. mar. 2026
 *      Author: vikin
 */


/* tdoa_q15.c */
#include "arm_math.h"
#include <string.h>
#include <tdoa.h>

/* ---------------------------------------------------------------
 * Internal helpers — all operate in float32 internally for the
 * 3x3 linear algebra, then convert back to Q15 at the boundary.
 * This is intentional: CMSIS-DSP q15 matrix multiply exists but
 * accumulates in q63 and requires careful shift management that
 * adds no benefit for a 3x3 system on a Cortex-M7 with FPU.
 * The float path uses the M7 FPU and is fully deterministic.
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
                arm_sqrt_f32(sum, &L[i][j]);   /* use CMSIS sqrt (may use FPU) */
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
 * Public function
 * --------------------------------------------------------------- */
int32_t TDOA_direction_solve_q15(const q15_t r[][3],
                                  const q15_t t[],
                                  q15_t       p[3])
{
    const int32_t N = TDOA_N_RECEIVERS;

    /* --- 1. Convert Q15 inputs to float ----------------------- */
    float32_t rf[TDOA_N_RECEIVERS][3];
    float32_t tf[TDOA_N_RECEIVERS];

    for (int32_t i = 0; i < N; i++)
    {
        tf[i] = Q15_TO_FLOAT(t[i]);
        for (int32_t j = 0; j < 3; j++)
        {
            rf[i][j] = Q15_TO_FLOAT(r[i][j]);
        }
    }

    /* --- 2. Build A (n-1 x 3) and b (n-1) --------------------- */
    /*        A[i] = r[0] - r[i+1],  b[i] = c*(t[i+1]-t[0])      */
    /*        Note: t[] already holds c*TOA, so b[i] = tf[i+1]    */
    /*        (caller pre-multiplies TOA by c and stores in t[])  */
    float32_t A[TDOA_N_RECEIVERS - 1][3];
    float32_t b_vec[TDOA_N_RECEIVERS - 1];

    for (int32_t i = 0; i < N - 1; i++)
    {
        b_vec[i] = tf[i + 1] - tf[0];   /* c*(t[i+1]-t[0]) */
        for (int32_t j = 0; j < 3; j++)
        {
            A[i][j] = rf[0][j] - rf[i + 1][j];
        }
    }

    /* --- 3. Form M = A^T * A + reg*I  and  y = A^T * b -------- */
    float32_t M[3][3] = {0};
    float32_t y[3]    = {0};

    for (int32_t i = 0; i < 3; i++)
    {
        for (int32_t j = 0; j < 3; j++)
        {
            float32_t sum = 0.0f;
            for (int32_t k = 0; k < N - 1; k++)
            {
                sum += A[k][i] * A[k][j];
            }
            M[i][j] = sum;
        }
        M[i][i] += TDOA_REG_F;   /* regularisation */

        float32_t ys = 0.0f;
        for (int32_t k = 0; k < N - 1; k++)
        {
            ys += A[k][i] * b_vec[k];
        }
        y[i] = ys;
    }

    /* --- 4. Cholesky decomposition of M ----------------------- */
    float32_t L[3][3];
    if (cholesky3x3(M, L) != 0)
    {
        return -1;   /* matrix not positive-definite */
    }

    /* --- 5. Solve L * x = y  (forward substitution) ----------- */
    float32_t x_vec[3];
    fwd_sub3(L, y, x_vec);

    /* --- 6. Solve L^T * p_f = x  (back substitution) ---------- */
    float32_t p_f[3];
    back_sub3(L, x_vec, p_f);

    /* --- 7. Saturate and convert result back to Q15 ------------ */
    for (int32_t i = 0; i < 3; i++)
    {
        /* arm_float_to_q15 expects a normalised float in [-1,1]  */
        float32_t normalised = p_f[i] / TDOA_SCALE;

        /* saturate to Q15 range */
        if      (normalised >  1.0f)  normalised =  1.0f;
        else if (normalised < -1.0f)  normalised = -1.0f;

        arm_float_to_q15(&normalised, &p[i], 1);
    }

    return 0;
}
