// #include "wsen_pads_port_sercom3.h"
//  temperature T. Model: r' = dP/P - dT/T; estimate slow background b (hull
//  compliance/depth drift), residual e = r' - b ~ dn/n, then Shewhart + CUSUM
//  on e.

#include "pressure_calc.h"
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "definitions.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Reasonable defaults
static inline struct leak_conf leak_conf_defaults(void) {
    struct leak_conf c = {
        .sample_hz = 5.0,
        .lp_tau_s = 8.0,
        .b_tau_s = 1200.0,         // 20 min
        .sigma_window_s = 1200.0,  // 20 min
        .shewhart_k = 4.0,
        .shewhart_min_hold_s = 3.0,
        .cusum_k_sigma = 0.5,
        .cusum_h_sigma = 5.0,
        .slow_alarm_min_s = 300.0,  // 5 min
        .tprime_mask_abs = 0.01,  // 1%/s in normalized terms would be 0.01 1/s
        .mask_relax_s = 20.0,
        .hard_e_abs = 0.0};
    return c;
}

static bool ring_init(struct ring_stats* r, size_t cap) {
    r->buf = (float*)calloc(cap, sizeof(float));
    if (!r->buf)
        return false;
    r->cap = cap;
    r->head = 0;
    r->count = 0;
    r->sum = 0.0;
    r->sumsq = 0.0;
    return true;
}

static void ring_push(struct ring_stats* r, float x) {
    if (r->count < r->cap) {
        r->buf[r->head++] = x;
        r->sum += x;
        r->sumsq += x * x;
        r->count++;
        if (r->head == r->cap)
            r->head = 0;
    } else {
        // overwrite oldest
        size_t idx = r->head;
        float old = r->buf[idx];
        r->buf[idx] = x;
        r->sum += x - old;
        r->sumsq += x * x - old * old;
        r->head = (r->head + 1) % r->cap;
    }
}

static float ring_std(const struct ring_stats* r) {
    if (r->count < 2)
        return 0.0;
    float n = (float)r->count;
    float mu = r->sum / n;
    float var = fmax(0.0, (r->sumsq - n * mu * mu) / (n - 1.0));
    return sqrt(var);
}

// Utility for EMA alpha given tau and dt
static inline float ema_alpha(float tau_s, float dt_s) {
    // matched to first-order RC: alpha = dt/(tau+dt)
    if (tau_s <= 0.0)
        return 1.0;  // no filtering
    return dt_s / (tau_s + dt_s);
}

/**
 * @brief Initialize a leak_det instance from a configuration.
 *
 * @param ld Pointer to the leak_det structure to initialize (must be non-NULL).
 * @param config Optional pointer to a leak_conf; if NULL, default configuration
 * is used.
 *
 * @return 0 on success, -1 if initialization failed.
 */
int leakdet_init(struct leak_det* ld, struct leak_conf* config) {
    struct leak_conf cfg = (config ? *config : leak_conf_defaults());
    memset(ld, 0, sizeof(*ld));
    ld->cfg = cfg;
    ld->dt = 1.0 / cfg.sample_hz;
    ld->alpha_lp = ema_alpha(cfg.lp_tau_s, ld->dt);
    ld->alpha_b = ema_alpha(cfg.b_tau_s, ld->dt);
    ld->shewhart_hold_needed =
        (int)ceil(cfg.shewhart_min_hold_s * cfg.sample_hz);
    ld->slow_alarm_persist_needed =
        (int)ceil(cfg.slow_alarm_min_s * cfg.sample_hz);

    size_t cap = (size_t)fmax(10.0, ceil(cfg.sigma_window_s * cfg.sample_hz));
    if (!ring_init(&ld->e_stats, cap))
        return -1;

    ld->inited = false;
    return 0;
}

/**
 * @brief Process a single pressure/temperature sample, update the algorithm
 * state in ld, refresh filtered/derived values and alarm timers, and produce
 * alarm outputs.
 *
 * @param ld Pointer to the pressure-calculation state/context to be updated
 * (mutable).
 * @param P  New pressure sample (kPa).
 * @param T  New temperature sample (°C).
 * @param[out] fast_alarm  Set to non-zero when an immediate/fast pressure alarm
 * condition is detected.
 * @param[out] slow_alarm  Set to non-zero when a slower/longer-term pressure
 * alarm condition is detected.
 */
void leakdet_update(struct leak_det* ld,
                    float P,
                    float T,
                    bool* fast_alarm,
                    bool* slow_alarm) {
    /* Convert input units to internal units used by the detector */
    /* Pressure: kPa -> Pa */
    P = P * 1000.0f;
    /* Temperature: degC -> K */
    T = T + 273.15f;

    if (!ld->inited) {
        ld->P_lp = P;
        ld->T_lp = T;
        ld->P_prev = P;
        ld->T_prev = T;
        ld->b = 0.0;
        ld->sigma_e = 1e-6;  // small non-zero to start
        ld->cusum_k = ld->cfg.cusum_k_sigma * ld->sigma_e;
        ld->cusum_h = ld->cfg.cusum_h_sigma * ld->sigma_e;
        ld->Cplus = ld->Cminus = 0.0;
        ld->shewhart_hold_count = 0;
        ld->slow_alarm_persist_count = 0;
        ld->relax_countdown = 0;
        ld->inited = true;
        if (fast_alarm)
            *fast_alarm = false;
        if (slow_alarm)
            *slow_alarm = false;
        return;
    }

    const float dt = ld->dt;

    // --- EMA low-pass
    ld->P_lp = ld->P_lp + ld->alpha_lp * (P - ld->P_lp);
    ld->T_lp = ld->T_lp + ld->alpha_lp * (T - ld->T_lp);

    // --- Derivatives (first-order difference on filtered signal)
    float dP = (ld->P_lp - ld->P_prev) / dt;
    float dT = (ld->T_lp - ld->T_prev) / dt;
    ld->P_prev = ld->P_lp;
    ld->T_prev = ld->T_lp;

    // Guard: avoid division by zero
    float P_for_norm = (fabs(ld->P_lp) < 1e-6 ? 1e-6 : ld->P_lp);
    float T_for_norm = (fabs(ld->T_lp) < 1e-6 ? 1e-6 : ld->T_lp);

    // --- Normalized rates
    ld->pprime = dP / P_for_norm;  // 1/s
    ld->tprime = dT / T_for_norm;  // 1/s
    ld->rprime = ld->pprime - ld->tprime;

    // --- Background follower b (very slow EWMA of r')
    ld->b = (1.0 - ld->alpha_b) * ld->b + ld->alpha_b * ld->rprime;

    // --- Residual ~ leak term
    ld->e = ld->rprime - ld->b;

    // --- Rolling sigma_e
    ring_push(&ld->e_stats, ld->e);
    ld->sigma_e = ring_std(&ld->e_stats);
    if (ld->sigma_e < 1e-9)
        ld->sigma_e = 1e-9;  // floor

    // Update CUSUM thresholds based on *current* sigma
    ld->cusum_k = ld->cfg.cusum_k_sigma * ld->sigma_e;
    ld->cusum_h = ld->cfg.cusum_h_sigma * ld->sigma_e;

    // Z-score for Shewhart
    ld->z = ld->e / ld->sigma_e;

    // --- Masking around big thermal transients
    bool masked = false;
    if (fabs(ld->tprime) > ld->cfg.tprime_mask_abs) {
        ld->relax_countdown =
            (int)ceil(ld->cfg.mask_relax_s * ld->cfg.sample_hz);
    }
    if (ld->relax_countdown > 0) {
        masked = true;
        ld->relax_countdown--;
    }

    // --- Shewhart (fast)
    bool fast = false;
    if (!masked) {
        bool cond = fabs(ld->z) >= ld->cfg.shewhart_k;
        if (ld->cfg.hard_e_abs > 0.0)
            cond = cond || (fabs(ld->e) >= ld->cfg.hard_e_abs);
        if (cond) {
            if (++ld->shewhart_hold_count >= ld->shewhart_hold_needed) {
                fast = true;
            }
        } else {
            ld->shewhart_hold_count = 0;
        }
    } else {
        // While masked, do not accumulate hold
        ld->shewhart_hold_count = 0;
    }

    // --- CUSUM (slow)
    bool slow = false;
    if (!masked) {
        // one-sided CUSUMs
        ld->Cplus = fmax(0.0, ld->Cplus + (ld->e - ld->cusum_k));
        ld->Cminus = fmax(0.0, ld->Cminus - (ld->e + ld->cusum_k));

        bool trip = (ld->Cplus >= ld->cusum_h) || (ld->Cminus >= ld->cusum_h);
        if (trip) {
            if (++ld->slow_alarm_persist_count >=
                ld->slow_alarm_persist_needed) {
                slow = true;
            }
        } else {
            // decay persistence when below threshold
            if (ld->slow_alarm_persist_count > 0)
                ld->slow_alarm_persist_count--;
            // optional: small leakage of CUSUM over long time to avoid latching
            float leak =
                0.0;  // set e.g. to 0.001*cusum_h per sample if desired
            if (ld->Cplus > 0.0)
                ld->Cplus = fmax(0.0, ld->Cplus - leak);
            if (ld->Cminus > 0.0)
                ld->Cminus = fmax(0.0, ld->Cminus - leak);
        }
    } else {
        // During mask, don't accumulate CUSUM
        // (Optionally, you could still run but widen k/h.)
    }

    ld->fast_leak_alarm = fast;
    ld->slow_leak_alarm = slow;
    if (fast_alarm)
        *fast_alarm = fast;
    if (slow_alarm)
        *slow_alarm = slow;
}

volatile bool leakdet_tick = false;  // Set to true every 0.2s (5Hz frequency)

static void tc1_cb(TC_TIMER_STATUS status, uintptr_t context) {
    (void)context;
    leakdet_tick = true;
}

/**
 * @brief Initializes a timer that sets leakdet_tick to true at a frequency of
 * 5Hz. Used in main to call leakdet_update at the correct frequency.
 */
void timing_tc1_init_5hz(void) {
    TC1_TimerCallbackRegister(tc1_cb, 0);
    TC1_TimerStart();
}
