import numpy as np
from scipy.signal import butter, lfilter, firwin, decimate, sosfilt
import matplotlib as plt

# ---------------------------
# Q15 conversion helpers
# ---------------------------
def f32_to_q15(x):
    """Convert float in [-1,1) to Q15 integer"""
    x = np.clip(x, -1.0, 0.999969)
    return np.int16(np.round(x * 32768))

def q15_to_f32(x):
    """Convert Q15 integer to float"""
    return x / 32768.0

def mul_q15(a, b):
    """Simulate Q15 multiply with saturation"""
    t = np.int32(a) * np.int32(b)  # Q15*Q15 -> Q30
    t = (t << 1)  # Q31
    t = t + (1 << 15)  # rounding
    r = t >> 16
    return np.int16(np.clip(r, -32768, 32767))

# ---------------------------
# Baseband mixing (complex downconversion)
# ---------------------------
def mix_to_baseband_q15(x, cos_d, sin_d, cos_p=32767, sin_p=0):
    """Mix Q15 input signal to baseband"""
    n = len(x)
    out_i = np.zeros(n, dtype=np.int16)
    out_q = np.zeros(n, dtype=np.int16)

    c = cos_p
    s = sin_p

    for k in range(n):
        xi = mul_q15(x[k], c)
        xq = mul_q15(x[k], -s)
        out_i[k] = xi
        out_q[k] = xq

        # Rotate by small phase increment
        c_next = np.int16(np.clip(((c * cos_d - s * sin_d) << 1) >> 16, -32768, 32767))
        s_next = np.int16(np.clip(((s * cos_d + c * sin_d) << 1) >> 16, -32768, 32767))
        c, s = c_next, s_next

    return out_i, out_q, c, s

# ---------------------------
# 6th-order Butterworth LPF (IIR biquads)
# ---------------------------
def lpf_butter6_q15(x):
    """6th-order Butterworth LPF"""
    sos = butter(6, 450, fs=192000, output='sos')
    return sosfilt(sos, x)

# ---------------------------
# FIR decimation
# ---------------------------
def fir_decimate_q15(x, num_taps=97, decim=2):
    taps = firwin(num_taps, cutoff=0.01)  # normalized cutoff
    y = lfilter(taps, [1.0], x)
    return y[::decim]

# ---------------------------
# Matched filter (complex correlation)
# ---------------------------
def matched_filter_q15(xi, xq, hi, hq):
    """Simulate STM32 Q15 matched filter"""
    xi_f = q15_to_f32(xi)
    xq_f = q15_to_f32(xq)
    hi_f = q15_to_f32(hi)
    hq_f = q15_to_f32(hq)

    re = np.sum(xi_f*hi_f - xq_f*hq_f)
    im = np.sum(xi_f*hq_f + xq_f*hi_f)
    return f32_to_q15(re), f32_to_q15(im)

# ---------------------------
# Deinterleave 3x2 -> 5 channels
# ---------------------------
def deinterleave_3x2_to_5(srcA, srcB, srcC):
    """Reproduce STM32 deinterleave logic"""
    outA0, outA1, outB0, outB1, outC0 = [], [], [], [], []

    n_words = len(srcA)
    for i in range(n_words):
        wa = srcA[i]
        wb = srcB[i]
        wc = srcC[i]

        outA0.append((wa >> 16) & 0xFFFF)
        outA1.append(wa & 0xFFFF)
        outB0.append((wb >> 16) & 0xFFFF)
        outB1.append(wb & 0xFFFF)
        outC0.append((wc >> 16) & 0xFFFF)

    return np.array(outA0), np.array(outA1), np.array(outB0), np.array(outB1), np.array(outC0)

# ---------------------------
# Example pipeline usage
# ---------------------------
if __name__ == "__main__":
    # Simulated raw Q15 data
    n = 1024
    raw = f32_to_q15(np.sin(2*np.pi*2500*np.arange(n)/192000))

    # Mix to baseband
    dphase = 2*np.pi*2500/192000
    cos_d = f32_to_q15(np.cos(dphase))
    sin_d = f32_to_q15(np.sin(dphase))
    i_base, q_base, _, _ = mix_to_baseband_q15(raw, cos_d, sin_d)

    # Lowpass filter
    i_lpf = lpf_butter6_q15(q15_to_f32(i_base))
    q_lpf = lpf_butter6_q15(q15_to_f32(q_base))

    # Decimate
    i_dec = fir_decimate_q15(f32_to_q15(i_lpf))
    q_dec = fir_decimate_q15(f32_to_q15(q_lpf))

    # Matched filter (using same signal as replica)
    outI, outQ = matched_filter_q15(i_dec, q_dec, i_dec, q_dec)
    print("Matched filter output:", outI, outQ)
    
    print(raw)