/**
 * @file    find_peaks.c
 * @brief   scipy.signal.find_peaks for q15_t and float32_t — STM32H753 / CMSIS-DSP
 *
 * Algorithm follows scipy's implementation:
 *   https://github.com/scipy/scipy/blob/main/scipy/signal/_peak_finding.py
 *
 * Notes on STM32H753 / Cortex-M7 optimisations used here:
 *   - All inner loops use q31_t arithmetic for intermediate values to avoid
 *     overflow without a saturation instruction per step.  The M7 has a
 *     64-bit accumulator, so widening is free.
 *   - __builtin_expect() guides branch prediction for the filter hot-paths.
 *   - restrict pointers allow the compiler to auto-vectorise the scan.
 *   - No heap allocation — all working state lives on the stack (or in
 *     caller-supplied buffers).  Safe for FreeRTOS tasks with known stack.
 *   - If you place the signal buffer in DTCM you will get the full benefit
 *     of zero-wait-state access during the O(n) scan.
 *   - The f32 variant uses the FPU directly via float32_t; the Cortex-M7
 *     FPU handles all comparisons and arithmetic in single-cycle throughput.
 */

#include "find_peaks.h"
#include <string.h>   /* memset */

/* =========================================================================
 * Shared helpers
 * ========================================================================= */

static inline int32_t i32_min(int32_t a, int32_t b) { return a < b ? a : b; }
static inline int32_t i32_max(int32_t a, int32_t b) { return a > b ? a : b; }

static inline float32_t f32_min(float32_t a, float32_t b) { return a < b ? a : b; }
static inline float32_t f32_max(float32_t a, float32_t b) { return a > b ? a : b; }


/* =========================================================================
 * Q15 internal helpers  (unchanged)
 * ========================================================================= */

static q15_t compute_prominence(
    const q15_t * restrict x,
    uint32_t n,
    uint32_t p,
    uint32_t wlen,
    q15_t   *left_base_out,
    q15_t   *right_base_out)
{
    uint32_t lo = (wlen > 0 && p >= wlen / 2) ? (p - wlen / 2) : 0;
    uint32_t hi = (wlen > 0 && p + wlen / 2 < n) ? (p + wlen / 2) : (n - 1);

    int32_t peak_val = (int32_t)x[p];

    int32_t left_min = peak_val;
    uint32_t i = p;
    while (i > lo) {
        --i;
        int32_t v = (int32_t)x[i];
        if (v < left_min) left_min = v;
        if (v >= peak_val) break;
    }

    int32_t right_min = peak_val;
    i = p;
    while (i < hi) {
        ++i;
        int32_t v = (int32_t)x[i];
        if (v < right_min) right_min = v;
        if (v >= peak_val) break;
    }

    int32_t base = i32_max(left_min, right_min);

    if (left_base_out)  *left_base_out  = (q15_t)left_min;
    if (right_base_out) *right_base_out = (q15_t)right_min;

    int32_t prom = peak_val - base;
    return (q15_t)(prom > 0 ? prom : 0);
}

static uint32_t compute_width(
    const q15_t * restrict x,
    uint32_t n,
    uint32_t p,
    q15_t    prominence,
    q15_t    rel_height_q15,
    uint32_t wlen,
    uint32_t *left_ips_out,
    uint32_t *right_ips_out)
{
    if (prominence <= 0) {
        if (left_ips_out)  *left_ips_out  = p;
        if (right_ips_out) *right_ips_out = p;
        return 0;
    }

    uint32_t lo = (wlen > 0 && p >= wlen / 2) ? (p - wlen / 2) : 0;
    uint32_t hi = (wlen > 0 && p + wlen / 2 < n) ? (p + wlen / 2) : (n - 1);

    int32_t rel_drop = (int32_t)((int32_t)prominence * (int32_t)rel_height_q15) >> 15;
    int32_t ref_h    = (int32_t)x[p] - rel_drop;

    uint32_t left_ips = p;
    uint32_t i = p;
    while (i > lo) {
        --i;
        if ((int32_t)x[i] <= ref_h) { left_ips = i; break; }
        left_ips = i;
    }

    uint32_t right_ips = p;
    i = p;
    while (i < hi) {
        ++i;
        if ((int32_t)x[i] <= ref_h) { right_ips = i; break; }
        right_ips = i;
    }

    if (left_ips_out)  *left_ips_out  = left_ips;
    if (right_ips_out) *right_ips_out = right_ips;
    return (right_ips >= left_ips) ? (right_ips - left_ips) : 0;
}


/* =========================================================================
 * Q15 public API  (unchanged)
 * ========================================================================= */

find_peaks_status_t find_peaks(
    const q15_t          * restrict x,
    uint32_t              n,
    const find_peaks_config_t *cfg,
    uint32_t             * restrict peak_idx,
    find_peaks_props_t   * restrict props,
    uint32_t              peaks_max,
    uint32_t             *n_peaks)
{
    if (__builtin_expect(!x || !peak_idx || !n_peaks, 0))
        return FIND_PEAKS_ERR_NULL;
    if (__builtin_expect(n < 3, 0))
        return FIND_PEAKS_ERR_INPUT;

    const find_peaks_config_t defaults = FIND_PEAKS_CONFIG_DEFAULT;
    if (!cfg) cfg = &defaults;

    *n_peaks = 0;

    /* Pass 1: Local maxima */
    for (uint32_t i = 1; i < n - 1; ++i)
    {
        if (__builtin_expect((int32_t)x[i] > (int32_t)x[i - 1] &&
                             (int32_t)x[i] > (int32_t)x[i + 1], 0))
        {
            if (*n_peaks >= peaks_max)
                return FIND_PEAKS_ERR_OVERFLOW;

            peak_idx[*n_peaks] = i;

            if (props) {
                find_peaks_props_t *pr = &props[*n_peaks];
                memset(pr, 0, sizeof(*pr));
                pr->peak_height     = x[i];
                pr->left_threshold  = (q15_t)((int32_t)x[i] - (int32_t)x[i - 1]);
                pr->right_threshold = (q15_t)((int32_t)x[i] - (int32_t)x[i + 1]);
            }

            (*n_peaks)++;
        }
    }

    if (*n_peaks == 0) return FIND_PEAKS_OK;

    /* Pass 2: Height / threshold filter */
    if (cfg->height != FIND_PEAKS_NONE || cfg->threshold != FIND_PEAKS_NONE)
    {
        uint32_t out = 0;
        for (uint32_t i = 0; i < *n_peaks; ++i)
        {
            uint32_t p = peak_idx[i];
            bool keep = true;

            if (cfg->height != FIND_PEAKS_NONE &&
                (int32_t)x[p] < (int32_t)cfg->height)
                keep = false;

            if (keep && cfg->threshold != FIND_PEAKS_NONE) {
                q15_t lt = (q15_t)((int32_t)x[p] - (int32_t)x[p - 1]);
                q15_t rt = (q15_t)((int32_t)x[p] - (int32_t)x[p + 1]);
                if (lt < cfg->threshold || rt < cfg->threshold)
                    keep = false;
            }

            if (keep) {
                peak_idx[out] = p;
                if (props && out != i) props[out] = props[i];
                out++;
            }
        }
        *n_peaks = out;
        if (*n_peaks == 0) return FIND_PEAKS_OK;
    }

    /* Pass 3: Distance filter */
    if (cfg->distance > 1 && *n_peaks > 1)
    {
        bool remove[peaks_max];
        memset(remove, 0, (*n_peaks) * sizeof(bool));

        for (uint32_t i = 0; i < *n_peaks; ++i) {
            if (remove[i]) continue;
            for (uint32_t j = i + 1; j < *n_peaks; ++j) {
                if (remove[j]) continue;
                uint32_t dist = peak_idx[j] - peak_idx[i];
                if (dist >= cfg->distance) break;

                if ((int32_t)x[peak_idx[i]] >= (int32_t)x[peak_idx[j]])
                    remove[j] = true;
                else
                    remove[i] = true;
            }
        }

        uint32_t out = 0;
        for (uint32_t i = 0; i < *n_peaks; ++i) {
            if (!remove[i]) {
                peak_idx[out] = peak_idx[i];
                if (props && out != i) props[out] = props[i];
                out++;
            }
        }
        *n_peaks = out;
        if (*n_peaks == 0) return FIND_PEAKS_OK;
    }

    /* Pass 4: Prominence filter */
    bool need_prom = (cfg->prominence != FIND_PEAKS_NONE) ||
                     (cfg->width > 0 && props);

    if (need_prom)
    {
        uint32_t out = 0;
        for (uint32_t i = 0; i < *n_peaks; ++i)
        {
            uint32_t p = peak_idx[i];
            q15_t left_base = 0, right_base = 0;

            q15_t prom = compute_prominence(x, n, p,
                                            cfg->wlen,
                                            &left_base, &right_base);

            if (props) {
                props[i].prominence  = prom;
                props[i].left_base   = left_base;
                props[i].right_base  = right_base;
            }

            bool keep = true;
            if (cfg->prominence != FIND_PEAKS_NONE &&
                prom < cfg->prominence)
                keep = false;

            if (keep) {
                peak_idx[out] = p;
                if (props && out != i) props[out] = props[i];
                out++;
            }
        }
        *n_peaks = out;
        if (*n_peaks == 0) return FIND_PEAKS_OK;
    }

    /* Pass 5: Width filter */
    if (cfg->width > 0)
    {
        uint32_t out = 0;
        for (uint32_t i = 0; i < *n_peaks; ++i)
        {
            uint32_t p    = peak_idx[i];
            q15_t    prom = props ? props[i].prominence : 0;

            if (!need_prom)
                prom = compute_prominence(x, n, p, cfg->wlen, NULL, NULL);

            uint32_t left_ips = 0, right_ips = 0;
            uint32_t w = compute_width(x, n, p, prom,
                                       cfg->rel_height_q15,
                                       cfg->wlen,
                                       &left_ips, &right_ips);

            if (props) {
                props[i].width_samples = w;
                props[i].left_ips      = left_ips;
                props[i].right_ips     = right_ips;
            }

            if (w >= cfg->width) {
                peak_idx[out] = p;
                if (props && out != i) props[out] = props[i];
                out++;
            }
        }
        *n_peaks = out;
    }

    return FIND_PEAKS_OK;
}

/* Maximum signal length for the q15 trough negated copy */
#define FIND_TROUGHS_MAX_N  512u

find_peaks_status_t find_troughs(
    const q15_t          * restrict x,
    uint32_t               n,
    const find_peaks_config_t *cfg,
    uint32_t             * restrict trough_idx,
    find_peaks_props_t   * restrict props,
    uint32_t               troughs_max,
    uint32_t              *n_troughs)
{
    if (__builtin_expect(n > FIND_TROUGHS_MAX_N, 0))
        return FIND_PEAKS_ERR_INPUT;

    find_peaks_config_t neg_cfg;
    if (cfg) {
        neg_cfg = *cfg;
        if (neg_cfg.height != FIND_PEAKS_NONE)
            neg_cfg.height = -neg_cfg.height;
    } else {
        const find_peaks_config_t defaults = FIND_PEAKS_CONFIG_DEFAULT;
        neg_cfg = defaults;
    }

    q15_t neg[FIND_TROUGHS_MAX_N];
    arm_negate_q15(x, neg, n);

    find_peaks_status_t status = find_peaks(neg, n, &neg_cfg,
                                            trough_idx, props,
                                            troughs_max, n_troughs);

    if (status == FIND_PEAKS_OK && props)
    {
        for (uint32_t i = 0; i < *n_troughs; ++i)
        {
            find_peaks_props_t *pr = &props[i];
            pr->peak_height = x[trough_idx[i]];
            pr->left_base   = -pr->left_base;
            pr->right_base  = -pr->right_base;
        }
    }

    return status;
}


/* =========================================================================
 * F32 internal helpers
 * ========================================================================= */

/**
 * Compute prominence for one peak at index p (f32 variant).
 *
 * Logic is identical to the q15 version; float32_t arithmetic replaces
 * int32_t widening.  The Cortex-M7 FPU evaluates comparisons and
 * min/max in a single cycle.
 */
static float32_t compute_prominence_f32(
    const float32_t * restrict x,
    uint32_t n,
    uint32_t p,
    uint32_t wlen,
    float32_t *left_base_out,
    float32_t *right_base_out)
{
    uint32_t lo = (wlen > 0 && p >= wlen / 2) ? (p - wlen / 2) : 0;
    uint32_t hi = (wlen > 0 && p + wlen / 2 < n) ? (p + wlen / 2) : (n - 1);

    float32_t peak_val = x[p];

    /* Search left */
    float32_t left_min = peak_val;
    uint32_t i = p;
    while (i > lo) {
        --i;
        float32_t v = x[i];
        if (v < left_min) left_min = v;
        if (v >= peak_val) break;
    }

    /* Search right */
    float32_t right_min = peak_val;
    i = p;
    while (i < hi) {
        ++i;
        float32_t v = x[i];
        if (v < right_min) right_min = v;
        if (v >= peak_val) break;
    }

    /* scipy: base = max of the two valley floors */
    float32_t base = f32_max(left_min, right_min);

    if (left_base_out)  *left_base_out  = left_min;
    if (right_base_out) *right_base_out = right_min;

    float32_t prom = peak_val - base;
    return (prom > 0.0f) ? prom : 0.0f;
}

/**
 * Compute width for one peak at index p (f32 variant).
 *
 * rel_height is a plain float in [0.0, 1.0] — no Q0.15 scaling needed.
 */
static uint32_t compute_width_f32(
    const float32_t * restrict x,
    uint32_t n,
    uint32_t p,
    float32_t prominence,
    float32_t rel_height,
    uint32_t  wlen,
    uint32_t *left_ips_out,
    uint32_t *right_ips_out)
{
    if (prominence <= 0.0f) {
        if (left_ips_out)  *left_ips_out  = p;
        if (right_ips_out) *right_ips_out = p;
        return 0;
    }

    uint32_t lo = (wlen > 0 && p >= wlen / 2) ? (p - wlen / 2) : 0;
    uint32_t hi = (wlen > 0 && p + wlen / 2 < n) ? (p + wlen / 2) : (n - 1);

    /* Reference height = x[p] - rel_height * prominence */
    float32_t ref_h = x[p] - rel_height * prominence;

    /* Walk left */
    uint32_t left_ips = p;
    uint32_t i = p;
    while (i > lo) {
        --i;
        if (x[i] <= ref_h) { left_ips = i; break; }
        left_ips = i;
    }

    /* Walk right */
    uint32_t right_ips = p;
    i = p;
    while (i < hi) {
        ++i;
        if (x[i] <= ref_h) { right_ips = i; break; }
        right_ips = i;
    }

    if (left_ips_out)  *left_ips_out  = left_ips;
    if (right_ips_out) *right_ips_out = right_ips;
    return (right_ips >= left_ips) ? (right_ips - left_ips) : 0;
}


/* =========================================================================
 * F32 public API
 * ========================================================================= */

find_peaks_status_t find_peaks_f32(
    const float32_t               * restrict x,
    uint32_t                        n,
    const find_peaks_config_f32_t  *cfg,
    uint32_t                       * restrict peak_idx,
    find_peaks_props_f32_t         * restrict props,
    uint32_t                        peaks_max,
    uint32_t                       *n_peaks)
{
    /* ------------------------------------------------------------------ */
    /* Argument validation                                                 */
    /* ------------------------------------------------------------------ */
    if (__builtin_expect(!x || !peak_idx || !n_peaks, 0))
        return FIND_PEAKS_ERR_NULL;
    if (__builtin_expect(n < 3, 0))
        return FIND_PEAKS_ERR_INPUT;

    const find_peaks_config_f32_t defaults = FIND_PEAKS_CONFIG_F32_DEFAULT;
    if (!cfg) cfg = &defaults;

    *n_peaks = 0;

    /* ------------------------------------------------------------------ */
    /* Pass 1: Collect local maxima                                        */
    /* ------------------------------------------------------------------ */
    for (uint32_t i = 1; i < n - 1; ++i)
    {
        if (__builtin_expect(x[i] > x[i - 1] && x[i] > x[i + 1], 0))
        {
            if (*n_peaks >= peaks_max)
                return FIND_PEAKS_ERR_OVERFLOW;

            peak_idx[*n_peaks] = i;

            if (props) {
                find_peaks_props_f32_t *pr = &props[*n_peaks];
                memset(pr, 0, sizeof(*pr));
                pr->peak_height     = x[i];
                pr->left_threshold  = x[i] - x[i - 1];
                pr->right_threshold = x[i] - x[i + 1];
            }

            (*n_peaks)++;
        }
    }

    if (*n_peaks == 0) return FIND_PEAKS_OK;

    /* ------------------------------------------------------------------ */
    /* Pass 2: Height / threshold filter                                   */
    /* ------------------------------------------------------------------ */
    if (cfg->height != FIND_PEAKS_NONE_F32 || cfg->threshold != FIND_PEAKS_NONE_F32)
    {
        uint32_t out = 0;
        for (uint32_t i = 0; i < *n_peaks; ++i)
        {
            uint32_t p = peak_idx[i];
            bool keep = true;

            if (cfg->height != FIND_PEAKS_NONE_F32 &&
                x[p] < cfg->height)
                keep = false;

            if (keep && cfg->threshold != FIND_PEAKS_NONE_F32) {
                float32_t lt = x[p] - x[p - 1];
                float32_t rt = x[p] - x[p + 1];
                if (lt < cfg->threshold || rt < cfg->threshold)
                    keep = false;
            }

            if (keep) {
                peak_idx[out] = p;
                if (props && out != i) props[out] = props[i];
                out++;
            }
        }
        *n_peaks = out;
        if (*n_peaks == 0) return FIND_PEAKS_OK;
    }

    /* ------------------------------------------------------------------ */
    /* Pass 3: Distance filter                                             */
    /* ------------------------------------------------------------------ */
    if (cfg->distance > 1 && *n_peaks > 1)
    {
        bool remove[peaks_max];
        memset(remove, 0, (*n_peaks) * sizeof(bool));

        for (uint32_t i = 0; i < *n_peaks; ++i) {
            if (remove[i]) continue;
            for (uint32_t j = i + 1; j < *n_peaks; ++j) {
                if (remove[j]) continue;
                uint32_t dist = peak_idx[j] - peak_idx[i];
                if (dist >= cfg->distance) break;

                /* Keep the taller, remove the shorter */
                if (x[peak_idx[i]] >= x[peak_idx[j]])
                    remove[j] = true;
                else
                    remove[i] = true;
            }
        }

        uint32_t out = 0;
        for (uint32_t i = 0; i < *n_peaks; ++i) {
            if (!remove[i]) {
                peak_idx[out] = peak_idx[i];
                if (props && out != i) props[out] = props[i];
                out++;
            }
        }
        *n_peaks = out;
        if (*n_peaks == 0) return FIND_PEAKS_OK;
    }

    /* ------------------------------------------------------------------ */
    /* Pass 4: Prominence filter                                           */
    /* ------------------------------------------------------------------ */
    bool need_prom = (cfg->prominence != FIND_PEAKS_NONE_F32) ||
                     (cfg->width > 0 && props);

    if (need_prom)
    {
        uint32_t out = 0;
        for (uint32_t i = 0; i < *n_peaks; ++i)
        {
            uint32_t p = peak_idx[i];
            float32_t left_base = 0.0f, right_base = 0.0f;

            float32_t prom = compute_prominence_f32(x, n, p,
                                                    cfg->wlen,
                                                    &left_base, &right_base);

            if (props) {
                props[i].prominence  = prom;
                props[i].left_base   = left_base;
                props[i].right_base  = right_base;
            }

            bool keep = true;
            if (cfg->prominence != FIND_PEAKS_NONE_F32 &&
                prom < cfg->prominence)
                keep = false;

            if (keep) {
                peak_idx[out] = p;
                if (props && out != i) props[out] = props[i];
                out++;
            }
        }
        *n_peaks = out;
        if (*n_peaks == 0) return FIND_PEAKS_OK;
    }

    /* ------------------------------------------------------------------ */
    /* Pass 5: Width filter                                                */
    /* ------------------------------------------------------------------ */
    if (cfg->width > 0)
    {
        uint32_t out = 0;
        for (uint32_t i = 0; i < *n_peaks; ++i)
        {
            uint32_t  p    = peak_idx[i];
            float32_t prom = props ? props[i].prominence : 0.0f;

            if (!need_prom)
                prom = compute_prominence_f32(x, n, p, cfg->wlen, NULL, NULL);

            uint32_t left_ips = 0, right_ips = 0;
            uint32_t w = compute_width_f32(x, n, p, prom,
                                           cfg->rel_height,
                                           cfg->wlen,
                                           &left_ips, &right_ips);

            if (props) {
                props[i].width_samples = w;
                props[i].left_ips      = left_ips;
                props[i].right_ips     = right_ips;
            }

            if (w >= cfg->width) {
                peak_idx[out] = p;
                if (props && out != i) props[out] = props[i];
                out++;
            }
        }
        *n_peaks = out;
    }

    return FIND_PEAKS_OK;
}

/**
 * find_troughs_f32
 *
 * Negates the signal on the stack so troughs become peaks, runs
 * find_peaks_f32, then restores the props into the original domain.
 *
 * arm_negate_f32() maps to VNEG.F32 on Cortex-M7 — single-cycle throughput.
 */
#define FIND_TROUGHS_F32_MAX_N  512u

find_peaks_status_t find_troughs_f32(
    const float32_t               * restrict x,
    uint32_t                        n,
    const find_peaks_config_f32_t  *cfg,
    uint32_t                       * restrict trough_idx,
    find_peaks_props_f32_t         * restrict props,
    uint32_t                        troughs_max,
    uint32_t                       *n_troughs)
{
    if (__builtin_expect(n > FIND_TROUGHS_F32_MAX_N, 0))
        return FIND_PEAKS_ERR_INPUT;

    /* Build a negated config so the caller thinks in trough-domain terms.
     * height: caller passes a positive magnitude (e.g. 0.5 means "troughs
     * at or below -0.5").  Negate it so find_peaks_f32 sees the correct
     * minimum peak height in the negated signal.
     * All other fields are magnitude/count — no change needed. */
    find_peaks_config_f32_t neg_cfg;
    if (cfg) {
        neg_cfg = *cfg;
        if (neg_cfg.height != FIND_PEAKS_NONE_F32)
            neg_cfg.height = -neg_cfg.height;
    } else {
        const find_peaks_config_f32_t defaults = FIND_PEAKS_CONFIG_F32_DEFAULT;
        neg_cfg = defaults;
    }

    /* Negate the signal — VNEG.F32 via arm_negate_f32 */
    float32_t neg[FIND_TROUGHS_F32_MAX_N];
    arm_negate_f32(x, neg, n);

    find_peaks_status_t status = find_peaks_f32(neg, n, &neg_cfg,
                                                trough_idx, props,
                                                troughs_max, n_troughs);

    /* Restore props into the original domain */
    if (status == FIND_PEAKS_OK && props)
    {
        for (uint32_t i = 0; i < *n_troughs; ++i)
        {
            find_peaks_props_f32_t *pr = &props[i];
            pr->peak_height = x[trough_idx[i]];  /* original (negative) value */
            pr->left_base   = -pr->left_base;
            pr->right_base  = -pr->right_base;
            /* prominence and thresholds are magnitudes — leave positive */
        }
    }

    return status;
}
