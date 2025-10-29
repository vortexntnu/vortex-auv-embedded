#include "dsp.h"
#include <stdint.h>
#include "arm_math_memory.h"
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
                             const q15_t* restrict raw_samples,
                             q15_t* restrict out_i,
                             q15_t* restrict out_q,
                             uint32_t size) {
    q15_t c = ctx->cos_p;
    q15_t s = ctx->sin_p;
    const q15_t cd = ctx->cos_d;
    const q15_t sd = ctx->sin_d;
    const q31_t cdsd = __PKHBT((uint16_t)cd, (uint16_t)sd, 16);
    q31_t cs, sc;
    q31_t c_q31, s_q31;
    q15_t* restrict oi = out_i;
    q15_t* restrict oq = out_q;

    for (uint32_t k = 0; k < size; k++) {
        const q15_t x = *raw_samples++;

        *oi++ = mul_q15(x, c);
        *oq++ = mul_q15(x, (q15_t)(-s));

        cs = __PKHBT((uint16_t)c, (uint16_t)s, 16);
        sc = __PKHBT((uint16_t)s, (uint16_t)c, 16);

        c_q31 = __SMUSD(cs, cdsd);
        s_q31 = __SMLAD(sc, cdsd, 0);

        c = (q15_t)__SSAT(((c_q31 << 1) + (1 << 15)) >> 16, 16);
        s = (q15_t)__SSAT(((s_q31 << 1) + (1 << 15)) >> 16, 16);
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

void dsp_fir_decimate_q15(struct dsp_context* ctx,
                          const q15_t* restrict in_i,
                          const q15_t* restrict in_q,
                          q15_t* restrict out_i,
                          q15_t* restrict out_q,
                          uint32_t size) {
    arm_fir_decimate_q15(&ctx->fir_i, in_i, out_i, size);
    arm_fir_decimate_q15(&ctx->fir_q, in_q, out_q, size);
}

// Round + shift Q30 → Q15 with saturation (symmetric rounding)
// Works safely for large 64-bit accumulators.
static inline q15_t q30_to_q15_sat64(int64_t x_q30) {
    // symmetric round-to-nearest: +bias for ≥0, -bias for <0
    x_q30 += (x_q30 >= 0) ? (1LL << 14) : -(1LL << 14);
    int64_t r = x_q30 >> 15;  // arithmetic shift

    // saturate to Q15 without narrowing overflow
    if (r > 32767)
        return (q15_t)32767;
    if (r < -32768)
        return (q15_t)-32768;
    return (q15_t)r;
}

void dsp_matched_filter_q15(
    const q15_t* restrict xi,  // input I window, length N
    const q15_t* restrict xq,  // input Q window, length N
    const q15_t* restrict hi,  // replica I (time-rev + conj), length N
    const q15_t* restrict hq,  // replica Q (time-rev + conj), length N
    uint32_t size,
    q15_t* restrict outI,
    q15_t* restrict outQ) {
    // We’ll accumulate the four partial sums in 64-bit, then combine:
    // re = sum(xi*hi)  - sum(xq*hq)
    // im = sum(xi*hq)  + sum(xq*hi)
    const q15_t* restrict xi_p = xi;
    const q15_t* restrict xq_p = xq;
    const q15_t* restrict hi_p = hi;
    const q15_t* restrict hq_p = hq;
    q63_t acc_xi_hi = 0;
    q63_t acc_xq_hq = 0;
    q63_t acc_xi_hq = 0;
    q63_t acc_xq_hi = 0;
    q31_t xi2;
    q31_t xq2;
    q31_t hi2;
    q31_t hq2;

    uint32_t n2 = size >> 1;
    for (uint32_t i = 0; i < n2; ++i) {
        xi2 = read_q15x2_ia(&xi_p);
        xq2 = read_q15x2_ia(&xq_p);
        hi2 = read_q15x2_ia(&hi_p);
        hq2 = read_q15x2_ia(&hq_p);

        // Dual 16x16 → 64-bit accumulate (Q15*Q15 → Q30 per lane, summed)
        acc_xi_hi = __SMLALD(xi2, hi2, acc_xi_hi);  // sum(xi*hi)
        acc_xq_hq = __SMLALD(xq2, hq2, acc_xq_hq);  // sum(xq*hq)
        acc_xi_hq = __SMLALD(xi2, hq2, acc_xi_hq);  // sum(xi*hq)
        acc_xq_hi = __SMLALD(xq2, hi2, acc_xq_hi);  // sum(xq*hi)
    }

    if (size & 1) {
        uint32_t k = size - 1;
        acc_xi_hi += (q31_t)xi[k] * (q31_t)hi[k];
        acc_xq_hq += (q31_t)xq[k] * (q31_t)hq[k];
        acc_xi_hq += (q31_t)xi[k] * (q31_t)hq[k];
        acc_xq_hi += (q31_t)xq[k] * (q31_t)hi[k];
    }

    q63_t re_q30_64 = acc_xi_hi - acc_xq_hq;
    q63_t im_q30_64 = acc_xi_hq + acc_xq_hi;

    *outI = q30_to_q15_sat64(re_q30_64);
    *outQ = q30_to_q15_sat64(im_q30_64);
}
