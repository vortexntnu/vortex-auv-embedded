import numpy as np
import matplotlib.pyplot as plt

import scipy.signal as scpy

from matplotlib.animation import FuncAnimation, PillowWriter
from matplotlib.widgets import Button

from functions import *
from plotting_functions import *
from julia_functions import load_simulation_config_json

# ==== Debug / Quick-run options ====
# Set to True to run only a limited number of frames and exit after printing SNR stats.
HEADLESS_SNR_ONLY = False
HEADLESS_MAX_FRAMES = 200

# ==== Load Simulation Configuration ====
config = load_simulation_config_json("simulation_config.json")
hydro_pos = config["hydrophones_pos"]
drone_pos = config["drone_pos"]
pinger_pos = config["pinger_pos"]
c = 1538.9235842  # speed of sound in water at ~15 degrees Celsius
print("Loaded simulation configuration.")
print(f"Hydrophone Positions: {hydro_pos}")
print(f"Drone Position: {drone_pos}")  
print(f"Pinger Position: {pinger_pos}")


# Run acoustic_data_simulator.jl to generate hydrophone data files before running this script
signal_data = load_all_hydrophone_data()
print("Loaded signal data from hydrophones.")


# ==== ADC Oversampling ====
ADC_out = np.zeros((5,), dtype=object)

oversampling_factor = 8
for i, (time, signal) in enumerate(signal_data):
    print(f"Processing Hydrophone {i+1} data...")
    # Oversample the signal
    oversampled_signal = adc_oversampling(signal, oversampling_factor)
    ADC_out[i] = oversampled_signal
    print(f"Hydrophone {i+1} signal oversampled.")

print("All hydrophone signals oversampled.")

# Compute signal range for proper axis scaling
min_val = min(np.min(adc) for adc in ADC_out)
max_val = max(np.max(adc) for adc in ADC_out)

def buffer_add_sample(signal, buffer, index):
    buffer[index % buffer_size] = signal[index]
    return buffer

def buffer_straigten(buffer, index):
    return np.concatenate((buffer[index % buffer_size:], buffer[:index % buffer_size]))

# initialize digital signal processing parameters
pinger_frequency = 30000  # 30 kHz
sampling_rate = 1000000  # 1 MHz
effective_sampling_rate = sampling_rate / oversampling_factor

# ==== Detection / template parameters ====
# A finite burst template is far more stable than a step-gated sine in multipath.
template_cycles = 20  # number of sine cycles in the matched-filter template
template_window = "tukey"  # "hann" | "tukey" | "rect"
template_tukey_alpha = 0.25
template_use_complex_iq = True  # robust to unknown carrier phase (recommended)

# Matched-filter CFAR-ish noise estimation (on MF power)
snr_threshold_db = 10.0  # detection threshold in dB for MF power / noise power
guard_len = None  # if None, derived from template length
noise_cells_fraction = 0.6  # fraction of buffer used as noise cells (excluding guard), spread on both sides


detection_area_size = max(np.linalg.norm(np.array(h) - np.array(hydro_pos[0])) for h in hydro_pos[1:]) * sampling_rate / (oversampling_factor * c)
detection_area_size = int(np.ceil(detection_area_size)) * 2
print(f"Estimated Minimum Detection Area Size: {detection_area_size} samples")
buffer_size = int(np.exp2(np.ceil(np.log2(detection_area_size))))
print(f"Buffer Size Set To: {buffer_size} samples")

buffers = np.zeros((5, buffer_size))

def generate_reference_signal(frequency, sampling_rate, output_length):
    t = np.arange(0, output_length/(sampling_rate), 1/sampling_rate)
    reference_signal = np.sin(2 * np.pi * frequency * t)
    for i in range(len(reference_signal)):
        if i < len(reference_signal)//2:
            reference_signal[i] *= 0
    return reference_signal

def generate_sine_burst_template(frequency_hz: float, fs_hz: float, cycles: int) -> np.ndarray:
    n = int(np.ceil(cycles * fs_hz / frequency_hz))
    n = max(8, n)
    t = np.arange(n) / fs_hz
    x = np.sin(2 * np.pi * frequency_hz * t)
    if template_window == "hann":
        w = np.hanning(n)
    elif template_window == "tukey":
        w = scpy.windows.tukey(n, alpha=template_tukey_alpha)
    else:
        w = np.ones(n)
    x = x * w
    x = x - np.mean(x)
    x_norm = np.linalg.norm(x)
    return x / x_norm if x_norm > 0 else x

def generate_complex_burst_template(frequency_hz: float, fs_hz: float, cycles: int) -> np.ndarray:
    n = int(np.ceil(cycles * fs_hz / frequency_hz))
    n = max(8, n)
    t = np.arange(n) / fs_hz
    x = np.exp(1j * 2 * np.pi * frequency_hz * t)
    if template_window == "hann":
        w = np.hanning(n)
    elif template_window == "tukey":
        w = scpy.windows.tukey(n, alpha=template_tukey_alpha)
    else:
        w = np.ones(n)
    x = x * w
    x_norm = np.linalg.norm(x)
    return x / x_norm if x_norm > 0 else x

def zero_padding(signal, desired_length):
    current_length = len(signal)
    if current_length >= desired_length:
        return signal[:desired_length]
    else:
        padding = np.zeros(desired_length - current_length)
        return np.concatenate((signal, padding))


if template_use_complex_iq:
    reference_signal_oversampled = generate_complex_burst_template(
        pinger_frequency,
        effective_sampling_rate,
        template_cycles,
    )
else:
    reference_signal_oversampled = generate_sine_burst_template(
        pinger_frequency,
        effective_sampling_rate,
        template_cycles,
    )

    

plot_reference_signal(np.real(reference_signal_oversampled))

for i in range(5):
    hydro_pos[i] = np.array(hydro_pos[i]) - np.array(drone_pos)  # Adjust for hydrophone offset

pinger_direction = np.array(pinger_pos) - hydro_pos[0]
pinger_direction = pinger_direction / np.linalg.norm(pinger_direction)

# ==== Animation Configuration ====
animation_length = 30  # seconds
desired_fps = 24  # Desired frames per second for the animation
animation_interval = 1000 // desired_fps  # Milliseconds between frames
frame_skip = buffer_size #ADC_out[0].shape[0] // (animation_length * desired_fps)  # Capture every N iterations as a frame
warmup_samples = buffer_size - 1  # wait until ring buffer is fully populated
frame_number = max(0, (ADC_out[0].shape[0] - warmup_samples) // frame_skip)
print(f"Animation will capture every {frame_skip} iterations.")

# Collect buffer states outputs for animation
buffer_frames = []
matched_filter_frames = []
noise_frames = []
SNR_frames = []
straigt_buffer_frames = []
detected_index_frames = []

noise_buffer = np.zeros(frame_number)
SNR_buffer = np.zeros(frame_number)
# ==== DSP parameters ====
SNR_threshold_db = snr_threshold_db  # kept for UI labels/plots

print("Running simulation and collecting buffer states...")
max_samples = ADC_out[0].shape[0]
if HEADLESS_SNR_ONLY:
    max_samples = min(max_samples, frame_skip * HEADLESS_MAX_FRAMES)

for i in range(max_samples):
    for j in range(ADC_out.shape[0]):
        buffers[j] = buffer_add_sample(ADC_out[j], buffers[j], i)

    straigt_buffer = buffer_straigten(buffers[0], i)

    buffer_is_full = i >= warmup_samples
    # Capture only when the ring buffer is full, and align to the end of each block.
    if buffer_is_full and ((i + 1) % frame_skip == 0):
        
        buffer_frames.append(buffers.copy())
        straigt_buffer_frames.append(straigt_buffer.copy())
        
        # Compute matched filter for reference hydrophone
        # Linear correlation via FFT convolution; using a finite template gives a sharp peak.
        h = reference_signal_oversampled
        x = straigt_buffer - float(np.mean(straigt_buffer))
        if np.iscomplexobj(h):
            mf_time = scpy.fftconvolve(x, np.conj(h[::-1]), mode="same")
        else:
            mf_time = scpy.fftconvolve(x, h[::-1], mode="same")
        mf_power = np.abs(mf_time) ** 2
        matched_filter_frames.append(mf_power.copy())

        # Noise floor estimation on MF power (CFAR-ish): exclude a guard band around candidate peak.
        # This keeps the noise estimate from inflating when the signal (and multipath peaks) appear.
        peaks, _ = scpy.find_peaks(mf_power)
        if peaks.size > 0:
            cand_idx = int(peaks[np.argmax(mf_power[peaks])])
        else:
            cand_idx = int(np.argmax(mf_power))

        g = guard_len
        if g is None:
            g = max(8, int(2 * len(h)))
        g = int(min(g, buffer_size // 2))

        # Choose noise cells from far away on both sides of the candidate.
        noise_cells_each_side = int((noise_cells_fraction * buffer_size) // 2)
        left_start = max(0, cand_idx - g - noise_cells_each_side)
        left_end = max(0, cand_idx - g)
        right_start = min(buffer_size, cand_idx + g)
        right_end = min(buffer_size, cand_idx + g + noise_cells_each_side)

        noise_cells = np.concatenate((mf_power[left_start:left_end], mf_power[right_start:right_end]))
        if noise_cells.size < 16:
            # Fallback: global median power (robust, but less precise)
            noise_floor = float(np.median(mf_power))
        else:
            noise_floor = float(np.mean(noise_cells))
        
        noise_buffer[:-1] = noise_buffer[1:]
        noise_buffer[-1] = noise_floor
        noise_frames.append(noise_buffer.copy())
        
        
        # Compute SNR
        SNR_buffer[:-1] = SNR_buffer[1:]
        # Prefer first significant peak above threshold for "arrival" under multipath.
        snr_curve_db = 10.0 * np.log10((mf_power + 1e-12) / (noise_floor + 1e-12))
        det_peaks, props = scpy.find_peaks(snr_curve_db, height=SNR_threshold_db)
        if det_peaks.size > 0:
            det_idx = int(det_peaks[0])
            peak_power = float(mf_power[det_idx])
        else:
            peak_power = float(np.max(mf_power))
            det_idx = -1
        snr_linear = peak_power / (noise_floor) if noise_floor > 0 else 0
        SNR_buffer[-1] = 10.0 * np.log10(snr_linear) if snr_linear > 0 else 0
        SNR_frames.append(SNR_buffer.copy())
        detected_index_frames.append(det_idx)

print(f"Simulation complete. Captured {len(buffer_frames)} frames.")

if HEADLESS_SNR_ONLY:
    snr_last = np.array([frame[-1] for frame in SNR_frames], dtype=float)
    best_idx = int(np.argmax(snr_last)) if snr_last.size else -1
    best_snr = float(snr_last[best_idx]) if best_idx >= 0 else float("nan")
    print(f"Max SNR: {best_snr:.2f} dB at frame {best_idx} (iteration {best_idx * frame_skip})")
    raise SystemExit(0)

# Compute matched filter range for proper axis scaling
mf_min = min(np.min(np.real(mf)) for mf in matched_filter_frames)
mf_max = max(np.max(np.real(mf)) for mf in matched_filter_frames)

noise_min = min(np.min(noise) for noise in noise_frames)
noise_max = max(np.max(noise) for noise in noise_frames)  # Add some headroom

SNR_min = min(np.min(SNR) for SNR in SNR_frames)
SNR_max = max(np.max(SNR) for SNR in SNR_frames) + 5  # Add some headroom

# Create animation with interactive controls
n = 5
fig = plt.figure(figsize=(12, 10))
axes = [plt.subplot(5, 1, i+1) for i in range(5)]
lines = []
for j in range(5):
    line, = axes[j].plot(buffer_frames[0][j])
    lines.append(line)
    axes[j].set_title(f'Hydrophone {j+1} Buffer')
    axes[j].set_ylim(min_val, max_val)
    axes[j].set_xlabel('Sample Index')

# Detection marker (used in matched-filter view)
det_vline = axes[1].axvline(0, color='r', linewidth=1, alpha=0.9)
det_vline.set_visible(False)

skip_n = 1  # Number of frames to skip on skip button press

# Animation state
animation_state = {'paused': False, 'current_frame': 0, 'show_matched_filter': False}

def animate(frame_num):
    if not animation_state['paused']:
        animation_state['current_frame'] = frame_num
    else:
        frame_num = animation_state['current_frame']
    
    if animation_state['show_matched_filter']:
        # Display only matched filter output for reference hydrophone
        lines[0].set_ydata(straigt_buffer_frames[frame_num])
        axes[0].set_title('Hydrophone 1 - Straigt Buffer Output')
        axes[0].set_ylim(min_val, max_val)
        
        lines[1].set_ydata(np.real(matched_filter_frames[frame_num]))
        axes[1].set_title('Hydrophone 1 - Matched Filter Output')
        axes[1].set_ylim(mf_min, mf_max)

        det_idx = detected_index_frames[frame_num] if frame_num < len(detected_index_frames) else -1
        if det_idx is not None and det_idx >= 0:
            det_vline.set_xdata([det_idx, det_idx])
            det_vline.set_visible(True)
        else:
            det_vline.set_visible(False)

        lines[2].set_xdata(np.arange(frame_number))
        lines[2].set_ydata(noise_frames[frame_num])
        axes[2].set_title('Hydrophone 1 - Noise Output')
        axes[2].set_ylim(noise_min, noise_max)

        lines[3].set_xdata(np.arange(frame_number))
        lines[3].set_ydata(SNR_frames[frame_num])
        axes[3].set_title('Hydrophone 1 - SNR Output')
        axes[3].set_ylim(SNR_min, SNR_max)

        # Hide other axes
        for j in range(4, 5):
            axes[j].set_visible(False)
    else:
        # Display buffer data for all hydrophones
        det_vline.set_visible(False)
        lines[2].set_xdata(np.arange(buffer_size))
        lines[3].set_xdata(np.arange(buffer_size))
        for j in range(5):
            lines[j].set_ydata(buffer_frames[frame_num][j])
            axes[j].set_title(f'Hydrophone {j+1} Buffer')
            axes[j].set_ylim(min_val, max_val)
            axes[j].set_visible(True)
    
    status = 'PAUSED' if animation_state['paused'] else 'PLAYING'
    view_mode = 'Matched Filter' if animation_state['show_matched_filter'] else 'Buffers'
    fig.suptitle(
        f'{view_mode} - Frame {frame_num}/{len(buffer_frames)-1} '
        f'(Iteration {warmup_samples + (frame_num + 1) * frame_skip - 1}) [{status}]'
    )
    return lines

ani = FuncAnimation(fig, animate, frames=len(buffer_frames), interval=animation_interval, repeat=True)

# Create control buttons
ax_play = plt.axes([0.2, 0.02, 0.08, 0.04])
ax_skip_back = plt.axes([0.1, 0.02, 0.08, 0.04])
ax_skip_fwd = plt.axes([0.3, 0.02, 0.08, 0.04])
ax_toggle = plt.axes([0.42, 0.02, 0.08, 0.04])
ax_save = plt.axes([0.52, 0.02, 0.08, 0.04])

btn_play = Button(ax_play, 'Pause/Play')
btn_skip_back = Button(ax_skip_back, 'Skip Back')
btn_skip_fwd = Button(ax_skip_fwd, 'Skip Fwd')
btn_toggle = Button(ax_toggle, 'Toggle View')
btn_save = Button(ax_save, 'Save GIF')

def on_play(event):
    animation_state['paused'] = not animation_state['paused']

def on_skip_back(event):
    animation_state['current_frame'] = max(0, animation_state['current_frame'] - skip_n)
    animation_state['paused'] = True

def on_skip_fwd(event):
    animation_state['current_frame'] = min(len(buffer_frames) - 1, animation_state['current_frame'] + skip_n)
    animation_state['paused'] = True

def on_toggle(event):
    animation_state['show_matched_filter'] = not animation_state['show_matched_filter']
    animation_state['paused'] = True

def on_save(event):
    print("Saving animation as GIF...")
    writer = PillowWriter(fps=desired_fps)
    ani.save('buffer_animation.gif', writer=writer)
    print("Animation saved as 'buffer_animation.gif'")

btn_play.on_clicked(on_play)
btn_skip_back.on_clicked(on_skip_back)
btn_skip_fwd.on_clicked(on_skip_fwd)
btn_toggle.on_clicked(on_toggle)
btn_save.on_clicked(on_save)

plt.subplots_adjust(bottom=0.12)
plt.show()
    

""" print("Pinger Direction Vector:", pinger_direction)
print("Pinger Position:", pinger_pos) """