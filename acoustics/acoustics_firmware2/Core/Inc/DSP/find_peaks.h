/**
 * @file    find_peaks.h
 * @brief   scipy.signal.find_peaks equivalent for q15_t arrays on STM32H753
 *
 * Mirrors scipy's find_peaks API as closely as makes sense in C:
 *   peaks, properties = scipy.signal.find_peaks(x,
 *       height=None, threshold=None, distance=None,
 *       prominence=None, width=None, wlen=None, rel_height=0.5)
 *
 * Usage:
 *   find_peaks_config_t cfg = FIND_PEAKS_CONFIG_DEFAULT;
 *   cfg.height    = 500;      // min absolute height (q15_t units)
 *   cfg.distance  = 10;       // min samples between peaks
 *   cfg.prominence = 200;     // min prominence
 *
 *   uint32_t peak_idx[MAX_PEAKS];
 *   find_peaks_props_t props[MAX_PEAKS];
 *   uint32_t n_peaks;
 *
 *   find_peaks_status_t s = find_peaks(signal, SIGNAL_LEN, &cfg,
 *                                      peak_idx, props, MAX_PEAKS, &n_peaks);
 */

#ifndef FIND_PEAKS_H
#define FIND_PEAKS_H

#include <stdint.h>
#include <stdbool.h>
#include "arm_math.h"   /* q15_t, q31_t — from CMSIS-DSP */

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------
 * Constants
 * ------------------------------------------------------------------------- */

/** Sentinel: parameter is disabled / not set */
#define FIND_PEAKS_NONE    ((q15_t)INT16_MIN)

/* -------------------------------------------------------------------------
 * Configuration struct  (mirrors scipy keyword arguments)
 * ------------------------------------------------------------------------- */

typedef struct {
    /**
     * height — minimum required height of peaks.
     *   scipy: height=scalar  → peaks where x[peak] >= height
     *   Set to FIND_PEAKS_NONE to disable.
     */
    q15_t height;

    /**
     * threshold — minimum vertical distance to neighbours.
     *   scipy: threshold=scalar  → x[peak] - x[peak±1] >= threshold
     *   Set to FIND_PEAKS_NONE to disable.
     */
    q15_t threshold;

    /**
     * distance — minimum horizontal distance between peaks (samples).
     *   scipy: distance=int
     *   When two peaks are closer than this, the shorter one is removed.
     *   Set to 0 to disable.
     */
    uint32_t distance;

    /**
     * prominence — minimum prominence of peaks.
     *   scipy: prominence=scalar
     *   Prominence = peak height minus the highest of the two surrounding
     *   valley minima (searching outward to the next higher peak or array edge).
     *   Set to FIND_PEAKS_NONE to disable (skips the O(n²) prominence pass).
     */
    q15_t prominence;

    /**
     * width — minimum peak width at rel_height fraction of prominence.
     *   scipy: width=scalar, rel_height=0.5 (half-prominence by default)
     *   Set to 0 to disable.
     */
    uint32_t width;

    /**
     * rel_height — fraction of prominence at which width is measured.
     *   scipy: rel_height=0.5  (value between 0.0 and 1.0, stored as Q15)
     *   0x4000 = 0.5 in Q15 (the scipy default).
     */
    q15_t rel_height_q15;

    /**
     * wlen — samples to search when computing prominence / width.
     *   scipy: wlen=int
     *   0 = search the full array (same as scipy's default None).
     */
    uint32_t wlen;

} find_peaks_config_t;

/**
 * Default config matching scipy defaults:
 *   height=None, threshold=None, distance=None, prominence=None,
 *   width=None, rel_height=0.5, wlen=None
 */
#define FIND_PEAKS_CONFIG_DEFAULT {     \
    .height        = FIND_PEAKS_NONE,  \
    .threshold     = FIND_PEAKS_NONE,  \
    .distance      = 0,                \
    .prominence    = FIND_PEAKS_NONE,  \
    .width         = 0,                \
    .rel_height_q15 = 0x4000,          \
    .wlen          = 0,                \
}

/* -------------------------------------------------------------------------
 * Per-peak properties  (mirrors scipy's "properties" dict return value)
 * ------------------------------------------------------------------------- */

typedef struct {
    q15_t    peak_height;       /**< x[peak_idx]                           */
    q15_t    left_threshold;    /**< x[peak] - x[peak-1]                   */
    q15_t    right_threshold;   /**< x[peak] - x[peak+1]                   */
    q15_t    prominence;        /**< computed prominence (0 if not computed)*/
    q15_t    left_base;         /**< valley floor on the left               */
    q15_t    right_base;        /**< valley floor on the right              */
    uint32_t left_ips;          /**< left  interpolated peak-width sample   */
    uint32_t right_ips;         /**< right interpolated peak-width sample   */
    uint32_t width_samples;     /**< right_ips - left_ips                   */
} find_peaks_props_t;

/* -------------------------------------------------------------------------
 * Return status
 * ------------------------------------------------------------------------- */

typedef enum {
    FIND_PEAKS_OK            = 0,
    FIND_PEAKS_ERR_NULL      = -1,  /**< null pointer argument              */
    FIND_PEAKS_ERR_INPUT     = -2,  /**< signal length < 3 or its too large */
    FIND_PEAKS_ERR_OVERFLOW  = -3,  /**< more peaks than peaks_max          */
} find_peaks_status_t;

/* -------------------------------------------------------------------------
 * Main API
 * ------------------------------------------------------------------------- */

/**
 * @brief Find peaks in a q15_t signal.
 *
 * @param[in]  x          Input signal (q15_t array, length n)
 * @param[in]  n          Number of samples
 * @param[in]  cfg        Filter configuration (NULL uses all-disabled defaults)
 * @param[out] peak_idx   Output array of peak indices (caller allocates)
 * @param[out] props      Output properties per peak (caller allocates, may be NULL)
 * @param[in]  peaks_max  Maximum entries in peak_idx / props
 * @param[out] n_peaks    Number of peaks found
 *
 * @return FIND_PEAKS_OK on success, negative error code otherwise.
 *
 * Complexity:
 *   Pass 1 (local max scan)  : O(n)
 *   Pass 2 (height/threshold): O(k)    k = candidate peaks after pass 1
 *   Pass 3 (distance)        : O(k²)   worst case, fast in practice
 *   Pass 4 (prominence)      : O(k·wlen)
 *   Pass 5 (width)           : O(k·wlen)
 */
find_peaks_status_t find_peaks(
    const q15_t          *x,
    uint32_t              n,
    const find_peaks_config_t *cfg,
    uint32_t             *peak_idx,
    find_peaks_props_t   *props,
    uint32_t              peaks_max,
    uint32_t             *n_peaks
);

find_peaks_status_t find_troughs(
    const q15_t          * restrict x,
    uint32_t               n,
    const find_peaks_config_t *cfg,
    uint32_t             * restrict trough_idx,
    find_peaks_props_t   * restrict props,
    uint32_t               troughs_max,
    uint32_t              *n_troughs
);

#ifdef __cplusplus
}
#endif

#endif /* FIND_PEAKS_H */
