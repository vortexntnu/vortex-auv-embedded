#include "dsp.h"
#include <stdint.h>
#include "arm_math_types.h"
#include "cmsis_gcc.h"
#include "dsp/filtering_functions.h"

// ======= 6th-order Butterworth LPF @ fs=192k, fc=450 Hz =======
// SOS coeffs for CMSIS DF2T: {b0,b1,b2,a1,a2} per biquad.
// If you change fs/fc, recompute these!
static const q15_t BUTTER6_COEFFS_SOS[5 * DSP_MAX_BIQUADS] = {0};

static const q15_t FIR_Q15_TAPS[97] = {0};

// float -> q15 with rounding & saturation
static inline q15_t f32_to_q15(float x) {
    if (x >= 0.999969f)
        return 32767;
    if (x <= -1.0f)
        return -32768;
    int32_t v = (int32_t)(x * 32768.0f + (x >= 0 ? 0.5f : -0.5f));
    return (q15_t)v;
}

void dsp_init(struct dsp_context* ctx,
              const q15_t* mf_ref_i,
              const q15_t* mf_ref_q,
              uint32_t mf_len) {
    memset(ctx, 0, sizeof(*ctx));

    ctx->mf_ref_i = mf_ref_i;
    ctx->mf_ref_q = mf_ref_q;
    ctx->mf_len = mf_len;

    ctx->dphase =
        2.0f * PI * ((float)PINGER_FREQUENCY / (float)SAMPLING_FREQUENCY);

    float s, c;
    arm_sin_cos_f32(ctx->dphase, &s, &c);
    ctx->sin_d = f32_to_q15(s);
    ctx->cos_d = f32_to_q15(c);

    ctx->cos_p = (q15_t)32767;
    ctx->sin_p = (q15_t)0;

    const int8_t IIR_POSTSHIFT = 1;
    arm_biquad_cascade_df1_init_q15(&ctx->iir_i, DSP_MAX_BIQUADS,
                                    BUTTER6_COEFFS_SOS, ctx->iir_state_i,
                                    IIR_POSTSHIFT);
    arm_biquad_cascade_df1_init_q15(&ctx->iir_q, DSP_MAX_BIQUADS,
                                    BUTTER6_COEFFS_SOS, ctx->iir_state_q,
                                    IIR_POSTSHIFT);

    arm_fir_decimate_init_q15(&ctx->fir_i, (uint16_t)NUM_TAPS,
                              (uint8_t)DECIMATE_FACTOR, FIR_Q15_TAPS,
                              ctx->fir_state_i, BLOCK_SIZE_IN);

    arm_fir_decimate_init_q15(&ctx->fir_q, (uint16_t)NUM_TAPS,
                              (uint8_t)DECIMATE_FACTOR, FIR_Q15_TAPS,
                              ctx->fir_state_q, BLOCK_SIZE_IN);
}

static inline q15_t mul_q15(q15_t a, q15_t b) {
    int32_t t = (int32_t)a * (int32_t)b;
    t = (t << 1);
    t = t + (1 << 15);
    return (q15_t)__SSAT(t >> 16, 16);
}

void dsp_mix_to_baseband_q15(struct dsp_context* ctx,
                             const q15_t* raw_samples,
                             q15_t* out_i,
                             q15_t* out_q,
                             uint32_t size) {
    q15_t c = ctx->cos_p;
    q15_t s = ctx->sin_p;
    const q15_t cd = ctx->cos_d;
    const q15_t sd = ctx->sin_d;

    for (uint32_t k = 0; k < size; k++) {
        q15_t x = *raw_samples++;

        *out_i++ = mul_q15(x, c);
        *out_q++ = mul_q15(x, (q15_t)(-s));

        int32_t cs = __PKHBT((uint16_t)c, (uint16_t)s, 16);
        int32_t sc = __PKHBT((uint16_t)s, (uint16_t)c, 16);
        int32_t cdsd = __PKHBT((uint16_t)cd, (uint16_t)sd, 16);

        int32_t c_q30 = __SMUSD(cs, cdsd);
        int32_t s_q30 = __SMLAD(sc, cdsd, 0);

        c = (q15_t)__SSAT(((c_q30 << 1) + (1 << 15)) >> 16, 16);
        s = (q15_t)__SSAT(((s_q30 << 1) + (1 << 15)) >> 16, 16);
    }

    ctx->cos_p = c;
    ctx->sin_p = s;
}

void dsp_lpf_6th_butterworth_q15(struct dsp_context* ctx,
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

// Round+shift Q30 -> Q15 with saturation
static inline q15_t q30_to_q15_sat(int32_t x_q30) {
    int32_t r = (x_q30 + (1 << 14)) >> 15;  // round-to-nearest
    return (q15_t)__SSAT(r, 16);
}

/**
 * Complex matched filter (Q15).
 * Computes: y = sum_{k=0..N-1}  (xi[k] + j xq[k]) * (hi[k] + j hq[k])
 * Assume (hi,hq) is already time-reversed AND conjugated replica.
 * Returns I/Q in Q15. Uses 64-bit accumulators for safety.
 */
void matched_filter_q15(
    const q15_t* restrict xi,  // input I window, length N
    const q15_t* restrict xq,  // input Q window, length N
    const q15_t* restrict hi,  // replica I (time-rev + conj), length N
    const q15_t* restrict hq,  // replica Q (time-rev + conj), length N
    uint32_t N,
    q15_t* outI,
    q15_t* outQ) {
    // We’ll accumulate the four partial sums in 64-bit, then combine:
    // re = sum(xi*hi)  - sum(xq*hq)
    // im = sum(xi*hq)  + sum(xq*hi)
    int64_t acc_xi_hi = 0;
    int64_t acc_xq_hq = 0;
    int64_t acc_xi_hq = 0;
    int64_t acc_xq_hi = 0;
    int32_t xi2;
    int32_t xq2;
    int32_t hi2;
    int32_t hq2;

    uint32_t n2 = N >> 1;
    for (uint32_t i = 0; i < n2; ++i) {
        xi2 = __PKHBT((uint16_t)xi[2 * i], (uint16_t)xi[2 * i + 1], 16);
        xq2 = __PKHBT((uint16_t)xq[2 * i], (uint16_t)xq[2 * i + 1], 16);
        hi2 = __PKHBT((uint16_t)hi[2 * i], (uint16_t)hi[2 * i + 1], 16);
        hq2 = __PKHBT((uint16_t)hq[2 * i], (uint16_t)hq[2 * i + 1], 16);

        // Dual 16x16 → 64-bit accumulate (Q15*Q15 → Q30 per lane, summed)
        acc_xi_hi = __SMLALD(xi2, hi2, acc_xi_hi);  // sum(xi*hi)
        acc_xq_hq = __SMLALD(xq2, hq2, acc_xq_hq);  // sum(xq*hq)
        acc_xi_hq = __SMLALD(xi2, hq2, acc_xi_hq);  // sum(xi*hq)
        acc_xq_hi = __SMLALD(xq2, hi2, acc_xq_hi);  // sum(xq*hi)
    }

    if (N & 1) {
        uint32_t k = N - 1;
        acc_xi_hi += (int32_t)xi[k] * (int32_t)hi[k];
        acc_xq_hq += (int32_t)xq[k] * (int32_t)hq[k];
        acc_xi_hq += (int32_t)xi[k] * (int32_t)hq[k];
        acc_xq_hi += (int32_t)xq[k] * (int32_t)hi[k];
    }

    int64_t re_q30_64 = acc_xi_hi - acc_xq_hq;
    int64_t im_q30_64 = acc_xi_hq + acc_xq_hi;

    // Convert Q30 → Q15 (round) with saturation
    // We downshift in 64-bit first to avoid intermediate overflow.
    int32_t re_q30_32 = (int32_t)re_q30_64;  // safe after >> if you prefer
    int32_t im_q30_32 = (int32_t)im_q30_64;

    *outI = q30_to_q15_sat(re_q30_32);
    *outQ = q30_to_q15_sat(im_q30_32);
}
