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
    float32_t dphase;    // 2*pi*f0/fs_in
    q15_t cos_d, sin_d;  // cos/sin(dphase)
    q15_t cos_p, sin_p;  // running oscillator state

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

/**
 * @brief Initialize DSP context for matched filtering.
 *
 * Sets up internal state and stores the time-reversed reference signal.
 *
 * @param[out] ctx       Pointer to DSP context to initialize.
 * @param[in]  mf_ref_i  Pointer to Q15 I (real) part of the time-reversed reference.
 * @param[in]  mf_ref_q  Pointer to Q15 Q (imag) part of the time-reversed reference.
 * @param[in]  mf_len    Number of samples in the reference.
 */
void dsp_init(struct dsp_context* ctx,
              const q15_t* mf_ref_i,
              const q15_t* mf_ref_q,
              uint32_t mf_len);

/**
 * @brief Mixes Q15 RF samples to complex baseband (I/Q).
 *
 * @param[in,out] ctx         DSP context (NCO phase, scaling, etc.).
 * @param[in]     in          Q15 RF input, length @p n.
 * @param[out]    i_out       Q15 in-phase (I, real) output, length @p n.
 * @param[out]    q_out       Q15 quadrature (Q, imag) output, length @p n.
 * @param[in]     size           Number of samples to process.
 */
void dsp_mix_to_baseband_q15(struct dsp_context* ctx,
                             const q15_t* restrict raw_samples,
                             q15_t* restrict out_i,
                             q15_t* restrict out_q,
                             uint32_t size);

/**
 * TODO: add documentation
 * However since we the FIR decimate might decimate 
 * in the same step, this filter might not be used
 */
void dsp_lpf_6th_butterworth_q15(struct dsp_context* ctx,
                                 const q15_t* restrict io_i,
                                 const q15_t* restrict io_q,
                                 q15_t* restrict out_i,
                                 q15_t* restrict out_q,
                                 uint32_t size);

/**
 * @brief Low-pass FIR + decimate complex Q15 I/Q.
 *
 * Processes @p n_in input samples per channel and returns @p n_out = floor(size
 * / D), where D is the decimation factor configured in @p ctx.
 *
 * @param[in,out] ctx     FIR/decimator context (taps, delay line, decimation D,
 * scaling).
 * @param[in]     i_in    Q15 I input, length @p size.
 * @param[in]     q_in    Q15 Q input, length @p size.
 * @param[out]    i_out   Q15 I output, capacity >= floor(@p size / D).
 * @param[out]    q_out   Q15 Q output, capacity >= floor(@p size / D).
 * @param[in]     size    Number of input samples per channel.
 */
void dsp_fir_decimate_q15(struct dsp_context* ctx,
                          const q15_t* restrict in_i,
                          const q15_t* restrict in_q,
                          q15_t* restrict out_i,
                          q15_t* restrict out_q,
                          uint32_t size);
/**
 * @brief Complex matched filter (Q15) for a single window on Cortex-M7.
 *
 * Computes one complex correlation sample
 *
 *     y = sum_{n=0}^{N-1} x[n] * h*[n]
 *
 * @param[in]  xi    Pointer to Q15 I (real) input samples, length N.
 * @param[in]  xq    Pointer to Q15 Q (imag) input samples, length N.
 * @param[in]  hi    Pointer to Q15 I part of pre-reversed & conjugated replica,
 * length N.
 * @param[in]  hq    Pointer to Q15 Q part of pre-reversed & conjugated replica,
 * length N.
 * @param[in]  N     Number of samples in the input window and replica.
 * @param[out] outI  Pointer to single Q15 output for the real (I) part of y.
 * @param[out] outQ  Pointer to single Q15 output for the imag (Q) part of y.
 */
void dsp_matched_filter_q15(const q15_t* restrict xi,
                            const q15_t* restrict xq,
                            const q15_t* restrict hi,
                            const q15_t* restrict hq,
                            uint32_t N,
                            q15_t* outI,
                            q15_t* outQ);

#ifdef __cplusplus
}
#endif

#endif  // DSP_H
