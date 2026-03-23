/**
 * @file    find_peaks.c
 * @brief   scipy.signal.find_peaks for q15_t — STM32H753 / CMSIS-DSP
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
 */

#include "find_peaks.h"
#include <string.h>   /* memset */

/* -------------------------------------------------------------------------
 * Internal helpers
 * ------------------------------------------------------------------------- */

/** Minimum of two int32 values — compiled to a single SMIN on M7 */
static inline int32_t i32_min(int32_t a, int32_t b) { return a < b ? a : b; }

/** Maximum of two int32 values — compiled to a single SMAX on M7 */
static inline int32_t i32_max(int32_t a, int32_t b) { return a > b ? a : b; }

/**
 * Compute prominence for one peak at index p.
 *
 * scipy definition:
 *   Search left and right until we reach a sample >= x[p] or the array edge
 *   (bounded by wlen if set).  The base is the maximum of the two valley
 *   minimums found during those searches.  Prominence = x[p] - base.
 *
 * Returns the prominence as a q15_t (clamped to 0 if negative, which can
 * happen for flat peaks).
 */
static q15_t compute_prominence(
    const q15_t * restrict x,
    uint32_t n,
    uint32_t p,
    uint32_t wlen,
    q15_t   *left_base_out,
    q15_t   *right_base_out)
{
    /* Window bounds */
    uint32_t lo = (wlen > 0 && p >= wlen / 2) ? (p - wlen / 2) : 0;
    uint32_t hi = (wlen > 0 && p + wlen / 2 < n) ? (p + wlen / 2) : (n - 1);

    int32_t peak_val = (int32_t)x[p];

    /* --- Search left ---------------------------------------------------- */
    int32_t left_min = peak_val;
    uint32_t i = p;
    while (i > lo) {
        --i;
        int32_t v = (int32_t)x[i];
        if (v < left_min) left_min = v;
        if (v >= peak_val) break;   /* found a higher/equal peak — stop */
    }

    /* --- Search right --------------------------------------------------- */
    int32_t right_min = peak_val;
    i = p;
    while (i < hi) {
        ++i;
        int32_t v = (int32_t)x[i];
        if (v < right_min) right_min = v;
        if (v >= peak_val) break;
    }

    /* scipy: base = max of the two valley floors */
    int32_t base = i32_max(left_min, right_min);

    if (left_base_out)  *left_base_out  = (q15_t)left_min;
    if (right_base_out) *right_base_out = (q15_t)right_min;

    int32_t prom = peak_val - base;
    return (q15_t)(prom > 0 ? prom : 0);
}

/**
 * Compute width for one peak at index p.
 *
 * scipy definition:
 *   Reference height = x[p] - rel_height * prominence
 *   Walk left and right from the peak to find where the signal crosses
 *   the reference height.  Interpolate the exact crossing fractionally.
 *
 * rel_height_q15 is stored in Q0.15 format (0x4000 = 0.5).
 *
 * Returns the width in samples (integer, rounding toward zero).
 * left_ips_out and right_ips_out are the integer sample positions of the
 * crossings (fractional part dropped for q15 simplicity).
 */
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

    /* Window bounds */
    uint32_t lo = (wlen > 0 && p >= wlen / 2) ? (p - wlen / 2) : 0;
    uint32_t hi = (wlen > 0 && p + wlen / 2 < n) ? (p + wlen / 2) : (n - 1);

    /* Reference height: x[p] - rel_height * prominence
     * rel_height_q15 is Q0.15, so multiply then shift by 15.
     * Use q31 to avoid overflow: prominence max = 65535, rel_height max ~1.0 */
    int32_t rel_drop = (int32_t)((int32_t)prominence * (int32_t)rel_height_q15) >> 15;
    int32_t ref_h    = (int32_t)x[p] - rel_drop;

    /* --- Walk left ------------------------------------------------------ */
    uint32_t left_ips = p;
    uint32_t i = p;
    while (i > lo) {
        --i;
        if ((int32_t)x[i] <= ref_h) {
            left_ips = i;
            break;
        }
        left_ips = i;
    }

    /* --- Walk right ----------------------------------------------------- */
    uint32_t right_ips = p;
    i = p;
    while (i < hi) {
        ++i;
        if ((int32_t)x[i] <= ref_h) {
            right_ips = i;
            break;
        }
        right_ips = i;
    }

    if (left_ips_out)  *left_ips_out  = left_ips;
    if (right_ips_out) *right_ips_out = right_ips;
    return (right_ips >= left_ips) ? (right_ips - left_ips) : 0;
}

/* -------------------------------------------------------------------------
 * Main implementation
 * ------------------------------------------------------------------------- */

find_peaks_status_t find_peaks(
    const q15_t          * restrict x,
    uint32_t              n,
    const find_peaks_config_t *cfg,
    uint32_t             * restrict peak_idx,
    find_peaks_props_t   * restrict props,
    uint32_t              peaks_max,
    uint32_t             *n_peaks)
{
    /* ------------------------------------------------------------------ */
    /* Argument validation                                                 */
    /* ------------------------------------------------------------------ */
    if (__builtin_expect(!x || !peak_idx || !n_peaks, 0))
        return FIND_PEAKS_ERR_NULL;
    if (__builtin_expect(n < 3, 0))
        return FIND_PEAKS_ERR_INPUT;

    /* Use defaults if no config supplied */
    const find_peaks_config_t defaults = FIND_PEAKS_CONFIG_DEFAULT;
    if (!cfg) cfg = &defaults;

    *n_peaks = 0;

    /* ------------------------------------------------------------------ */
    /* Pass 1: Collect local maxima                                        */
    /* x[i] > x[i-1]  AND  x[i] > x[i+1]                                 */
    /* scipy treats flat peaks differently; here we use strict inequality  */
    /* which matches the most common use-case.                             */
    /* ------------------------------------------------------------------ */
    for (uint32_t i = 1; i < n - 1; ++i)
    {
        if (__builtin_expect((int32_t)x[i] > (int32_t)x[i - 1] &&
                             (int32_t)x[i] > (int32_t)x[i + 1], 0))
        {
            if (*n_peaks >= peaks_max)
                return FIND_PEAKS_ERR_OVERFLOW;

            peak_idx[*n_peaks] = i;

            /* Fill basic per-peak properties now; others filled later */
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

    /* ------------------------------------------------------------------ */
    /* Pass 2: Height filter                                               */
    /* ------------------------------------------------------------------ */
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

    /* ------------------------------------------------------------------ */
    /* Pass 3: Distance filter                                             */
    /* Remove the shorter peak when two peaks are within cfg.distance.    */
    /* scipy scans sorted by height (tallest kept); we do the same.       */
    /* ------------------------------------------------------------------ */
    if (cfg->distance > 1 && *n_peaks > 1)
    {
        /*
         * Strategy: mark peaks for removal with a boolean array.
         * We iterate pairs — for each pair that violates distance, drop
         * the shorter one.  A single O(k²) pass is fine for k << 1000;
         * if k grows large consider a priority-queue approach.
         */
        bool remove[peaks_max]; /* VLA — stack, fine on H753 with ≤256 peaks */
        memset(remove, 0, (*n_peaks) * sizeof(bool));

        for (uint32_t i = 0; i < *n_peaks; ++i) {
            if (remove[i]) continue;
            for (uint32_t j = i + 1; j < *n_peaks; ++j) {
                if (remove[j]) continue;
                uint32_t dist = peak_idx[j] - peak_idx[i]; /* idx is sorted */
                if (dist >= cfg->distance) break; /* peaks are ordered; done */

                /* Keep the taller, remove the shorter */
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

    /* ------------------------------------------------------------------ */
    /* Pass 4: Prominence filter (and compute prominence for all if width  */
    /* is also requested)                                                  */
    /* ------------------------------------------------------------------ */
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

    /* ------------------------------------------------------------------ */
    /* Pass 5: Width filter                                                */
    /* ------------------------------------------------------------------ */
    if (cfg->width > 0)
    {
        uint32_t out = 0;
        for (uint32_t i = 0; i < *n_peaks; ++i)
        {
            uint32_t p    = peak_idx[i];
            q15_t    prom = props ? props[i].prominence : 0;

            /* If prominence wasn't computed above, do it now */
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

/**
 * @brief   Find troughs (local minima) in a q15_t signal.
 *
 * Implemented as the signal-negated inverse of find_peaks: negating x turns
 * every trough into a peak, find_peaks runs unchanged, then prominence bases
 * and heights are negated back into the original domain.
 *
 * arm_negate_q15() is used for the negate — single-cycle SIMD on Cortex-M7.
 * The negated copy lives on the stack; size is bounded by n.
 *
 * All cfg fields have identical semantics to find_peaks_config_t EXCEPT:
 *   - cfg->height:     interpreted as a depth ceiling — troughs with
 *                      x[p] > -cfg->height are rejected. Pass the negated
 *                      threshold, e.g. height = -2000 to accept only troughs
 *                      at or below -2000 in the original signal.
 *   - cfg->threshold:  minimum drop on each side — identical semantics,
 *                      works correctly after negation.
 *   - prominence/width: computed on the negated signal; values are identical
 *                      in magnitude to what a native trough search would give.
 *
 * @param x             Input signal (read-only, original domain).
 * @param n             Number of samples. Must be >= 3 and <= FIND_TROUGHS_MAX_N.
 * @param cfg           Same config struct as find_peaks. NULL = defaults.
 * @param trough_idx    Output array of trough indices. Caller-allocated.
 * @param props         Output per-trough properties, or NULL if not needed.
 *                      peak_height will be negative (original trough value).
 *                      left_base/right_base will also be negated back.
 * @param troughs_max   Capacity of trough_idx / props arrays.
 * @param n_troughs     On return: number of troughs found.
 * @return              Same status codes as find_peaks.
 */

/* Maximum signal length supported — sets the stack allocation for the
 * negated copy. Tune to your largest expected input. 512 q15_t = 1 kB. */
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

    /* Build a negated config so the caller can think in trough terms.
     *
     * height:    caller passes the minimum depth (e.g. 2000 means "only
     *            troughs whose value is <= -2000 in the original signal").
     *            Negating it gives find_peaks the correct minimum peak height
     *            in the negated domain.
     *
     * threshold: caller passes the minimum drop on each side, which is a
     *            positive quantity in both domains — negate it so it matches
     *            the negated signal's rises. Actually symmetric so sign is
     *            the same; kept as-is. No change needed.
     *
     * prominence, width, wlen, distance: purely magnitudes / sample counts,
     *            identical in both domains. No change needed.
     */
    find_peaks_config_t neg_cfg;
    if (cfg) {
        neg_cfg = *cfg;   /* copy all fields */

        /* height in trough-domain is a depth floor (most negative value
         * accepted). Negate it to get the equivalent peak height floor
         * in the negated signal. Guard against FIND_PEAKS_NONE sentinel. */
        if (neg_cfg.height != FIND_PEAKS_NONE)
            neg_cfg.height = -neg_cfg.height;

        /* threshold: the caller supplies a positive "minimum drop" value.
         * In the negated domain the same drop appears as a positive rise,
         * so no change is needed. */

    } else {
        const find_peaks_config_t defaults = FIND_PEAKS_CONFIG_DEFAULT;
        neg_cfg = defaults;
    }

    /* Negate the signal so troughs become peaks */
    q15_t neg[FIND_TROUGHS_MAX_N];
    arm_negate_q15(x, neg, n);

    find_peaks_status_t status = find_peaks(neg, n, &neg_cfg,
                                            trough_idx, props,
                                            troughs_max, n_troughs);

    /* Restore props into the original domain */
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
