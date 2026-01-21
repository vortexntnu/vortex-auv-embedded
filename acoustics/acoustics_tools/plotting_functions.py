import numpy as np
import matplotlib.pyplot as plt


# ============ Plotting Functions ============
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

def plot_reference_signal(reference_signal):
    i = np.arange(reference_signal.shape[0])
    plt.plot(i, reference_signal)
    plt.title('Reference Signal')
    plt.xlabel('Sample Index')  
    plt.ylabel('Amplitude')
    plt.show()

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