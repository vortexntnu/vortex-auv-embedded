#ifndef MULTILATERATION_H
#define MULTILATERATION_H

#include <stdbool.h>
#include <stdint.h>
#include "arm_math.h"

#ifdef __cplusplus
extern "C" {
#endif

struct ml_accumulator {
    float32_t A[9]; 
    float32_t b[3];
    uint32_t K;   
};

void ml_init(struct ml_accumulator* ml);

void ml_add_ray(struct ml_accumulator* ml,
                const float32_t p[3],   
                const float32_t u_in[3],
                float32_t w);          

bool ml_solve(const struct ml_accumulator* ml, float32_t s_out[3]);

bool ml_solve_batch(const float32_t* poses,   
                    const float32_t* dirs,   
                    const float32_t* weights, 
                    uint32_t K,
                    float32_t s_out[3]);

#ifdef __cplusplus
}
#endif

#endif  // MULTILATERATION_H
