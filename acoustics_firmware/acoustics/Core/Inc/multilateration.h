#ifndef MULTILATERATION_H
#define MULTILATERATION_H

#include <stdbool.h>
#include <stdint.h>
#include "arm_math.h"

#ifdef __cplusplus
extern "C" {
#endif

struct multilateration_accumulator {
    float32_t A[9]; 
    float32_t b[3];
    uint32_t K;   
};

void multilateration_init(struct multilateration_accumulator* ml);

void multilateration_add_ray(struct multilateration_accumulator* ml,
                const float32_t p[3],   
                const float32_t u_in[3],
                float32_t w);          

bool multilateration_solve(const struct multilateration_accumulator* ml, float32_t s_out[3]);

bool multilateration_solve_batch(const float32_t* poses,   
                    const float32_t* dirs,   
                    const float32_t* weights, 
                    uint32_t K,
                    float32_t s_out[3]);

#ifdef __cplusplus
}
#endif

#endif  // MULTILATERATION_H
