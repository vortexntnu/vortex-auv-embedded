#include "multilateration.h"
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "arm_math.h"
#include "dsp/matrix_functions.h"

static inline void vec3_normalize_arm(const float32_t in[3], float32_t out[3]) {
    float32_t n2;
    arm_dot_prod_f32(in, in, 3, &n2);
    if (n2 > 0.0f) {
        float32_t n;
        arm_sqrt_f32(n2, &n);
        float32_t invn = 1.0f / n;
        arm_scale_f32(in, invn, out, 3);
    } else {
        out[0] = 1.0f;
        out[1] = 0.0f;
        out[2] = 0.0f;
    }
}

static inline void mat3_add_scaled_arm(float32_t A[9],
                                       const float32_t B[9],
                                       float32_t w) {
    float32_t Bw[9];
    arm_scale_f32(B, (w > 0.0f ? w : 1.0f), Bw, 9);
    arm_add_f32(A, Bw, A, 9);
}

static inline void mat3_vec3_add_scaled_arm(float32_t b[3],
                                            const float32_t M[9],
                                            const float32_t v[3],
                                            float32_t w) {
    float32_t t[3];
    arm_matrix_instance_f32 MM, vv, tt;
    arm_mat_init_f32(&MM, 3, 3, (float32_t*)M);
    arm_mat_init_f32(&vv, 3, 1, (float32_t*)v);
    arm_mat_init_f32(&tt, 3, 1, t);
    arm_mat_mult_f32(&MM, &vv, &tt);

    arm_scale_f32(t, (w > 0.0f ? w : 1.0f), t, 3);
    arm_add_f32(b, t, b, 3);
}

static inline void projector_from_u_arm(const float32_t u[3], float32_t P[9]) {
    float32_t U[3] = {u[0], u[1], u[2]};
    float32_t UT[3] = {u[0], u[1], u[2]};
    float32_t UUT[9];
    float32_t I[9] = {1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f};

    arm_matrix_instance_f32 Um, UTm, UUTm, Im, Pm;
    arm_mat_init_f32(&Um, 3, 1, U);
    arm_mat_init_f32(&UTm, 1, 3, UT);
    arm_mat_init_f32(&UUTm, 3, 3, UUT);
    arm_mat_init_f32(&Im, 3, 3, I);
    arm_mat_init_f32(&Pm, 3, 3, P);

    arm_mat_mult_f32(&Um, &UTm, &UUTm);
    arm_mat_sub_f32(&Im, &UUTm, &Pm);
}

void multilateration_add_ray(struct multilateration_accumulator* ml,
                const float32_t p[3],
                const float32_t u_in[3],
                float32_t w) {
    float32_t u[3];
    vec3_normalize_arm(u_in, u);

    float32_t P[9];
    projector_from_u_arm(u, P);

    mat3_add_scaled_arm(ml->A, P, w);
    mat3_vec3_add_scaled_arm(ml->b, P, p, w);
    ml->K++;
}

bool multilateration_solve(const struct multilateration_accumulator* ml, float32_t s_out[3]) {
    float32_t A_buf[9];
    float32_t b_buf[3];
    memcpy(A_buf, ml->A, sizeof(A_buf));
    memcpy(b_buf, ml->b, sizeof(b_buf));

    arm_matrix_instance_f32 A, b, x;
    arm_mat_init_f32(&A, 3, 3, A_buf);
    arm_mat_init_f32(&b, 3, 1, b_buf);
    arm_mat_init_f32(&x, 3, 1, s_out);

    const float32_t lambda = 1e-6f;
    A_buf[0] += lambda;
    A_buf[4] += lambda;
    A_buf[8] += lambda;

    arm_status status = arm_mat_cholesky_f32(&A, &A);
    if (status != ARM_MATH_SUCCESS)
        return false;

    arm_mat_solve_lower_triangular_f32(&A, &b, &x);
    arm_mat_trans_f32(&A, &A);
    arm_mat_solve_upper_triangular_f32(&A, &x, &x);

    return true;
}

bool multilateration_solve_batch(const float32_t* poses,    // K×3
                    const float32_t* dirs,     // K×3
                    const float32_t* weights,  // nullable
                    uint32_t K,
                    float32_t s_out[3]) {
    struct multilateration_accumulator ml;
    memset(&ml, 0, sizeof(ml));
    for (uint32_t k = 0; k < K; k++) {
        const float32_t* p = &poses[3 * k];
        const float32_t* u = &dirs[3 * k];
        float32_t w = weights ? weights[k] : 1.0f;
        multilateration_add_ray(&ml, p, u, w);
    }
    return multilateration_solve(&ml, s_out);
}
