#include "cwt.h"
#include "main.h"
#include "arm_math.h"
#include <math.h>  // for expf, only used at init

// CWT-specific buffers
TCM q15_t cwt_kernel[FFT_SIZE * 2];     // complex Morlet kernel (freq domain)
TCM q15_t cwt_product[FFT_SIZE * 2];    // complex product buffer
TCM q15_t cwt_result[FFT_SIZE * 2];     // IFFT output (complex)

// -----------------------------------------------------------
// Build Morlet kernel at startup (float -> q15 conversion)
// target_freq: the frequency you want to focus on (e.g. 30000.0f)
// fs:          your ADC sample rate (e.g. 200000.0f for AD7606)
// -----------------------------------------------------------
void cwt_build_morlet_kernel_q15(float32_t target_freq, float32_t fs)
{
    float32_t f0 = 5.0f;  // Morlet center frequency (cycles, typical: 5-6)
    // scale = f0 / target_freq (in normalized units)
    float32_t scale = f0 / (target_freq / fs * FFT_SIZE);

    for (uint32_t k = 0; k < FFT_SIZE / 2; k++)
    {
        float32_t freq_norm = (float32_t)k;  // bin index
        float32_t arg = (scale * freq_norm - f0);
        float32_t val = expf(-0.5f * arg * arg);

        // val is real-only kernel; clamp to q15 range [-1, 1)
        // Pack as interleaved [real, imag] — imaginary = 0
        val = fmaxf(-1.0f, fminf(0.99997f, val));

        cwt_kernel[2 * k]     = (q15_t)(val * 32767.0f);  // real
        cwt_kernel[2 * k + 1] = 0;                         // imaginary
    }

    // Zero negative frequencies (one-sided Morlet)
    for (uint32_t k = FFT_SIZE / 2; k < FFT_SIZE; k++)
    {
        cwt_kernel[2 * k]     = 0;
        cwt_kernel[2 * k + 1] = 0;
    }
}

// input:    your raw q15_t ADC buffer [FFT_SIZE]
// out_mag:  output magnitude buffer [FFT_SIZE] — time-domain envelope
void cwt_morlet_q15(const q15_t *input, q15_t *out_mag)
{
    // 1. Copy input (arm_rfft_q15 modifies in-place)
    arm_copy_q15(input, fft_input, FFT_SIZE);

    // 2. Forward FFT  →  fft_output is complex interleaved, length FFT_SIZE*2
    arm_rfft_q15(&fft_instance, fft_input, fft_output);

    // 3. Complex multiply: fft_output * cwt_kernel → cwt_product
    //    arm_cmplx_mult_cmplx_q15 saturates and right-shifts by 1 internally
    arm_cmplx_mult_cmplx_q15(fft_output, cwt_kernel, cwt_product, FFT_SIZE);

    // 4. Inverse FFT  →  cwt_result is complex interleaved
    //    Pass ifftFlag=1, bitReverseFlag=1
    arm_rfft_q15(&fft_instance, cwt_product, cwt_result);
    //    NOTE: CMSIS arm_rfft_q15 does not support in-place IFFT natively;
    //    if your version lacks IFFT, use arm_cfft_q15 instead (see note below)

    // 5. Complex magnitude of result → envelope of CWT at target frequency
    arm_cmplx_mag_q15(cwt_result, out_mag, FFT_SIZE);
    //    out_mag[n] = sqrt(re^2 + im^2) — this is your time-domain energy envelope
}
