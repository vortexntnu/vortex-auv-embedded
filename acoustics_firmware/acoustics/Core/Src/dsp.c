#include "dsp.h"
#include <math.h>
#include <stdint.h>
#include <string.h>
#include "arm_math_types.h"
#include "dsp/filtering_functions.h"

// ======= 6th-order Butterworth LPF @ fs=192k, fc=450 Hz =======
// SOS coeffs for CMSIS DF2T: {b0,b1,b2,a1,a2} per biquad.
// If you change fs/fc, recompute these!
static const float32_t BUTTER6_COEFFS_SOS[5 * DSP_MAX_BIQUADS] = {
    // Biquad 1
    4.27271978e-07f, 8.54543957e-07f, 4.27271978e-07f, -1.97948956f,
    0.98000293f,
    // Biquad 2
    1.00000000f, 2.00000000f, 1.00000000f, -1.98956364f, 0.98961461f,
    // Biquad 3
    1.00000000f, 2.00000000f, 1.00000000f, -1.99521656f, 0.99524618f};

// ======= Init =======
void dsp_init(struct dsp_context* ctx,
              float32_t fs_in,
              float32_t f0,
              float32_t fc_lp,
              uint32_t decim,
              const float32_t* mf_ref,
              uint32_t mf_len) {
    memset(ctx, 0, sizeof(*ctx));
    ctx->fs_in = fs_in;
    ctx->f0 = f0;
    ctx->fc_lp = fc_lp;
    ctx->decim = decim ? decim : DSP_DECIM_FACTOR;

    // NCO init (start at phase 0)
    ctx->dphase = 2.0f * PI * (f0 / fs_in);
    arm_sin_cos_f32(ctx->dphase, &ctx->sin_d, &ctx->cos_d);
    ctx->cos_p = 1.0f;
    ctx->sin_p = 0.0f;

    // LPF init (3 biquads)
    ctx->num_biquads = DSP_MAX_BIQUADS;
    memcpy(ctx->biquad_coeffs, BUTTER6_COEFFS_SOS, sizeof(BUTTER6_COEFFS_SOS));
    arm_biquad_cascade_df2T_init_f32(&ctx->iir_i, ctx->num_biquads,
                                     ctx->biquad_coeffs, ctx->iir_state_i);
    arm_biquad_cascade_df2T_init_f32(&ctx->iir_q, ctx->num_biquads,
                                     ctx->biquad_coeffs, ctx->iir_state_q);

    // Matched filter replica
    ctx->mf_ref = mf_ref;
    ctx->mf_len = mf_len;
}

// ======= Mix int16 (Q15) to complex baseband float32 =======
void dsp_mix_to_baseband_i16(struct dsp_context* ctx,
                             const int16_t* raw_samples,
                             float32_t* out_i,
                             float32_t* out_q,
                             uint32_t n) {
    const float32_t S = 1.0f / 32768.0f;

    float32_t cos_p = ctx->cos_p;
    float32_t sin_p = ctx->sin_p;
    const float32_t cos_d = ctx->cos_d;
    const float32_t sin_d = ctx->sin_d;

    for (uint32_t k = 0; k < n; ++k) {
        float32_t x = (float32_t)raw_samples[k] * S;

        // Complex mix by e^{-j 2π f0 t}: I = x*cos, Q = x*(-sin)
        out_i[k] = x * cos_p;
        out_q[k] = x * -sin_p;

        float32_t c = cos_p * cos_d - sin_p * sin_d;
        float32_t s = sin_p * cos_d + cos_p * sin_d;
        cos_p = c;
        sin_p = s;
    }

    ctx->cos_p = cos_p;
    ctx->sin_p = sin_p;
}

void dsp_lpf_6th_butterworth(struct dsp_context* ctx,
                             const float32_t* io_i,
                             const float32_t* io_q,
                             float32_t* out_i,
                             float32_t* out_q,
                             uint32_t n) {
    arm_biquad_cascade_df2T_f32(&ctx->iir_i, io_i, out_i, n);
    arm_biquad_cascade_df2T_f32(&ctx->iir_q, io_q, out_q, n);
}

uint32_t dsp_decimate_pickM(const float32_t* in_i,
                            const float32_t* in_q,
                            float32_t* out_i,
                            float32_t* out_q,
                            uint32_t n,
                            uint32_t M) {
    if (M == 0)
        return 0u;
    uint32_t outN = n / M;
    for (uint32_t m = 0, k = 0; m < outN; ++m, k += M) {
        out_i[m] = in_i[k];
        out_q[m] = in_q[k];
    }
    return outN;
}

void dsp_decimate(struct dsp_context* ctx,
                  const float32_t* in_i,
                  const float32_t* in_q,
                  float32_t* out_i,
                  float32_t* out_q, uint32_t n) {
    arm_fir_decimate_f32(&ctx->fir_i, in_i, out_i, n);
    arm_fir_decimate_f32(&ctx->fir_q, in_q, out_q, n);
}

uint32_t dsp_matched_filter(const struct dsp_context* ctx,
                            const float32_t* in_iq,
                            uint32_t len_in,
                            float32_t* out_corr,
                            float32_t* peak_val) {
    const uint32_t L = ctx->mf_len;
    if (L == 0 || len_in < L) {
        if (peak_val)
            *peak_val = 0.0f;
        return 0u;
    }
    const uint32_t out_len = len_in - L + 1;

    float32_t peak = -1.0f;
    uint32_t peak_idx = 0;

    for (uint32_t k = 0; k < out_len; ++k) {
        const float32_t* xk = &in_iq[2u * k];
        float32_t re, im;
        arm_cmplx_dot_prod_f32(xk, ctx->mf_ref, L, &re, &im);
        float32_t p = re * re + im * im;
        out_corr[k] = p;
        if (p > peak) {
            peak = p;
            peak_idx = k;
        }
    }

    if (peak_val)
        *peak_val = peak;
    return peak_idx;
}
