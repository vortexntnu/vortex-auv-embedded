"""
DSP utilities for hydrophone signal processing and simulator orchestration.
"""
import numpy as np
import scipy.signal as scpy
import os


def TDOA_pos_solve(r, t, c):
    """
    Functions for TDOA based localization and signal processing

    Parameters
    ----------
    r : list of np.arrays
        receiver positions (3D)
    t : list of floats
        time of arrivals
    c : float
        speed of sound in medium
    """
    r = [np.asarray(ri, dtype=float).reshape(-1) for ri in r]
    t = np.asarray(t, dtype=float).reshape(-1)

    if len(r) != len(t):
        raise ValueError(f"len(r) must equal len(t), got {len(r)} and {len(t)}")
    if any(ri.shape[0] != 3 for ri in r):
        raise ValueError("Each receiver position r[i] must be 3D (shape (3,))")
    if len(t) < 5:
        # 4 unknowns => need at least 4 equations => n-1 >= 4 => n >= 5
        raise ValueError("Need at least 5 receivers for this 4-unknown linear TDOA formulation")

    n = len(t)
    A = []
    b = []

    for i in range(1, n):
        delta_d = c * (t[i] - t[0])

        Ai = 2 * (r[0] - r[i])
        Ai = np.append(Ai, -2 * delta_d)  # NOTE: matches 2*(Δd0-Δdi) with Δd0=0
        bi = (np.dot(r[0], r[0]) - np.dot(r[i], r[i]) + delta_d**2)

        A.append(Ai)
        b.append(bi)

    A = np.asarray(A, dtype=float)
    b = np.asarray(b, dtype=float)

    AT = A.T
    M = AT @ A + np.identity(4) * 1e-6
    y = AT @ b

    L = np.linalg.cholesky(M)
    x = np.zeros(4)
    for i in range(4):
        x[i] = (y[i] - np.dot(L[i][:i], x[:i])) / L[i][i]

    LT = L.T
    p = np.zeros(4)
    for i in range(3, -1, -1):
        p[i] = (x[i] - np.dot(LT[i][i + 1:], p[i + 1:])) / LT[i][i]

    D0 = p[3]
    P = p[:3]
    return P, D0



def TDOA_direction_solve(r,t,c):
    """
    Functions for TDOA based localization and signal processing

    Parameters
    ----------
    r : list of np.arrays
        receiver positions
    t : list of floats
        time of arrivals
    c : float
        speed of sound in medium
    """

    #Generate linear system on form Ap = b so that we can solve for p
    n = len(t)
    A = []
    b = []

    for i in range(1,n):
        Ai = (r[0]-r[i])
        bi = c*(t[i]-t[0])

        A.append(Ai)
        b.append(bi)

    A = np.array(A)
    b = np.array(b)

    #Use least squares to make a system that is easier to solve and fixes other things

    AT = A.T #Transpose
    M = np.matmul(AT,A) + np.identity(3)*10**-6
    y = np.matmul(AT,b)

    #now we solve Mp = y wher M is Semi positive definite and symetrical
    #for this we use cholesky
    
    L = np.linalg.cholesky(M)
    LT = L.T

    #solve L*L^T * p = y by first solving L*x = y for x, then solving L^T * p = L*x for p
    #since L and L^T are triangular this is trivial

    x = np.zeros(3)

    for i in range(3):
        x[i] = (y[i]-np.dot(L[i][:i],x[:i]))/L[i][i]

    p = np.zeros(3)

    for i in range(2,-1,-1):
        p[i] = (x[i]-np.dot(LT[i][i+1:],p[i+1:]))/LT[i][i]
    
    return p

def tdoa_localization(sensors, times, c,
                      huber_delta=0.2,
                      max_iter=16,
                      lambda0=1e-3,
                      tdoa_std=1e-5):
    """
    Full TDOA pipeline:
    - Linear initial estimate
    - Levenberg-Marquardt refinement
    - Huber robust loss
    - Error covariance estimation

    Parameters
    ----------
    sensors : (5,3) ndarray
        Hydrophone positions
    times   : (5,) ndarray
        Arrival times
    c       : float
        Speed of sound
    huber_delta : float
        Huber threshold (meters)
    max_iter : int
        LM max iterations
    lambda0 : float
        Initial LM damping
    tdoa_std : float
        Standard deviation of TDOA distances (meters)

    Returns
    -------
    p_est : (3,) ndarray
        Estimated source position
    cov_p : (3,3) ndarray
        Estimated position covariance
    """

    # -------------------------------
    # Step 0: precompute TDOAs
    # -------------------------------
    s0 = sensors[0]
    t0 = times[0]
    d = c * (times[1:] - t0)   # distances relative to reference

    # -------------------------------
    # Step 1: Linear initial estimate
    # -------------------------------
    A = np.zeros((4,3))
    b = np.zeros(4)
    for i in range(1,5):
        diff = sensors[i] - s0
        A[i-1,:] = 2 * diff
        b[i-1] = np.dot(sensors[i],sensors[i]) - np.dot(s0,s0) - d[i-1]**2

    # Least squares solution
    """ p0, _, _, _ = np.linalg.lstsq(A, b, rcond=None)
    p = p0.copy() """

    U, S, Vh = np.linalg.svd(A,compute_uv=True,full_matrices=True)
    S_inv = np.zeros((3,4))
    for i in range(3):
        if S[i] > 1e-12:
            S_inv[i,i] = 1.0 / S[i]
    
    p = Vh.T @ S_inv @ U.T @ b

    lam = lambda0

    # -------------------------------
    # Step 2: LM iterations with Huber
    # -------------------------------
    def compute_residual_and_jacobian(p):
        residual = np.zeros(4)
        J = np.zeros((4,3))
        dist0 = np.linalg.norm(p - s0)
        u0 = (p - s0) / max(dist0, 1e-6)
        for i in range(1,5):
            si = sensors[i]
            diff = p - si
            dist_i = np.linalg.norm(diff)
            ui = diff / max(dist_i, 1e-6)
            residual[i-1] = (dist_i - dist0) - d[i-1]
            J[i-1,:] = ui - u0
        return residual, J

    residual, J = compute_residual_and_jacobian(p)
    err = residual @ residual

    for _ in range(max_iter):
        # Huber weights
        w = np.ones_like(residual)
        mask = np.abs(residual) > huber_delta
        w[mask] = huber_delta / np.abs(residual[mask])

        # Weighted residuals and Jacobian
        W_sqrt = np.sqrt(w)
        residual_w = residual * W_sqrt
        J_w = J * W_sqrt[:, np.newaxis]

        # LM step
        JTJ = J_w.T @ J_w
        JTr = J_w.T @ residual_w
        H_lm = JTJ + lam * np.eye(3)
        delta_p = np.linalg.solve(H_lm, JTr)

        # Candidate update
        p_candidate = p - delta_p
        residual_new, J_new = compute_residual_and_jacobian(p_candidate)
        err_new = residual_new @ residual_new

        if err_new < err:
            # Accept
            p = p_candidate
            residual = residual_new
            J = J_new
            err = err_new
            lam *= 0.3
        else:
            lam *= 10.0

    # -------------------------------
    # Step 3: Error covariance
    # -------------------------------
    # Using weighted Jacobian at final solution
    w = np.ones_like(residual)
    mask = np.abs(residual) > huber_delta
    w[mask] = huber_delta / np.abs(residual[mask])
    W_sqrt = np.sqrt(w)
    J_w = J * W_sqrt[:, np.newaxis]

    cov_p = tdoa_std**2 * np.linalg.inv(J_w.T @ J_w)

    return p, cov_p

def adc_oversampling(signal,s):
    """
    Performs oversampling by averaging the signal in blocks of size s.

    Parameters
    ----------
    signal : array-like
        The input signal.
    s : int
        The block size for averaging.

    Returns
    -------
    np.array
        The oversampled signal.
    """
    N = len(signal)
    n = len(signal)//s
    if N%s != 0:
        n += 1

    new_signal = np.zeros(n)

    for i in range(n):
        if N-i*s < s:
            new_signal[i] = np.average(signal[i*s:N])
        else:
            new_signal[i] = np.average(signal[i*s:i*s+s])

    return new_signal

def single_freq_DFT(signal,freq,dt):
    """
    Computes the Discrete Fourier Transform at a single frequency.

    Parameters
    ----------
    signal : array-like
        The input signal.
    freq : float
        The frequency to analyze.
    dt : float
        The time step between samples.

    Returns
    -------
    tuple
        (amplitude, phase) of the signal at the given frequency.
    """
    n = len(signal)
    R = 0
    I = 0
    for i in range(n):
        R += np.cos(2*np.pi*i*freq*dt)*signal[i]
        I += -np.sin(2*np.pi*i*freq*dt)*signal[i]

    amplitude = 2*np.sqrt(R**2+I**2)/n
    phase = np.atan2(I,R)

    #print("Amplitude",amplitude)
    #print("Phase",phase)

    return amplitude,phase

def TDOA_calculate(times):
    """
    Calculates time differences for TDOA relative to the first time.

    Parameters
    ----------
    times : list of float
        List of arrival times.

    Returns
    -------
    list
        Time differences relative to the first time.
    """
    time_differences = []
    for i in range(1,len(times)):
        time_differences.append(times[i]-times[0])

    return time_differences

def match_filter(signal, template):
    """
    Applies matched filtering to the input signal using the provided template.

    Parameters
    ----------
    signal : array-like
        The input signal.
    template : array-like
        The template signal to match.

    Returns
    -------
    np.array
        The filtered signal.
    """
    filtered_signal = scpy.correlate(signal, template, mode='same')
    return filtered_signal

def FFT(signal, dt):
    """
    Computes the Fast Fourier Transform of the input signal.

    Parameters
    ----------
    signal : array-like
        The input signal.
    dt : float
        The time step between samples.

    Returns
    -------
    tuple
        (frequencies, magnitudes) of the FFT.
    """
    n = len(signal)
    fft_result = np.fft.fft(signal)
    freqs = np.fft.fftfreq(n, dt)
    magnitudes = np.abs(fft_result) / n

    return freqs, magnitudes

def IFFT(freqs, magnitudes):
    """
    Computes the Inverse Fast Fourier Transform from frequency domain data.

    Parameters
    ----------
    freqs : array-like
        The frequency bins.
    magnitudes : array-like
        The magnitudes corresponding to the frequencies.

    Returns
    -------
    np.array
        The reconstructed time-domain signal.
    """
    n = len(magnitudes)
    complex_spectrum = magnitudes * np.exp(1j * 0)  # Assuming zero phase for simplicity
    signal = np.fft.ifft(complex_spectrum * n).real

    return signal

def signal_generate(frequency, phase, duration, dt):
    """
    Generates a sine wave signal.

    Parameters
    ----------
    frequency : float
        Frequency of the sine wave.
    phase : float
        Phase of the sine wave in radians.
    duration : float
        Duration of the signal in seconds.
    dt : float
        Time step between samples.

    Returns
    -------
    np.array
        The generated sine wave signal.
    """
    t = np.arange(0, duration, dt)
    signal = np.sin(2 * np.pi * frequency * t + phase)
    return signal

# Detect signal presence
# Compute reference signal fourier transform
def matched_filtering_fft(signal_fft, reference_signal_fft):
    """Perform matched filtering in the frequency domain.

    Parameters
    ----------
    signal_fft : np.ndarray
        Complex FFT of the input signal.
    reference_signal_fft : np.ndarray
        Complex FFT of the reference (template) signal.

    Returns
    -------
    np.ndarray
        Frequency-domain matched-filter output suitable for IFFT.
    """
    matched_filter_output_fft = signal_fft * np.conj(reference_signal_fft)
    return matched_filter_output_fft

def signal_fft_power(X):
    """Compute the power of a signal in the frequency domain.
    Parameters
    ----------
    X : np.ndarray
        Complex FFT of the signal.
    Returns
    -------
    float
        Power of the signal.
    """
    return np.sum(np.real(X*np.conj(X)))

def compute_snr_db(signal_power, noise_power):
    """Compute SNR in decibels.

    Parameters
    ----------
    signal_power : float
        Power of the signal.
    noise_power : float
        Power of the noise.

    Returns
    -------
    float
        SNR in decibels.
    """
    if noise_power == 0:
        return 0
    if signal_power == 0:
        return 0
    snr = 10 * np.log10(signal_power / noise_power)
    return snr

def Hilbert_envelope(signal):
    """Compute the Hilbert envelope of a real-valued signal.

    Parameters
    ----------
    signal : np.ndarray
        Real-valued input signal.

    Returns
    -------
    np.ndarray
        Hilbert envelope of the input signal.
    """
    analytic_signal = scpy.hilbert(signal)
    envelope = np.abs(analytic_signal)
    return envelope

def CFAR_Thresholding(signal, window_size, guard_size, p_fa):
    """Apply CFAR thresholding to a 1D signal.

    Parameters
    ----------
    signal : np.ndarray
        Input signal.
    window_size : int
        Size of the training window on each side.
    guard_size : int
        Size of the guard window on each side.
    p_fa : float
        Desired probability of false alarm.

    Returns
    -------
    np.ndarray
        Binary array indicating detections (1) and non-detections (0).
    """
    n = len(signal)
    thresholded_signal = np.zeros(n)
    threshold_line = np.zeros(n)
    alpha = window_size * (p_fa ** (-1 / window_size) - 1)  # CFAR scaling factor

    for i in range(n):
        start = max(0, i - guard_size - window_size)
        end = min(n, i + guard_size + window_size + 1)

        # Exclude guard cells
        training_cells = np.concatenate((signal[start:i - guard_size], signal[i + guard_size + 1:end]))
        noise_level = np.mean(training_cells) if len(training_cells) > 0 else 0

        threshold = alpha * noise_level
        threshold_line[i] = threshold

        if signal[i] > threshold:
            thresholded_signal[i] = 1

    return thresholded_signal, threshold_line

def angle_between_directions_deg(dir1, dir2):
    """Calculate the (unsigned) angle in degrees between two direction vectors.

    Robust implementation using atan2(||cross||, dot), returning [0, 180].

    Parameters
    ----------
    dir1 : np.ndarray
        First direction vector.
    dir2 : np.ndarray
        Second direction vector.

    Returns
    -------
    float
        Angle between the two directions in degrees.
    """
    dir1 = np.asarray(dir1, dtype=float)
    dir2 = np.asarray(dir2, dtype=float)

    n1 = np.linalg.norm(dir1)
    n2 = np.linalg.norm(dir2)
    if n1 == 0 or n2 == 0:
        return float("nan")

    a = dir1 / n1
    b = dir2 / n2

    dot = float(np.clip(np.dot(a, b), -1.0, 1.0))
    cross_norm = float(np.linalg.norm(np.cross(a, b)))

    return float(np.degrees(np.arctan2(cross_norm, dot)))

#def 
def angle_between_directions_deg2(dir1, dir2):
    """Calculate the angle in degrees between two direction vectors.

    Parameters
    ----------
    dir1 : np.ndarray
        First direction vector.
    dir2 : np.ndarray
        Second direction vector.

    Returns
    -------
    float
        Angle between the two directions in degrees.
    """
    dir1_normalized = dir1 / np.linalg.norm(dir1)
    dir2_normalized = dir2 / np.linalg.norm(dir2)
    dot_product = np.clip(np.dot(dir1_normalized, dir2_normalized), -1.0, 1.0)
    angle_rad = np.arccos(dot_product)
    angle_deg = np.degrees(angle_rad)

    dir_angle_cos = float(np.clip(np.dot(dir1_normalized, dir2_normalized), -1.0, 1.0))
    ortho_direction = dir1_normalized - dir_angle_cos*dir2_normalized
    ortho_direction = ortho_direction / np.linalg.norm(ortho_direction)
    dir_angle_sin = float(np.clip(np.dot(dir1_normalized,   ortho_direction), -1.0, 1.0))
    angle_deg = float(np.degrees(np.atan2(dir_angle_sin, dir_angle_cos)))
    return angle_deg

def generate_reference_signal(frequency, sampling_rate, output_length):
    t = np.arange(0, output_length/(sampling_rate), 1/sampling_rate)
    reference_signal = np.sin(2 * np.pi * frequency * t)
    for i in range(len(reference_signal)):
        if i < len(reference_signal)//2:
            reference_signal[i] *= 0
    return reference_signal