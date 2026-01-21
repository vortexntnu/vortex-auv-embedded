"""
DSP utilities for hydrophone signal processing and simulator orchestration.
"""
import numpy as np
import scipy.signal as scpy


def TDOA_pos_solve(r,t,c):
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
        Ai = 2*(r[0]-r[i])
        bi = c**2*(t[i]**2-t[0]**2) + np.dot(r[0],r[0]) - np.dot(r[i],r[i])

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
        p[i] = (x[i]-np.dot(LT[i][i:],p[i:]))/LT[i][i]
    
    return p

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
        p[i] = (x[i]-np.dot(LT[i][i:],p[i:]))/LT[i][i]
    
    return p

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

def load_hydrophone_data(file_path):
    """
    Loads hydrophone data from a CSV file and returns time and signal as numpy arrays.

    Parameters
    ----------
    file_path : str
        Path to the CSV file containing 'time,signal' columns.

    Returns
    -------
    tuple
        (time_array, signal_array) as numpy arrays.
    """
    data = np.loadtxt(file_path, delimiter=',', skiprows=1)
    time = data[:, 0]
    signal = data[:, 1]
    return time, signal

def load_all_hydrophone_data():
    """
    Loads data from all 5 hydrophone CSV files.

    Returns
    -------
    list
        List of tuples [(time1, signal1), (time2, signal2), ..., (time5, signal5)].
    """
    hydro_data = []
    for i in range(1, 6):
        file_path = f'hydrophone_{i}_data.csv'
        time, signal = load_hydrophone_data(file_path)
        hydro_data.append((time, signal))
    return hydro_data

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