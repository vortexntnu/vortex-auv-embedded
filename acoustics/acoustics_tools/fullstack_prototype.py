import numpy as np
import matplotlib.pyplot as plt
from functions import *


# Run acoustic_data_simulator.jl to generate hydrophone data files before running this script
signal_data = load_all_hydrophone_data()
print("Loaded signal data from hydrophones.")

ADC_out = np.zeros((5,), dtype=object)
for i, (time, signal) in enumerate(signal_data):
    print(f"Processing Hydrophone {i+1} data...")
    # Oversample the signal
    s = 8  # oversampling factor
    oversampled_signal = adc_oversampling(signal, s)
    ADC_out[i] = oversampled_signal
    print(f"Hydrophone {i+1} signal oversampled.")

print("All hydrophone signals oversampled.")
def plot_oversampled_signals(ADC_out):
    # Plot oversampled signals
    i = np.arange(ADC_out[0].shape[0])
    plt.figure(figsize=(10, 8))
    for j in range(5):  
        plt.subplot(5, 1, j+1)
        plt.plot(i, ADC_out[j])
        plt.title(f'Hydrophone {j+1} Oversampled Signal')
        plt.xlabel('Sample Index')
        plt.ylabel('Amplitude')

    plt.tight_layout()
    plt.show()

def buffer_add_sample(signal, buffer, index):
    buffer[:-1] = buffer[1:]
    buffer[-1] = signal[index]
    return buffer

# initialize buffers
buffer_size = 1024
buffers = np.zeros((5, buffer_size))

def generate_reference_signal(frequency, sampling_rate, duration, output_length=None):
    t = np.arange(0, duration, 1/sampling_rate)
    reference_signal = np.sin(2 * np.pi * frequency * t)
    if output_length is not None:
        if len(reference_signal) < output_length:
            # Pad with zeros evenly on both sides
            pad_total = output_length - len(reference_signal)
            pad_left = pad_total // 2
            pad_right = pad_total - pad_left
            padded = np.zeros(output_length)
            padded[pad_left : pad_left + len(reference_signal)] = reference_signal
            reference_signal = padded
        elif len(reference_signal) > output_length:
            # Truncate if longer
            reference_signal = reference_signal[:output_length]
    return reference_signal

pinger_frequency = 30000  # 30 kHz
sampling_rate = 125000  # 125 kHz
duration = 0.004  # 4 ms
reference_signal = generate_reference_signal(pinger_frequency, sampling_rate, duration)
reference_signal_with_padding = generate_reference_signal(pinger_frequency, sampling_rate, duration, output_length=buffer_size)

def plot_reference_signal(reference_signal):
    i = np.arange(reference_signal.shape[0])
    plt.plot(i, reference_signal)
    plt.title('Reference Signal')
    plt.xlabel('Sample Index')  
    plt.ylabel('Amplitude')
    plt.show()

plot_reference_signal(reference_signal_with_padding)

# Detect signal presence
# Compute reference signal fourier transform
reference_signal_fft = np.fft.fft(reference_signal)
def matched_filtering_fft(signal_fft, reference_signal_fft):
    # Perform matched filtering in frequency domain
    matched_filter_output_fft = signal_fft * np.conj(reference_signal_fft)
    return matched_filter_output_fft

def plot_matched_filter_fft_output(input_signal, reference_signal_fft):
    mf = matched_filtering_fft(np.fft.fft(input_signal), reference_signal_fft)
    i = np.arange(len(mf))
    plt.plot(i, mf**2)
    plt.title('Matched Filter Output (Hydrophone 1)')
    plt.xlabel('Frequency Bin')
    plt.ylabel('Magnitude')
    plt.show()

def plot_matched_filter_time_output(input_signal, reference_signal):
    mf_time = np.fft.ifft(matched_filtering_fft(np.fft.fft(input_signal), np.fft.fft(reference_signal)))
    i = np.arange(len(mf_time))
    plt.plot(i - len(mf_time)//2, np.fft.fftshift(np.abs(mf_time)))
    plt.title('Matched Filter Time Domain Output')
    plt.xlabel('Lag')
    plt.ylabel('Magnitude')
    plt.show()

def detect_signal_in_buffer(buffer, reference_signal, reference_signal_fft, threshold):
    # Perform matched filtering
    starting_index = (buffer_size - len(reference_signal))//2
    buffer = buffer[starting_index:(starting_index + len(reference_signal))]
    buffer_fft = np.fft.fft(buffer)
    mf_output_fft = matched_filtering_fft(buffer_fft, reference_signal_fft)
    mf_output = np.fft.ifft(mf_output_fft)
    mf_magnitude = np.abs(mf_output)

    # Check if any value exceeds the threshold
    max_val = np.max(mf_magnitude)
    #print(f"Max matched filter magnitude: {max_val}")
    if max_val > threshold:
        peak_index = np.argmax(mf_magnitude)
        return True, mf_magnitude, peak_index
    else:
        return False, mf_magnitude, None
    
def calculate_TDOA(peak_indices, sampling_rate):
    tdoa = np.array(peak_indices) / sampling_rate
    return tdoa

def cross_correlation_tdoa(buffers):
    peak_indices = []
    for j in range(0,5):
        correlation = scpy.correlate(buffers[j], buffers[0], mode='full')
        peak_index = np.argmax(np.abs(correlation))
        peak_indices.append(peak_index)
    tdoa = calculate_TDOA(peak_indices, sampling_rate)
    return tdoa

#plot_reference_signal(reference_signal)

mf_magnitude_list = np.zeros(ADC_out[0].shape[0], dtype=object)
print(ADC_out[0].shape[0])

max_magnitude = 0
peak_reached = False
peak_i = -1
peak_index_in_buffer = -1
decrease_counter = 0
patience = 100  # Number of consecutive decreases before breaking

hydro_pos = [
    [0.5, 0.5, 0.5],
    [1.0, 0.0, 0.0],
    [0.0, 1.0, 0.0],
    [1.0, 1.0, 0.0],
    [0.0, 0.0, 0.0]
]

pinger_pos = [10.0, 5.0, -18.0]
pinger_direction = np.array(pinger_pos) - np.array([0.5, 0.5, 0.5])
pinger_direction = pinger_direction / np.linalg.norm(pinger_direction)

for i in range(ADC_out[0].shape[0]):
    for j in range(5):
        buffers[j] = buffer_add_sample(ADC_out[j], buffers[j], i)

    signal_found, mf_magnitude, peak_index = detect_signal_in_buffer(buffers[0], reference_signal, reference_signal_fft, threshold=10)
    mf_magnitude_list[i] = mf_magnitude
    current_max = np.max(mf_magnitude)
    
    if signal_found and not peak_reached:
        peak_reached = True
        print(f"Signal detected at sample index {i}")
    
    if peak_reached:
        if current_max > max_magnitude:
            max_magnitude = current_max
            peak_i = i
            peak_index_in_buffer = peak_index
            decrease_counter = 0  # Reset counter
        elif current_max < max_magnitude:
            decrease_counter += 1
            if decrease_counter > patience:
                # Magnitude has been decreasing for 'patience' steps, break
                print(f"Peak reached at sample {peak_i}, peak index in buffer {peak_index_in_buffer}")
                #plot_matched_filter_time_output(buffers[0], reference_signal_with_padding)
                #plot_matched_filter_time_output(buffers[1], reference_signal_with_padding)
                tdoa = cross_correlation_tdoa(buffers)
                print("TDOA:", tdoa)
                p = TDOA_direction_solve(np.array(hydro_pos), tdoa,1500)
                print("Estimated direction:", p/np.linalg.norm(p))
                print("True direction:", pinger_direction)
                break
        else:
            decrease_counter = 0  # If equal, reset

# Plot the max magnitudes over time
plt.plot(np.arange(len(mf_magnitude_list)), [np.max(mf) if mf is not None else 0 for mf in mf_magnitude_list])
plt.show()
# Plot final buffer states
def plot_buffer_states(buffers, buffer_size):
    plt.figure(figsize=(10, 8))
    for j in range(5):  
        plt.subplot(5, 1, j+1)
        plt.plot(np.arange(buffer_size), buffers[j])
        plt.title(f'Hydrophone {j+1} Final Buffer State')
        plt.xlabel('Sample Index')
        plt.ylabel('Amplitude')
    plt.tight_layout()
    plt.show()

plot_buffer_states(buffers, buffer_size)


# TDOA calculate


# Solve position from TDOA