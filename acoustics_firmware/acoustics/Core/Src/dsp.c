#include "dsp.h"
#include <stdint.h>
#include "arm_math_types.h"
#include "dsp/filtering_functions.h"

// ======= 6th-order Butterworth LPF @ fs=192k, fc=450 Hz =======
// SOS coeffs for CMSIS DF2T: {b0,b1,b2,a1,a2} per biquad.
// If you change fs/fc, recompute these!
static const q15_t BUTTER6_COEFFS_SOS[5 * DSP_MAX_BIQUADS] = {0};

static const q15_t FIR_Q15_TAPS[97] = {0};

void dsp_init(struct dsp_context* ctx,
              const q15_t* mf_ref_i,
              const q15_t* mf_ref_q,
              uint32_t mf_len) {
    memset(ctx, 0, sizeof(*ctx));

    ctx->mf_ref_i = mf_ref_i;
    ctx->mf_ref_q = mf_ref_q;
    ctx->mf_len = mf_len;

    ctx->dphase = 2.0f * PI * ((float)PINGER_FREQUENCY / SAMPLING_FREQUENCY);
    arm_sin_cos_f32(ctx->dphase, &ctx->sin_d, &ctx->cos_d);
    ctx->cos_p = 1.0f;
    ctx->sin_p = 0.0f;

    arm_biquad_cascade_df1_init_q15(&ctx->iir_i, DSP_MAX_BIQUADS,
                                    BUTTER6_COEFFS_SOS, ctx->iir_state_i, 0);
    arm_biquad_cascade_df1_init_q15(&ctx->iir_q, DSP_MAX_BIQUADS,
                                    BUTTER6_COEFFS_SOS, ctx->iir_state_q, 0);

    arm_fir_decimate_init_q15(&ctx->fir_i, (uint16_t)NUM_TAPS,
                              (uint8_t)DECIMATE_FACTOR, FIR_Q15_TAPS,
                              ctx->fir_state_i, BLOCK_SIZE_IN);

    arm_fir_decimate_init_q15(&ctx->fir_q, (uint16_t)NUM_TAPS,
                              (uint8_t)DECIMATE_FACTOR, FIR_Q15_TAPS,
                              ctx->fir_state_q, BLOCK_SIZE_IN);
}

// ======= Mix int16 (Q15) to complex baseband float32 =======
void dsp_mix_to_baseband_i16(struct dsp_context* ctx,
                             const int16_t* raw_samples,
                             float32_t* out_i,
                             float32_t* out_q,
                             uint32_t size) {
    const float32_t S = 1.0f / 32768.0f;

    float32_t cos_p = ctx->cos_p;
    float32_t sin_p = ctx->sin_p;
    const float32_t cos_d = ctx->cos_d;
    const float32_t sin_d = ctx->sin_d;

    for (uint32_t k = 0; k < size; k++) {
        float32_t x = (float32_t)*raw_samples++ * S;

        // Complex mix by e^{-j 2π f0 t}: I = x*cos, Q = x*(-sin)
        *out_i++ = x * cos_p;
        *out_q++ = x * -sin_p;

        float32_t c = cos_p * cos_d - sin_p * sin_d;
        float32_t s = sin_p * cos_d + cos_p * sin_d;
        cos_p = c;
        sin_p = s;
    }

    ctx->cos_p = cos_p;
    ctx->sin_p = sin_p;
}

void dsp_lpf_6th_butterworth(struct dsp_context* ctx,
                             const q15_t* restrict io_i,
                             const q15_t* restrict io_q,
                             q15_t* restrict out_i,
                             q15_t* restrict out_q,
                             uint32_t size) {
    arm_biquad_cascade_df1_fast_q15(&ctx->iir_i, io_i, out_i, size);
    arm_biquad_cascade_df1_fast_q15(&ctx->iir_q, io_q, out_q, size);
}

void dsp_decimate_q15(struct dsp_context* ctx,
                      const q15_t* restrict in_i,
                      const q15_t* restrict in_q,
                      q15_t* restrict out_i,
                      q15_t* restrict out_q,
                      uint32_t size) {
    arm_fir_decimate_q15(&ctx->fir_i, in_i, out_i, size);
    arm_fir_decimate_q15(&ctx->fir_q, in_q, out_q, size);
}

uint32_t dsp_matched_filter_q15(const struct dsp_context* ctx,
                                const q15_t* restrict in_i,
                                const q15_t* restrict in_q,
                                uint32_t len_in,
                                q15_t* restrict out_corr,
                                q15_t* peak_val,
                                q15_t* opt_peak_re,
                                q15_t* opt_peak_im) {
    const uint32_t L = ctx->mf_len;
    if (L == 0u || len_in < L) {
        if (peak_val)
            *peak_val = 0.0f;
        return 0u;
    }

    const uint32_t out_len = len_in - L + 1u;
    const q15_t* restrict hi = ctx->mf_ref_i;
    const q15_t* restrict hq = ctx->mf_ref_q;

    float32_t best = -FLT_MAX;
    uint32_t best_k = 0u;
    float32_t best_re = 0.0f, best_im = 0.0f;

    float32_t xr0;
    float32_t xq0;
    float32_t hr0;
    float32_t hq0;
    float32_t xr1;
    float32_t xq1;
    float32_t hr1;
    float32_t hq1;

    for (uint32_t k = 0; k < out_len; ++k) {
        float32_t acc_re = 0.0f, acc_im = 0.0f;

        uint32_t n = 0;
        for (; n + 1u < L; n += 2u) {
            // n
            xr0 = in_i[k + n];
            xq0 = in_q[k + n];
            hr0 = hi[n];
            hq0 = hq[n];
            acc_re += xr0 * hr0 - xq0 * hq0;
            acc_im += xr0 * hq0 + xq0 * hr0;
            // n+1
            xr1 = in_i[k + n + 1];
            xq1 = in_q[k + n + 1];
            hr1 = hi[n + 1];
            hq1 = hq[n + 1];
            acc_re += xr1 * hr1 - xq1 * hq1;
            acc_im += xr1 * hq1 + xq1 * hr1;
        }
        for (; n < L; ++n) {
            float32_t xr = in_i[k + n];
            float32_t xqv = in_q[k + n];
            float32_t hr = hi[n];
            float32_t hqv = hq[n];
            acc_re += xr * hr - xqv * hqv;
            acc_im += xr * hqv + xqv * hr;
        }

        float32_t p = acc_re * acc_re + acc_im * acc_im;

        if (out_corr)
            out_corr[k] = p;

        if (p > best) {
            best = p;
            best_k = k;
            best_re = acc_re;
            best_im = acc_im;
        }
    }

    if (peak_val)
        *peak_val = best;
    if (opt_peak_re)
        *opt_peak_re = best_re;
    if (opt_peak_im)
        *opt_peak_im = best_im;
    return best_k;
}

// uint32_t dsp_matched_filter(const struct dsp_context* ctx,
//                             const float32_t* restrict in_i,
//                             const float32_t* restrict in_q,
//                             uint32_t len_in,
//                             float32_t* restrict out_corr,
//                             float32_t* peak_val,
//                             float32_t* opt_peak_re,
//                             float32_t* opt_peak_im) {
//     const uint32_t L = ctx->mf_len;
//     if (L == 0u || len_in < L) {
//         if (peak_val)
//             *peak_val = 0.0f;
//         return 0u;
//     }
//
//     const uint32_t out_len = len_in - L + 1u;
//     const float32_t* restrict hi = ctx->mf_ref_i;
//     const float32_t* restrict hq = ctx->mf_ref_q;
//
//     float32_t best = -FLT_MAX;
//     uint32_t best_k = 0u;
//     float32_t best_re = 0.0f, best_im = 0.0f;
//
//     float32_t xr0;
//     float32_t xq0;
//     float32_t hr0;
//     float32_t hq0;
//     float32_t xr1;
//     float32_t xq1;
//     float32_t hr1;
//     float32_t hq1;
//
//     for (uint32_t k = 0; k < out_len; ++k) {
//         float32_t acc_re = 0.0f, acc_im = 0.0f;
//
//         uint32_t n = 0;
//         for (; n + 1u < L; n += 2u) {
//             // n
//             xr0 = in_i[k + n];
//             xq0 = in_q[k + n];
//             hr0 = hi[n];
//             hq0 = hq[n];
//             acc_re += xr0 * hr0 - xq0 * hq0;
//             acc_im += xr0 * hq0 + xq0 * hr0;
//             // n+1
//             xr1 = in_i[k + n + 1];
//             xq1 = in_q[k + n + 1];
//             hr1 = hi[n + 1];
//             hq1 = hq[n + 1];
//             acc_re += xr1 * hr1 - xq1 * hq1;
//             acc_im += xr1 * hq1 + xq1 * hr1;
//         }
//         for (; n < L; ++n) {
//             float32_t xr = in_i[k + n];
//             float32_t xqv = in_q[k + n];
//             float32_t hr = hi[n];
//             float32_t hqv = hq[n];
//             acc_re += xr * hr - xqv * hqv;
//             acc_im += xr * hqv + xqv * hr;
//         }
//
//         float32_t p = acc_re * acc_re + acc_im * acc_im;
//
//         if (out_corr)
//             out_corr[k] = p;
//
//         if (p > best) {
//             best = p;
//             best_k = k;
//             best_re = acc_re;
//             best_im = acc_im;
//         }
//     }
//
//     if (peak_val)
//         *peak_val = best;
//     if (opt_peak_re)
//         *opt_peak_re = best_re;
//     if (opt_peak_im)
//         *opt_peak_im = best_im;
//     return best_k;
// }
