#include "multilateration.h"
#include <math.h>

static inline void vec3_normalize(const float32_t in[3], float32_t out[3]) {
    float32_t n2 = in[0] * in[0] + in[1] * in[1] + in[2] * in[2];
    if (n2 > 0.0f) {
        float32_t invn = 1.0f / sqrtf(n2);
        out[0] = in[0] * invn;
        out[1] = in[1] * invn;
        out[2] = in[2] * invn;
    } else {
        out[0] = 1.0f;
        out[1] = 0.0f;
        out[2] = 0.0f;
    }
}

static inline void mat3_add_scaled(float32_t A[9],
                                   const float32_t B[9],
                                   float32_t w) {
    for (int i = 0; i < 9; i++)
        A[i] += w * B[i];
}

static inline void mat3_vec3_add_scaled(float32_t b[3],
                                        const float32_t M[9],
                                        const float32_t v[3],
                                        float32_t w) {
    // b += w * (M v)
    float32_t t0 = M[0] * v[0] + M[1] * v[1] + M[2] * v[2];
    float32_t t1 = M[3] * v[0] + M[4] * v[1] + M[5] * v[2];
    float32_t t2 = M[6] * v[0] + M[7] * v[1] + M[8] * v[2];
    b[0] += w * t0;
    b[1] += w * t1;
    b[2] += w * t2;
}

// Build projector P = I - u u^T
static inline void projector_from_u(const float32_t u[3], float32_t P[9]) {
    float32_t ux = u[0], uy = u[1], uz = u[2];
    float32_t uxx = ux * ux, uyy = uy * uy, uzz = uz * uz;
    float32_t uxy = ux * uy, uxz = ux * uz, uyz = uy * uz;

    // I - u u^T (row-major)
    P[0] = 1.0f - uxx;
    P[1] = -uxy;
    P[2] = -uxz;
    P[3] = -uxy;
    P[4] = 1.0f - uyy;
    P[5] = -uyz;
    P[6] = -uxz;
    P[7] = -uyz;
    P[8] = 1.0f - uzz;
}

// --- public API ---

void ml_add_ray(ml_accum_t* ml,
                const float32_t p[3],
                const float32_t u_in[3],
                float32_t w) {
    if (w < 0.0f)
        w = 0.0f;
    float32_t u[3];
    vec3_normalize(u_in, u);

    float32_t P[9];
    projector_from_u(u, P);

    mat3_add_scaled(ml->A, P, w > 0.0f ? w : 1.0f);
    mat3_vec3_add_scaled(ml->b, P, p, w > 0.0f ? w : 1.0f);
    ml->K++;
}


bool ml_solve(const ml_accum_t* ml, float32_t s_out[3]) {
    arm_matrix_instance_f32 A, Ainv, b, x;
    arm_mat_init_f32(&A, 3, 3, (float32_t*)ml->A);
    float32_t Ainv_buf[9];
    arm_mat_init_f32(&Ainv, 3, 3, Ainv_buf);
    float32_t x_buf[3];
    arm_mat_init_f32(&b, 3, 1, (float32_t*)ml->b);
    arm_mat_init_f32(&x, 3, 1, x_buf);
    if (arm_mat_inverse_f32(&A, &Ainv) != ARM_MATH_SUCCESS)
        return false;
    arm_mat_mult_f32(&Ainv, &b, &x);
    s_out[0] = x_buf[0];
    s_out[1] = x_buf[1];
    s_out[2] = x_buf[2];
    return true;
}

bool ml_solve_batch(const float32_t* poses,
                    const float32_t* dirs,
                    const float32_t* weights,
                    uint32_t K,
                    float32_t s_out[3]) {
    ml_accum_t ml;
    ml_init(&ml);
    for (uint32_t k = 0; k < K; k++) {
        const float32_t* p = &poses[3 * k];
        const float32_t* u = &dirs[3 * k];
        float32_t w = weights ? weights[k] : 1.0f;
        ml_add_ray(&ml, p, u, w);
    }
    return ml_solve(&ml, s_out);
}
