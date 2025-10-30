#ifndef PRESSURE_CALC_H
#define PRESSURE_CALC_H

#ifdef __cplusplus
extern "C" {
#endif
#include <stdbool.h>
typedef struct {
    float sample_hz;
    float lp_tau_s;
    float b_tau_s;
    float sigma_window_s;
    float shewhart_k;
    float shewhart_min_hold_s;
    float cusum_k_sigma;
    float cusum_h_sigma;
    float slow_alarm_min_s;
    float tprime_mask_abs;
    float mask_relax_s;
    float hard_e_abs;
} LeakConf;

typedef struct LeakDet LeakDet;
typedef struct LeakConf;
int leakdet_init(LeakDet* ld, LeakConf* config);

#ifdef __cplusplus
}
#endif

#endif