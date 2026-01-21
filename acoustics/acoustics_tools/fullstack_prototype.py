import numpy as np
import matplotlib.pyplot as plt

from matplotlib.animation import FuncAnimation, PillowWriter
from matplotlib.widgets import Button

from functions import *
from plotting_functions import *
from julia_functions import load_simulation_config_json

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


reference_signal = generate_reference_signal(pinger_frequency, sampling_rate, buffer_size*oversampling_factor)

reference_signal_oversampled = adc_oversampling(reference_signal, oversampling_factor)

plot_reference_signal(reference_signal_oversampled)

for i in range(5):
    hydro_pos[i] = np.array(hydro_pos[i]) - np.array(drone_pos)  # Adjust for hydrophone offset

pinger_direction = np.array(pinger_pos) - hydro_pos[0]
pinger_direction = pinger_direction / np.linalg.norm(pinger_direction)

# ==== Animation Configuration ====
animation_length = 30  # seconds
desired_fps = 24  # Desired frames per second for the animation
animation_interval = 1000 // desired_fps  # Milliseconds between frames
frame_skip = buffer_size #ADC_out[0].shape[0] // (animation_length * desired_fps)  # Capture every N iterations as a frame
frame_number = ADC_out[0].shape[0] // frame_skip
print(f"Animation will capture every {frame_skip} iterations.")

# Collect buffer states and matched filter outputs for animation
buffer_frames = []
matched_filter_frames = []
noise_frames = []
SNR_frames = []
straigt_buffer_frames = []

noise_buffer = np.zeros(frame_number)

SNR_buffer = np.zeros(frame_number)

print("Running simulation and collecting buffer states...")
for i in range(ADC_out[0].shape[0]):
    for j in range(ADC_out.shape[0]):
        buffers[j] = buffer_add_sample(ADC_out[j], buffers[j], i)

    straigt_buffer = buffer_straigten(buffers[0], i)
    
    if i % frame_skip == 0:  # Capture frame every frame_skip iterations
        
        buffer_frames.append(buffers.copy())
        straigt_buffer_frames.append(straigt_buffer.copy())
        
        # Compute matched filter for reference hydrophone
        reference_hydrophone_fft = np.fft.fftshift(np.fft.fft(straigt_buffer))
        reference_signal_fft = np.fft.fftshift(np.fft.fft(reference_signal_oversampled, n=buffer_size))

        mf_fft = matched_filtering_fft(reference_hydrophone_fft, reference_signal_fft)
        matched_reference_buffer = np.fft.ifftshift(np.fft.ifft(mf_fft))
        matched_filter_frames.append(matched_reference_buffer.copy()) 

        noies_estimate_type = "RMS"
        if noies_estimate_type == "RMS":
            noise = np.sqrt(np.mean(straigt_buffer**2))
        elif noies_estimate_type == "MAD":
            pass
        
        noise_buffer[:-1] = noise_buffer[1:]
        noise_buffer[-1] = noise
        noise_frames.append(noise_buffer.copy())
            

        # Compute SNR
        SNR_buffer[:-1] = SNR_buffer[1:]
        peak_signal = np.max(np.abs(matched_reference_buffer))
        mf_norm = peak_signal / noise if noise != 0 else 0
        SNR_buffer[-1] = 20 * np.log10(mf_norm) if mf_norm != 0 else 0
        SNR_frames.append(SNR_buffer.copy())


print(f"Simulation complete. Captured {len(buffer_frames)} frames.")

# Compute matched filter range for proper axis scaling
mf_min = min(np.min(np.real(mf)) for mf in matched_filter_frames)
mf_max = max(np.max(np.real(mf)) for mf in matched_filter_frames)

noise_min = min(np.min(noise) for noise in noise_frames)
noise_max = max(np.max(noise) for noise in noise_frames) + 10  # Add some headroom

SNR_min = min(np.min(SNR) for SNR in SNR_frames)
SNR_max = max(np.max(SNR) for SNR in SNR_frames) + 10  # Add some headroom

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
        lines[2].set_xdata(np.arange(buffer_size))
        lines[3].set_xdata(np.arange(buffer_size))
        for j in range(5):
            lines[j].set_ydata(buffer_frames[frame_num][j])
            axes[j].set_title(f'Hydrophone {j+1} Buffer')
            axes[j].set_ylim(min_val, max_val)
            axes[j].set_visible(True)
    
    status = 'PAUSED' if animation_state['paused'] else 'PLAYING'
    view_mode = 'Matched Filter' if animation_state['show_matched_filter'] else 'Buffers'
    fig.suptitle(f'{view_mode} - Frame {frame_num}/{len(buffer_frames)-1} (Iteration {frame_num*frame_skip}) [{status}]')
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