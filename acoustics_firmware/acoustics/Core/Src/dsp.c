#include "dsp.h"
#include "arm_math_types.h"

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

static const float32_t FIR_DECIM48_TAPS_97[97] = {
    1.6930365205e-04f, 2.5375194940e-04f, 3.5614954360e-04f, 4.7826088013e-04f, 
    6.2183104041e-04f, 7.8856306358e-04f, 9.8009429196e-04f, 1.1979720207e-03f, 
    1.4436287558e-03f, 1.7183574038e-03f, 2.0232867327e-03f, 2.3593574538e-03f, 
    2.7272992800e-03f, 3.1276093156e-03f, 3.5605321306e-03f, 4.0260418568e-03f, 
    4.5238266330e-03f, 5.0532757002e-03f, 5.6134694230e-03f, 6.2031724827e-03f, 
    6.8208304491e-03f, 7.4645698997e-03f, 8.1322022096e-03f, 8.8212310896e-03f, 
    9.5288638995e-03f, 1.0252026714e-02f, 1.0987383067e-02f, 1.1731356243e-02f, 
    1.2480154943e-02f, 1.3229802100e-02f, 1.3976166549e-02f, 1.4714997269e-02f, 
    1.5441959790e-02f, 1.6152674416e-02f, 1.6842755798e-02f, 1.7507853427e-02f, 
    1.8143692572e-02f, 1.8746115163e-02f, 1.9311120145e-02f, 1.9834902811e-02f, 
    2.0313892609e-02f, 2.0744788981e-02f, 2.1124594776e-02f, 2.1450646802e-02f, 
    2.1720643155e-02f, 2.1932666961e-02f, 2.2085206228e-02f, 2.2177169559e-02f, 
    2.2207897528e-02f, 2.2177169559e-02f, 2.2085206228e-02f, 2.1932666961e-02f, 
    2.1720643155e-02f, 2.1450646802e-02f, 2.1124594776e-02f, 2.0744788981e-02f, 
    2.0313892609e-02f, 1.9834902811e-02f, 1.9311120145e-02f, 1.8746115163e-02f, 
    1.8143692572e-02f, 1.7507853427e-02f, 1.6842755798e-02f, 1.6152674416e-02f, 
    1.5441959790e-02f, 1.4714997269e-02f, 1.3976166549e-02f, 1.3229802100e-02f, 
    1.2480154943e-02f, 1.1731356243e-02f, 1.0987383067e-02f, 1.0252026714e-02f, 
    9.5288638995e-03f, 8.8212310896e-03f, 8.1322022096e-03f, 7.4645698997e-03f, 
    6.8208304491e-03f, 6.2031724827e-03f, 5.6134694230e-03f, 5.0532757002e-03f, 
    4.5238266330e-03f, 4.0260418568e-03f, 3.5605321306e-03f, 3.1276093156e-03f, 
    2.7272992800e-03f, 2.3593574538e-03f, 2.0232867327e-03f, 1.7183574038e-03f, 
    1.4436287558e-03f, 1.1979720207e-03f, 9.8009429196e-04f, 7.8856306358e-04f, 
    6.2183104041e-04f, 4.7826088013e-04f, 3.5614954360e-04f, 2.5375194940e-04f, 
    1.6930365205e-04f
};


void dsp_init(struct dsp_context* ctx,
              const float32_t* mf_ref_i,
              const float32_t* mf_ref_q,
              uint32_t mf_len) {
    memset(ctx, 0, sizeof(*ctx));

    ctx->mf_ref_i = mf_ref_i;
    ctx->mf_ref_q = mf_ref_q;
    ctx->mf_len = mf_len;

    ctx->dphase = 2.0f * PI * ((float) PINGER_FREQUENCY / SAMPLING_FREQUENCY);
    arm_sin_cos_f32(ctx->dphase, &ctx->sin_d, &ctx->cos_d);
    ctx->cos_p = 1.0f;
    ctx->sin_p = 0.0f;

    arm_biquad_cascade_df2T_init_f32(&ctx->iir_i, DSP_MAX_BIQUADS,
                                     BUTTER6_COEFFS_SOS, ctx->iir_state_i);
    arm_biquad_cascade_df2T_init_f32(&ctx->iir_q, DSP_MAX_BIQUADS,
                                     BUTTER6_COEFFS_SOS, ctx->iir_state_q);

    arm_fir_decimate_init_f32(&ctx->fir_i, (uint16_t)NUM_TAPS,
                              (uint8_t)DECIMATE_FACTOR, FIR_DECIM48_TAPS_97,
                              ctx->fir_state_i, BLOCK_SIZE_IN);

    arm_fir_decimate_init_f32(&ctx->fir_q, (uint16_t)NUM_TAPS,
                              (uint8_t)DECIMATE_FACTOR, FIR_DECIM48_TAPS_97,
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
                             const float32_t* restrict io_i,
                             const float32_t* restrict io_q,
                             float32_t* restrict out_i,
                             float32_t* restrict out_q,
                             uint32_t size) {
    arm_biquad_cascade_df2T_f32(&ctx->iir_i, io_i, out_i, size);
    arm_biquad_cascade_df2T_f32(&ctx->iir_q, io_q, out_q, size);
}

void dsp_decimate(struct dsp_context* ctx,
                  const float32_t* restrict in_i,
                  const float32_t* restrict in_q,
                  float32_t* out_i,
                  float32_t* out_q,
                  uint32_t size) {
    arm_fir_decimate_f32(&ctx->fir_i, in_i, out_i, size);
    arm_fir_decimate_f32(&ctx->fir_q, in_q, out_q, size);
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
