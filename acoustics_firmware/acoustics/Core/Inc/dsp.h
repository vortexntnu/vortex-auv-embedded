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

#define DSP_MAX_BIQUADS (3)  // 6th-order IIR = 3 biquads

#ifndef DSP_MAX_BLOCK_SAMPLES
#define DSP_MAX_BLOCK_SAMPLES 512u
#endif

// Maximum FIR length you plan to use
#ifndef DSP_MAX_FIR_TAPS
#define DSP_MAX_FIR_TAPS 128u
#endif

#define SAMPLING_FREQUENCY 192000
#define DECIMATE_FACTOR 24
#define BLOCK_SIZE_IN 1024
#define NUM_TAPS 4
#define LOW_PASS_CUTOFF 450

#define PINGER_FREQUENCY 30000

struct dsp_context {
    // NCO (shared across all channels; keep in lockstep)
    float32_t dphase;        // 2*pi*f0/fs_in
    float32_t cos_d, sin_d;  // cos/sin(dphase)
    float32_t cos_p, sin_p;  // running oscillator state

    // IIR lowpass (same chain for I and Q)
    arm_biquad_cascade_df2T_instance_f32 iir_i;
    arm_biquad_cascade_df2T_instance_f32 iir_q;

    // FIR decimators (I and Q)
    arm_fir_decimate_instance_f32 fir_i;
    arm_fir_decimate_instance_f32 fir_q;

    float32_t iir_state_i[4 * DSP_MAX_BIQUADS];
    float32_t iir_state_q[4 * DSP_MAX_BIQUADS];

    // pState length must be (numTaps + blockSize - 1)
    float32_t fir_state_i[DSP_MAX_FIR_TAPS + DSP_MAX_BLOCK_SAMPLES - 1];
    float32_t fir_state_q[DSP_MAX_FIR_TAPS + DSP_MAX_BLOCK_SAMPLES - 1];

    const float32_t* mf_ref_i;  // time-reversed + conjugated replica
    const float32_t* mf_ref_q;  // time-reversed + conjugated replica
    uint32_t mf_len;            // in complex samples
};

void dsp_init(struct dsp_context* ctx,
              const float32_t* mf_ref_i,
              const float32_t* mf_ref_q,
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

void dsp_decimate(struct dsp_context* ctx,
                  const float32_t* in_i,
                  const float32_t* in_q,
                  float32_t* out_i,
                  float32_t* out_q,
                  uint32_t n);

uint32_t dsp_matched_filter(const struct dsp_context* ctx,
                            const float32_t* in_iq,
                            uint32_t len_in,
                            float32_t* out_corr,
                            float32_t* peak_val);

#ifdef __cplusplus
}
#endif

#endif  // DSP_H
