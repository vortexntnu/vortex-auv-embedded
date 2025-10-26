#ifndef DSP_H
#define DSP_H

#include <stddef.h>
#include <stdint.h>
#include "arm_math.h"
#include "arm_math_types.h"
#include "dsp/filtering_functions.h"
#include "stm32h7xx_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DSP_MAX_BIQUADS (3)  // 6th-order IIR = 3 biquads
#define DSP_MAX_BLOCK_SAMPLES 512u
#define DSP_MAX_FIR_TAPS 128u

#define SAMPLING_FREQUENCY 192000
#define BLOCK_SIZE_IN 1024
#define LOW_PASS_CUTOFF 450
#define DECIMATE_FACTOR 48
#define NUM_TAPS 97

#define PINGER_FREQUENCY 30000

struct dsp_context {
    // NCO (shared across all channels; keep in lockstep)
    float32_t dphase;        // 2*pi*f0/fs_in
    float32_t cos_d, sin_d;  // cos/sin(dphase)
    float32_t cos_p, sin_p;  // running oscillator state

    // IIR lowpass (same chain for I and Q)
    arm_biquad_casd_df1_inst_q15 iir_i;
    arm_biquad_casd_df1_inst_q15 iir_q;

    // FIR decimators (I and Q)
    arm_fir_decimate_instance_q15 fir_i;
    arm_fir_decimate_instance_q15 fir_q;

    q15_t iir_state_i[4 * DSP_MAX_BIQUADS];
    q15_t iir_state_q[4 * DSP_MAX_BIQUADS];

    // pState length must be (numTaps + blockSize - 1)
    q15_t fir_state_i[DSP_MAX_FIR_TAPS + DSP_MAX_BLOCK_SAMPLES - 1];
    q15_t fir_state_q[DSP_MAX_FIR_TAPS + DSP_MAX_BLOCK_SAMPLES - 1];

    const q15_t* mf_ref_i;  // time-reversed + conjugated replica
    const q15_t* mf_ref_q;  // time-reversed + conjugated replica
    uint32_t mf_len;        // in complex samples
};

void dsp_init(struct dsp_context* ctx,
              const q15_t* mf_ref_i,
              const q15_t* mf_ref_q,
              uint32_t mf_len);

void dsp_mix_to_baseband_i16(struct dsp_context* ctx,
                             const int16_t* raw_samples,
                             float32_t* out_i,
                             float32_t* out_q,
                             uint32_t size);


void dsp_lpf_6th_butterworth(struct dsp_context* ctx,
                             const q15_t* restrict io_i,
                             const q15_t* restrict io_q,
                             q15_t* restrict out_i,
                             q15_t* restrict out_q,
                             uint32_t size);


void dsp_decimate_q15(struct dsp_context* ctx,
                      const q15_t* restrict in_i,
                      const q15_t* restrict in_q,
                      q15_t* restrict out_i,
                      q15_t* restrict out_q,
                      uint32_t size);

uint32_t dsp_matched_filter(const struct dsp_context* ctx,
                            const float32_t* restrict in_i,
                            const float32_t* restrict in_q,
                            uint32_t len_in,
                            float32_t* restrict out_corr,
                            float32_t* peak_val,
                            float32_t* opt_peak_re,
                            float32_t* opt_peak_im);

#ifdef __cplusplus
}
#endif

#endif  // DSP_H
