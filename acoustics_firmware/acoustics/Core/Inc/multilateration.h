#ifndef MULTILATERATION_H
#define MULTILATERATION_H

#include <stdbool.h>
#include <stdint.h>
#include "arm_math.h"

#ifdef __cplusplus
extern "C" {
#endif

// Accumulator for batch/online “point-to-rays” least squares:
//    (Σ P_k) s = Σ P_k p_k,  where P_k = I - u_k u_k^T
// You can keep pushing rays with ml_add_ray() and call ml_solve() anytime.
typedef struct {
    float32_t A[9];  // Σ P_k   (row-major 3x3)
    float32_t b[3];  // Σ P_k p_k
    uint32_t K;      // number of rays added (for info)
} ml_accum_t;

// Initialize/clear the accumulator
static inline void ml_init(ml_accum_t* ml) {
    for (int i = 0; i < 9; i++)
        ml->A[i] = 0.0f;
    ml->b[0] = ml->b[1] = ml->b[2] = 0.0f;
    ml->K = 0u;
}

// Add a single ray (pose p, unit bearing u). Optional weight w (>=0).
// If u is not normalized, we normalize it here.
void ml_add_ray(ml_accum_t* ml,
                const float32_t p[3],
                const float32_t u_in[3],
                float32_t w);

// Solve (Σ P_k) s = Σ P_k p_k   → s_out (3x1).
// Returns true on success (matrix invertible).
bool ml_solve(const ml_accum_t* ml, float32_t s_out[3]);

// Convenience one-shot batch solver from arrays.
// poses:  K×3 (row-major: [px,py,pz, px,py,pz, ...])
// dirs:   K×3 (unit DOA vectors; will be normalized inside)
// weights optional (can be NULL → all ones).
bool ml_solve_batch(const float32_t* poses,
                    const float32_t* dirs,
                    const float32_t* weights,  // or NULL
                    uint32_t K,
                    float32_t s_out[3]);

#ifdef __cplusplus
}
#endif
#endif  // MULTILATERATION_H
