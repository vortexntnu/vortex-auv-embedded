#ifndef DSP_H
#define DSP_H

#include <stddef.h>
#include <stdint.h>
#include "arm_math.h"
#include "dsp/filtering_functions.h"
#include "stm32h7xx_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DSP_MAX_BIQUADS (3)    // 6th-order IIR = 3 biquads
#define DSP_DECIM_FACTOR (32)  // 192 kS/s -> 6 kS/s

struct dsp_context {
    // Rates & frequency plan
    float32_t fs_in;  // e.g., 192000.0f
    float32_t f0;     // e.g., 30000.0f
    float32_t fc_lp;  // e.g., 450.0f

    // NCO (shared across all channels; keep in lockstep)
    float32_t dphase;        // 2*pi*f0/fs_in
    float32_t cos_d, sin_d;  // cos/sin(dphase)
    float32_t cos_p, sin_p;  // running oscillator state

    // IIR lowpass (same chain for I and Q)
    arm_biquad_cascade_df2T_instance_f32 iir_i;
    arm_biquad_cascade_df2T_instance_f32 iir_q;
    arm_fir_decimate_instance_f32 fir_i;
    arm_fir_decimate_instance_f32 fir_q;
    float32_t biquad_coeffs[5 * DSP_MAX_BIQUADS];
    float32_t iir_state_i[4 * DSP_MAX_BIQUADS];
    float32_t iir_state_q[4 * DSP_MAX_BIQUADS];
    uint32_t num_biquads;

    // Matched filter reference (complex, interleaved [re,im,...])
    const float32_t* mf_ref;  // time-reversed + conjugated replica
    uint32_t mf_len;          // in complex samples

    // Decimation factor
    uint32_t decim;  // e.g., 32
};

void dsp_init(struct dsp_context* ctx,
              float32_t fs_in,
              float32_t f0,
              float32_t fc_lp,
              uint32_t decim,
              const float32_t* mf_ref,
              uint32_t mf_len);

void dsp_mix_to_baseband_i16(struct dsp_context* ctx,
                             const int16_t* raw_samples,
                             float32_t* out_i,
                             float32_t* out_q,
                             uint32_t n);

void dsp_lpf_6th_butterworth(struct dsp_context* ctx,
                             const float32_t* io_i,
                             const float32_t* io_q,
                             float32_t* out_i,
                             float32_t* out_q,
                             uint32_t n);

uint32_t dsp_decimate_pickM(const float32_t* in_i,
                            const float32_t* in_q,
                            float32_t* out_i,
                            float32_t* out_q,
                            uint32_t n,
                            uint32_t M);

uint32_t dsp_matched_filter(const struct dsp_context* ctx,
                            const float32_t* in_iq,
                            uint32_t len_in,
                            float32_t* out_corr,
                            float32_t* peak_val);

#ifdef __cplusplus
}
#endif

#endif  // DSP_H
