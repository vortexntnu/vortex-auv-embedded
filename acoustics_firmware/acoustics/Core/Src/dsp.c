#include "dsp.h"
#include "dsp/support_functions.h"

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
              uint32_t decim,               // M
              const float32_t* fir_coeffs,  // FIR coeffs for decimator
              uint32_t fir_num_taps,        // number of taps
              uint32_t block_size_in,       // input block size used per call
              const float32_t* mf_ref_i,
              const float32_t* mf_ref_q,
              uint32_t mf_len) {
    memset(ctx, 0, sizeof(*ctx));

    // Basic config
    ctx->fs_in = fs_in;
    ctx->f0 = f0;
    ctx->fc_lp = fc_lp;
    ctx->decim = decim;
    ctx->mf_ref_i = mf_ref_i;
    ctx->mf_ref_q = mf_ref_q;
    ctx->mf_len = mf_len;

    // NCO (start at phase 0)
    ctx->dphase = 2.0f * PI * (f0 / fs_in);
    arm_sin_cos_f32(ctx->dphase, &ctx->sin_d, &ctx->cos_d);
    ctx->cos_p = 1.0f;
    ctx->sin_p = 0.0f;

    // IIR LPF init (3 biquads)
    ctx->num_biquads = DSP_MAX_BIQUADS;
    memcpy(ctx->biquad_coeffs, BUTTER6_COEFFS_SOS, sizeof(BUTTER6_COEFFS_SOS));
    arm_biquad_cascade_df2T_init_f32(&ctx->iir_i, ctx->num_biquads,
                                     ctx->biquad_coeffs, ctx->iir_state_i);
    arm_biquad_cascade_df2T_init_f32(&ctx->iir_q, ctx->num_biquads,
                                     ctx->biquad_coeffs, ctx->iir_state_q);

    ctx->fir_coeffs = fir_coeffs;
    ctx->fir_num_taps = fir_num_taps;
    ctx->block_size_in = block_size_in;

    arm_fir_decimate_init_f32(&ctx->fir_i, (uint16_t)ctx->fir_num_taps,
                              (uint8_t)ctx->decim, ctx->fir_coeffs,
                              ctx->fir_state_i, ctx->block_size_in);

    arm_fir_decimate_init_f32(&ctx->fir_q, (uint16_t)ctx->fir_num_taps,
                              (uint8_t)ctx->decim, ctx->fir_coeffs,
                              ctx->fir_state_q, ctx->block_size_in);
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

    for (uint32_t k = 0; k < n; k++) {
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
                             const float32_t* restrict io_i,
                             const float32_t* restrict io_q,
                             float32_t* restrict out_i,
                             float32_t* restrict out_q,
                             uint32_t n) {
    arm_biquad_cascade_df2T_f32(&ctx->iir_i, io_i, out_i, n);
    arm_biquad_cascade_df2T_f32(&ctx->iir_q, io_q, out_q, n);
}

void dsp_decimate(struct dsp_context* ctx,
                  const float32_t* in_i,
                  const float32_t* in_q,
                  float32_t* out_i,
                  float32_t* out_q,
                  uint32_t n) {
    arm_fir_decimate_f32(&ctx->fir_i, in_i, out_i, n);
    arm_fir_decimate_f32(&ctx->fir_q, in_q, out_q, n);
}

uint32_t dsp_matched_filter_sep(const struct dsp_context* ctx,
                                const float32_t* restrict in_i,
                                const float32_t* restrict in_q,
                                uint32_t len_in,
                                float32_t* restrict out_corr,
                                float32_t* peak_val) {
    const uint32_t L = ctx->mf_len;
    if (L == 0u || len_in < L) {
        if (peak_val)
            *peak_val = 0.0f;
        return 0u;
    }

    const uint32_t out_len = len_in - L + 1u;
    const float32_t* restrict hi = ctx->mf_ref_i;
    const float32_t* restrict hq = ctx->mf_ref_q;

    float32_t best = -FLT_MAX;
    uint32_t best_k = 0u;

    for (uint32_t k = 0; k < out_len; ++k) {
        const float32_t* __restrict xi = &in_i[k];
        const float32_t* __restrict xq = &in_q[k];

        float32_t acc_re = 0.0f;
        float32_t acc_im = 0.0f;

        uint32_t n = 0;
        for (; n + 1u < L; n += 2u) {
            // n
            float32_t xr0 = xi[0], xq0 = xq[0];
            float32_t hr0 = hi[0], hq0 = hq[0];
            acc_re += xr0 * hr0 - xq0 * hq0;
            acc_im += xr0 * hq0 + xq0 * hr0;

            // n+1
            float32_t xr1 = xi[1], xq1 = xq[1];
            float32_t hr1 = hi[1], hq1 = hq[1];
            acc_re += xr1 * hr1 - xq1 * hq1;
            acc_im += xr1 * hq1 + xq1 * hr1;

            xi += 2;
            xq += 2;
            hi += 2;
            hq += 2;
        }
        for (; n < L; ++n) {
            float32_t xr = *xi++, xqv = *xq++;
            float32_t hr = *hi++, hqv = *hq++;
            acc_re += xr * hr - xqv * hqv;
            acc_im += xr * hqv + xqv * hr;
        }

        float32_t p = acc_re * acc_re + acc_im * acc_im;
        out_corr[k] = p;

        if (p > best) {
            best = p;
            best_k = k;
        }

        hi -= L;
        hq -= L;
    }

    if (peak_val)
        *peak_val = best;
    return best_k;
}
