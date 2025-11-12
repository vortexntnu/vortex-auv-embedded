#ifndef PRESSURE_CALC_H
#define PRESSURE_CALC_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdio.h>

// Internal ring buffer for rolling sigma
struct ring_stats {
    float* buf;
    size_t cap;
    size_t head;
    size_t count;
    float sum;
    float sumsq;
};

struct leak_conf {
    // Sampling + filters
    float sample_hz;
    float lp_tau_s;  // low-pass time constant for P,T
    float b_tau_s;   // slow background EWMA time constant
    // Rolling stats for sigma_e
    float sigma_window_s;
    // Thresholds
    float shewhart_k;           // z-threshold
    float shewhart_min_hold_s;  // require sustain
    float cusum_k_sigma;        // reference k in units of sigma
    float cusum_h_sigma;        // decision h in units of sigma
    float slow_alarm_min_s;     // persistence for slow alarm
    // Guards
    float tprime_mask_abs;  // ignore decisions when |t'| exceeds (e.g., 0.01
                            // 1/s ~= 0.6 %/min)
    float mask_relax_s;     // duration to relax thresholds after mask event
    // Hard rate backstop (optional; set <=0 to disable)
    float hard_e_abs;  // absolute e backstop in 1/s (e.g., 0.0005 -> 0.05%/s)
};

// Detector state
struct leak_det {
    struct leak_conf cfg;

    // Filters
    float alpha_lp;  // EMA coeff for P,T
    float alpha_b;   // EWMA coeff for background b
    float dt;        // 1/sample_hz

    // State
    bool inited;
    float P_lp, T_lp;  // low-pass filtered P,T
    float P_prev, T_prev;
    float b;                    // slow background estimate
    struct ring_stats e_stats;  // rolling stats for sigma_e

    // Derivative / residuals (latest)
    float pprime, tprime, rprime, e, sigma_e, z;

    // Shewhart
    int shewhart_hold_needed;  // samples required to sustain
    int shewhart_hold_count;

    // CUSUM
    float cusum_k;  // absolute (k = cfg.cusum_k_sigma * sigma_e)
    float cusum_h;  // absolute (h = cfg.cusum_h_sigma * sigma_e)
    float Cplus, Cminus;
    int slow_alarm_persist_needed;  // samples
    int slow_alarm_persist_count;

    // Masking
    int relax_countdown;  // samples left to relax after |t'| spike

    // Outputs
    bool fast_leak_alarm;
    bool slow_leak_alarm;
};

int leakdet_init(struct leak_det* ld, struct leak_conf* config);
void leakdet_update(struct leak_det* ld,
                    float P,
                    float T,
                    bool* fast_alarm,
                    bool* slow_alarm);

#ifdef __cplusplus
}
#endif

#endif