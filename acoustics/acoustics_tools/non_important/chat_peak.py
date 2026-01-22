import numpy as np

# ------------------------------------------------------------
# Utility functions
# ------------------------------------------------------------

def rms(x):
    """
    Root Mean Square (block-level)
    """
    return np.sqrt(np.mean(x**2))


def mad(x):
    """
    Median Absolute Deviation (block-level)
    Robust estimator of noise scale.
    """
    med = np.median(x)
    return np.median(np.abs(x - med))


def db(x):
    """
    Convert linear ratio to dB (for debugging / plotting).
    """
    return 20 * np.log10(np.maximum(x, 1e-12))


# ------------------------------------------------------------
# Matched filter (FFT-based)
# ------------------------------------------------------------

def fft_matched_filter(block, ref_fft):
    """
    Perform FFT-based matched filtering on one block.

    Parameters
    ----------
    block : np.ndarray
        Time-domain input signal (length N)
    ref_fft : np.ndarray
        FFT of reference signal (length N)

    Returns
    -------
    mf_out : np.ndarray
        Time-domain matched filter output
    """
    # FFT of input block
    X = np.fft.fft(block)

    # Frequency-domain multiply with conjugated reference
    Y = X * np.conj(ref_fft)

    # Back to time domain
    mf_out = np.fft.ifft(Y).real

    return mf_out


# ------------------------------------------------------------
# Detection logic
# ------------------------------------------------------------

def detect_ping(
    block,
    ref_fft,
    noise_method="mad",
    detection_threshold=8.0
):
    """
    Detect presence of a ping in one block.

    Parameters
    ----------
    block : np.ndarray
        Input time-domain samples (N = 256–512)
    ref_fft : np.ndarray
        FFT of reference ping
    noise_method : str
        'rms' or 'mad'
    detection_threshold : float
        Dimensionless threshold on normalized MF output

    Returns
    -------
    detected : bool
        Whether a ping was detected
    peak_value : float
        Peak normalized MF value
    peak_index : int
        Sample index of detection
    """

    # 1) Matched filter
    mf = fft_matched_filter(block, ref_fft)

    # 2) Noise estimate (block-level)
    if noise_method == "rms":
        noise = rms(mf)
    elif noise_method == "mad":
        noise = mad(mf)
    else:
        raise ValueError("noise_method must be 'rms' or 'mad'")

    # Protect against divide-by-zero
    if noise < 1e-12:
        return False, 0.0, None

    # 3) Normalize matched filter output
    mf_norm = mf / noise

    # 4) Peak detection
    peak_index = np.argmax(mf_norm)
    peak_value = mf_norm[peak_index]

    detected = peak_value > detection_threshold

    return detected, peak_value, peak_index


# ------------------------------------------------------------
# Example usage / test
# ------------------------------------------------------------

if __name__ == "__main__":

    # -----------------------
    # Simulation parameters
    # -----------------------
    fs = 1_000_000          # 1 MS/s
    f0 = 30_000             # 30 kHz
    block_size = 512
    ping_length = int(0.000512 * fs)  # 0.512 ms ping

    # -----------------------
    # Create reference ping
    # -----------------------
    t_ping = np.arange(ping_length) / fs
    ref_ping = np.sin(2 * np.pi * f0 * t_ping)

    # Zero-pad reference to block size
    ref_padded = np.zeros(block_size)
    ref_padded[:ping_length] = ref_ping

    # FFT of reference (precomputed once!)
    ref_fft = np.fft.fft(ref_padded)

    # -----------------------
    # Create test block
    # -----------------------
    noise = 0.5 * np.random.randn(block_size)

    block = noise.copy()

    # Inject ping at random location
    insert_idx = 100
    block[insert_idx:insert_idx + ping_length] += ref_ping[:block_size - insert_idx]

    # -----------------------
    # Run detector
    # -----------------------
    detected, peak, index = detect_ping(
        block,
        ref_fft,
        noise_method="rms",       # try "rms" as well
        detection_threshold=8.0
    )

    print("Detected:", detected)
    print("Peak value:", peak)
    print("Peak index:", index)
