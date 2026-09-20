/**
 * @file    find_peaks.h
 * @brief   scipy.signal.find_peaks equivalent for q15_t and float32_t arrays
 *          on STM32H753 / CMSIS-DSP.
 *
 * Mirrors scipy's find_peaks API as closely as makes sense in C:
 *   peaks, properties = scipy.signal.find_peaks(x,
 *       height=None, threshold=None, distance=None,
 *       prominence=None, width=None, wlen=None, rel_height=0.5)
 *
 * Q15 usage (unchanged):
 *   find_peaks_config_t cfg = FIND_PEAKS_CONFIG_DEFAULT;
 *   cfg.height    = 500;
 *   cfg.distance  = 10;
 *   cfg.prominence = 200;
 *
 *   uint32_t peak_idx[MAX_PEAKS];
 *   find_peaks_props_t props[MAX_PEAKS];
 *   uint32_t n_peaks;
 *
 *   find_peaks_status_t s = find_peaks(signal, SIGNAL_LEN, &cfg,
 *                                      peak_idx, props, MAX_PEAKS, &n_peaks);
 *
 * F32 usage (new):
 *   find_peaks_config_f32_t cfg = FIND_PEAKS_CONFIG_F32_DEFAULT;
 *   cfg.height    = 0.5f;
 *   cfg.distance  = 10;
 *   cfg.prominence = 0.1f;
 *
 *   uint32_t peak_idx[MAX_PEAKS];
 *   find_peaks_props_f32_t props[MAX_PEAKS];
 *   uint32_t n_peaks;
 *
 *   find_peaks_status_t s = find_peaks_f32(signal, SIGNAL_LEN, &cfg,
 *                                          peak_idx, props, MAX_PEAKS, &n_peaks);
 */

#ifndef FIND_PEAKS_H
#define FIND_PEAKS_H

#include <stdint.h>
#include <stdbool.h>
#include "arm_math.h"   /* q15_t, q31_t, float32_t — from CMSIS-DSP */

#ifdef __cplusplus
extern "C" {
#endif

/* =========================================================================
 * Shared return status  (used by both q15 and f32 variants)
 * ========================================================================= */

typedef enum {
    FIND_PEAKS_OK            =  0,
    FIND_PEAKS_ERR_NULL      = -1,  /**< null pointer argument              */
    FIND_PEAKS_ERR_INPUT     = -2,  /**< signal length < 3 or too large     */
    FIND_PEAKS_ERR_OVERFLOW  = -3,  /**< more peaks than peaks_max          */
} find_peaks_status_t;


/* =========================================================================
 * Q15 API  (unchanged from original)
 * ========================================================================= */

/** Sentinel: parameter is disabled / not set (q15 variant) */
#define FIND_PEAKS_NONE    ((q15_t)INT16_MIN)

typedef struct {
    q15_t    height;            /**< min absolute height; FIND_PEAKS_NONE = off  */
    q15_t    threshold;         /**< min vertical drop to both neighbours         */
    uint32_t distance;          /**< min horizontal distance between peaks        */
    q15_t    prominence;        /**< min prominence; FIND_PEAKS_NONE = off        */
    uint32_t width;             /**< min width in samples; 0 = off                */
    q15_t    rel_height_q15;    /**< Q0.15 fraction for width measurement (0x4000 = 0.5) */
    uint32_t wlen;              /**< search window for prominence/width; 0 = full */
} find_peaks_config_t;

#define FIND_PEAKS_CONFIG_DEFAULT {     \
    .height         = FIND_PEAKS_NONE, \
    .threshold      = FIND_PEAKS_NONE, \
    .distance       = 0,               \
    .prominence     = FIND_PEAKS_NONE, \
    .width          = 0,               \
    .rel_height_q15 = 0x4000,          \
    .wlen           = 0,               \
}

typedef struct {
    q15_t    peak_height;       /**< x[peak_idx]                            */
    q15_t    left_threshold;    /**< x[peak] - x[peak-1]                    */
    q15_t    right_threshold;   /**< x[peak] - x[peak+1]                    */
    q15_t    prominence;        /**< computed prominence (0 if not computed) */
    q15_t    left_base;         /**< valley floor on the left               */
    q15_t    right_base;        /**< valley floor on the right              */
    uint32_t left_ips;          /**< left  interpolated peak-width sample   */
    uint32_t right_ips;         /**< right interpolated peak-width sample   */
    uint32_t width_samples;     /**< right_ips - left_ips                   */
} find_peaks_props_t;

/**
 * @brief Find peaks in a q15_t signal.
 */
find_peaks_status_t find_peaks(
    const q15_t               *x,
    uint32_t                   n,
    const find_peaks_config_t *cfg,
    uint32_t                  *peak_idx,
    find_peaks_props_t        *props,
    uint32_t                   peaks_max,
    uint32_t                  *n_peaks
);

/**
 * @brief Find troughs (local minima) in a q15_t signal.
 */
find_peaks_status_t find_troughs(
    const q15_t               * restrict x,
    uint32_t                    n,
    const find_peaks_config_t  *cfg,
    uint32_t                  * restrict trough_idx,
    find_peaks_props_t        * restrict props,
    uint32_t                    troughs_max,
    uint32_t                   *n_troughs
);


/* =========================================================================
 * F32 API  (new)
 * ========================================================================= */

/** Sentinel: parameter is disabled / not set (f32 variant) */
#define FIND_PEAKS_NONE_F32    (-3.402823466e+38f)   /* -FLT_MAX */

/**
 * @brief  Configuration for find_peaks_f32 / find_troughs_f32.
 *
 * Field semantics are identical to find_peaks_config_t except:
 *   - height, threshold, prominence are float32_t (no fixed-point scaling).
 *   - rel_height is a plain float in [0.0, 1.0] (scipy default 0.5f).
 *   - Use FIND_PEAKS_NONE_F32 to disable height / threshold / prominence.
 */
typedef struct {
    float32_t height;       /**< min absolute height; FIND_PEAKS_NONE_F32 = off  */
    float32_t threshold;    /**< min vertical drop to both neighbours             */
    uint32_t  distance;     /**< min horizontal distance between peaks            */
    float32_t prominence;   /**< min prominence; FIND_PEAKS_NONE_F32 = off        */
    uint32_t  width;        /**< min width in samples; 0 = off                    */
    float32_t rel_height;   /**< fraction of prominence for width measurement     */
    uint32_t  wlen;         /**< search window for prominence/width; 0 = full     */
} find_peaks_config_f32_t;

/** Default config matching scipy defaults (f32 variant) */
#define FIND_PEAKS_CONFIG_F32_DEFAULT {         \
    .height     = FIND_PEAKS_NONE_F32,          \
    .threshold  = FIND_PEAKS_NONE_F32,          \
    .distance   = 0,                            \
    .prominence = FIND_PEAKS_NONE_F32,          \
    .width      = 0,                            \
    .rel_height = 0.5f,                         \
    .wlen       = 0,                            \
}

/**
 * @brief  Per-peak properties for find_peaks_f32 / find_troughs_f32.
 *
 * Field meanings are identical to find_peaks_props_t but carry float32_t
 * values, so no negation or scaling artefacts from fixed-point arithmetic.
 */
typedef struct {
    float32_t peak_height;      /**< x[peak_idx]                            */
    float32_t left_threshold;   /**< x[peak] - x[peak-1]                    */
    float32_t right_threshold;  /**< x[peak] - x[peak+1]                    */
    float32_t prominence;       /**< computed prominence (0 if not computed) */
    float32_t left_base;        /**< valley floor on the left               */
    float32_t right_base;       /**< valley floor on the right              */
    uint32_t  left_ips;         /**< left  interpolated peak-width sample   */
    uint32_t  right_ips;        /**< right interpolated peak-width sample   */
    uint32_t  width_samples;    /**< right_ips - left_ips                   */
} find_peaks_props_f32_t;

/**
 * @brief Find peaks in a float32_t signal.
 *
 * @param[in]  x          Input signal (float32_t array, length n)
 * @param[in]  n          Number of samples (must be >= 3)
 * @param[in]  cfg        Filter configuration (NULL uses all-disabled defaults)
 * @param[out] peak_idx   Output array of peak indices (caller allocates)
 * @param[out] props      Output properties per peak (caller allocates, may be NULL)
 * @param[in]  peaks_max  Maximum entries in peak_idx / props
 * @param[out] n_peaks    Number of peaks found
 *
 * @return FIND_PEAKS_OK on success, negative error code otherwise.
 *
 * Complexity mirrors the q15 variant:
 *   Pass 1 (local max scan)  : O(n)
 *   Pass 2 (height/threshold): O(k)
 *   Pass 3 (distance)        : O(k²) worst case
 *   Pass 4 (prominence)      : O(k·wlen)
 *   Pass 5 (width)           : O(k·wlen)
 */
find_peaks_status_t find_peaks_f32(
    const float32_t               *x,
    uint32_t                       n,
    const find_peaks_config_f32_t *cfg,
    uint32_t                      *peak_idx,
    find_peaks_props_f32_t        *props,
    uint32_t                       peaks_max,
    uint32_t                      *n_peaks
);

/**
 * @brief Find troughs (local minima) in a float32_t signal.
 *
 * Semantics are identical to find_troughs (q15 variant).
 * cfg->height is interpreted as a minimum depth magnitude: only troughs
 * where x[p] <= -height are accepted.  Pass FIND_PEAKS_NONE_F32 to disable.
 */
find_peaks_status_t find_troughs_f32(
    const float32_t               * restrict x,
    uint32_t                        n,
    const find_peaks_config_f32_t  *cfg,
    uint32_t                       * restrict trough_idx,
    find_peaks_props_f32_t         * restrict props,
    uint32_t                        troughs_max,
    uint32_t                       *n_troughs
);

#ifdef __cplusplus
}
#endif

#endif /* FIND_PEAKS_H */
