import numpy as np
import matplotlib.pyplot as plt
import scipy.signal as scpy
import argparse

from matplotlib.animation import FuncAnimation, PillowWriter
from matplotlib.widgets import Button

from functions import *
from plotting_functions import *
from julia_functions import *

def main():
    parser = argparse.ArgumentParser(description="Run the acoustic signal processing simulation.")
    parser.add_argument("--config", type=str, default="simulation_config.json", help="Path to the simulation configuration JSON file.")
    parser.add_argument("--tdoa_method", type=str, default="envelope_envelope", help="Time Difference of Arrival method to use.")
    args = parser.parse_args()

    TDOA_method = args.tdoa_method

    # ==== Load Simulation Configuration ====
    config = load_simulation_config_json(args.config)
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
        ADC_out[i] = adc_oversampling(signal, oversampling_factor)

    print("All hydrophone signals oversampled.")

    # initialize digital signal processing parameters
    pinger_frequency = 30000  # 30 kHz
    sampling_rate = 1000000  # 1 MHz
    effective_sampling_rate = sampling_rate / oversampling_factor

    # Estimate minimum detection area size based on pinger frequency and speed of sound
    largest_distance_from_reference = max(np.linalg.norm(np.array(h) - np.array(hydro_pos[0])) for h in hydro_pos[1:])
    detection_area_radius = (largest_distance_from_reference / c) * effective_sampling_rate
    detection_area_diameter = int(np.ceil(detection_area_radius)) * 2
    print(f"Estimated Minimum Detection Area Diameter: {detection_area_diameter} samples")

    block_size = int(np.exp2(np.ceil(np.log2(detection_area_diameter))))
    print(f"Block Size Set To: {block_size} samples")

    head_room_blocks = 2 # extra blocks to ensure working block is always valid. SHOUDL ALLWAYS BE >= 2 to ensure enough past data.
    block_number = 3 + head_room_blocks # total number of blocks in ring buffers. 3 blocks for working + future and past data, rest for head room.
    buffer_size = block_size * block_number # total buffer size per hydrophone
    working_space_size = block_size * 3  # size of working space (3 blocks)
    print(f"Total Buffer Size Per Hydrophone: {buffer_size} samples")
    warmup_samples = block_size*head_room_blocks  # wait until first working block is fully populated

    def buffer_add_sample(signal, buffer, index):
        buffer[index % len(buffer)] = signal[index]
        return buffer

    def buffer_straigten(buffer, index):
        return np.concatenate((buffer[index % len(buffer):], buffer[:index % len(buffer)]))

    def working_block(buffer, new_data_index, block_size):
        new_data_block = new_data_index // block_size
        working_block = (new_data_block + head_room_blocks + 1) % (block_number)
        start_index = working_block * block_size
        return buffer[start_index:(start_index + block_size)]

    def working_space(buffer, new_data_index, block_size):
        new_data_block = new_data_index // block_size
        working_block = (new_data_block + head_room_blocks) % (block_number)
        start_index = working_block * block_size
        end_index = ((working_block + 3) * block_size) % (len(buffer)+1)

        if end_index > start_index:
            return buffer[start_index:end_index]
        else:
            return np.concatenate((buffer[start_index:], buffer[:end_index + 1]))

    buffers = np.zeros((5, buffer_size))

    max_allowed_detection_execution_time = block_size / (effective_sampling_rate/1000000) # in microseconds
    print(f"Max Detection Execution Time Per Frame: {max_allowed_detection_execution_time:.0f} µs")

    for i in range(5):
        hydro_pos[i] = np.array(hydro_pos[i]) - np.array(drone_pos)  # Adjust for drone position

    pinger_direction = np.array(pinger_pos) - hydro_pos[0]
    pinger_direction = pinger_direction / np.linalg.norm(pinger_direction)

    if __name__ == "__main__":
        # ==== Animation Configuration ====
        animation_length = 30  # seconds
        desired_fps = 24  # Desired frames per second for the animation
        animation_interval = 1000 // desired_fps  # Milliseconds between frames
        frame_skip = block_size #ADC_out[0].shape[0] // (animation_length * desired_fps)  # Capture every N iterations as a frame
        
        frame_number = max(0, (ADC_out[0].shape[0] - warmup_samples) // frame_skip)
        print(f"Animation will capture every {frame_skip} iterations.")

    
        # Collect buffer states outputs for animation
        buffer_frames = []
        noise_power_frames = []
        signal_power_frames = []
        SNR_frames = []
        FFT_signal_frames = []
        FFT_noise_frames = []
        FFT_freqs_frames = []
        hilbert_envelope_frames = []
        hilbert_envelope_envelope_frames = []
        working_space_frames = []
        buffer_envelope_frames = []
        working_space_envelope_frames = []
        reference_buffer_frames = []
        detected_index_frames = []

        signal_power_buffer = np.zeros(frame_number)
        noise_buffer = np.zeros(frame_number)
        SNR_buffer = np.zeros(frame_number)

    print("Running simulation and collecting buffer states...")
    pinger_found = False

    for i in range(ADC_out[0].shape[0]):
        for j in range(ADC_out.shape[0]):
            buffers[j] = buffer_add_sample(ADC_out[j], buffers[j], i)

        buffer_is_full = i >= warmup_samples
        # Capture only when the ring buffer is full, and align to the end of each block.
        if buffer_is_full and (i % block_size == 0):

            reference_buffer = working_block(buffers[0], i, block_size)

            buffer_fft = np.fft.fft(reference_buffer)
            fft_freqs = np.fft.fftfreq(len(reference_buffer), d=1/effective_sampling_rate)

            q = np.abs(np.abs(fft_freqs) - pinger_frequency) <= effective_sampling_rate / block_size * 3 # 7-bin width
            F = np.zeros(buffer_fft.size)
            F[q] = 1.

            buffer_signal_fft = buffer_fft * F
            buffer_noise_fft = buffer_fft * (1 - F)

            fft_plot_freqs = np.fft.fftshift(fft_freqs)[fft_freqs < 0]

            noise_power = signal_fft_power(buffer_noise_fft) / block_size
            pinger_power = signal_fft_power(buffer_signal_fft) / block_size

            SNR = pinger_power / (noise_power) if noise_power > 0 else 0

            print(f"Iteration {i}: Pinger Power = {pinger_power:.2f}, Noise Power = {noise_power:.2f}, SNR = {compute_snr_db(pinger_power, noise_power):.2f} dB")

            # ==== Hilbert Shenanigans ====
            reference_working_space = working_space(buffers[0], i, block_size)
            reference_analytic_signal = scpy.hilbert(reference_working_space)
            reference_envelope = np.abs(reference_analytic_signal)
            reference_envelope_analytic_signal = scpy.hilbert(reference_envelope)
            reference_envelope_envelope = reference_envelope_analytic_signal.imag

            detected_indices = [None] * 5
            if SNR > 1.0:
                detected_indices[0] = int(np.argmin(reference_envelope_envelope))
                print(f"Hydrophone 1: Detected Index = {detected_indices[0]}")
                for k in range(1, 5):
                    working_space_k = working_space(buffers[k], i, block_size)
                    analytic_signal_k = scpy.hilbert(working_space_k)
                    envelope_k = np.abs(analytic_signal_k)
                    envelope_analytic_signal_k = scpy.hilbert(envelope_k)
                    envelope_envelope_k = envelope_analytic_signal_k.imag

                    if TDOA_method == "correlation":
                        correlation = scpy.correlate(working_space_k, reference_working_space, mode='full')
                        lags = scpy.correlation_lags(len(working_space_k), len(reference_working_space), mode='full')
                        detected_index = lags[np.argmax(correlation)]
                        detected_indices[k] = int(detected_index + detected_indices[0])

                    elif TDOA_method == "envelope_correlation":
                        correlation = scpy.correlate(envelope_k, reference_envelope, mode='full')
                        lags = scpy.correlation_lags(len(envelope_k), len(reference_envelope), mode='full')
                        detected_index = lags[np.argmax(correlation)]
                        detected_indices[k] = int(detected_index + detected_indices[0])

                    elif TDOA_method == "envelope_envelope_correlation":
                        correlation = scpy.correlate(envelope_envelope_k, reference_envelope_envelope, mode='full')
                        lags = scpy.correlation_lags(len(envelope_envelope_k), len(reference_envelope_envelope), mode='full')
                        detected_index = lags[np.argmax(correlation)]
                        detected_indices[k] = int(detected_index + detected_indices[0])
                    else:  # default to envelope_envelope method
                        detected_indices[k] = int(np.argmin(envelope_envelope_k)) + (detected_indices[0])
                    
                    print(f"Hydrophone {k+1}: Detected Index = {detected_indices[k]}")

                times_of_arrival = np.array(detected_indices) / effective_sampling_rate
                # Solve for pinger position using TDOA
                estimated_position = TDOA_pos_solve(hydro_pos, times_of_arrival - times_of_arrival[0], c)
                print(f"Estimated Pinger Position: {estimated_position}")
                print(f"Actual Pinger Position: {pinger_pos}")

                position_error = np.arccos(np.dot(estimated_position, pinger_direction)/np.dot(pinger_direction, pinger_direction)) * (180.0 / np.pi)
                print(f"Direction Error from Estimated position: {position_error}")

                estimated_direction = TDOA_direction_solve(hydro_pos, times_of_arrival - times_of_arrival[0], c)
                print(f"Estimated Pinger Direction: {estimated_direction}")
                print(f"Actual Pinger Direction: {pinger_direction}")

                direction_error = np.arccos(np.clip(np.dot(estimated_direction, pinger_direction), -1.0, 1.0)) * (180.0 / np.pi)
                print(f"Direction Error: {direction_error} degrees")


                pinger_found = True

                

            if __name__ == "__main__":
                # ==== Store frame data ====
                buffers_frame = []
                for buffer in buffers:
                    buffers_frame.append(working_block(buffer, i, block_size))

                buffer_frames.append(np.array(buffers_frame).copy())
                buffer_envelope_frames.append(np.array([np.abs(scpy.hilbert(b)) for b in buffers_frame]).copy())
                reference_buffer_frames.append(reference_buffer.copy())
                hilbert_envelope_frames.append(reference_envelope.copy())
                hilbert_envelope_envelope_frames.append(reference_envelope_envelope.copy())
                detected_index_frames.append(detected_indices)

                working_space_frame = []
                for buffer in buffers:
                    working_space_frame.append(working_space(buffer, i, block_size))
                working_space_frames.append(np.array(working_space_frame).copy())
                working_space_envelope_frames.append(
                    np.array([np.abs(scpy.hilbert(w)) for w in working_space_frame]).copy()
                )

                signal_power_buffer[:-1] = signal_power_buffer[1:]
                signal_power_buffer[-1] = pinger_power
                signal_power_frames.append(signal_power_buffer.copy())

                noise_buffer[:-1] = noise_buffer[1:]
                noise_buffer[-1] = noise_power
                noise_power_frames.append(noise_buffer.copy())
                
                SNR_buffer[:-1] = SNR_buffer[1:]
                SNR_buffer[-1] = compute_snr_db(pinger_power, noise_power)
                SNR_frames.append(SNR_buffer.copy())

                buffer_plot_signal_fft = np.abs(np.fft.fftshift(buffer_signal_fft)) / block_size
                buffer_plot_signal_fft = buffer_plot_signal_fft[fft_freqs < 0]

                buffer_plot_noise_fft = np.abs(np.fft.fftshift(buffer_noise_fft)) / block_size
                buffer_plot_noise_fft = buffer_plot_noise_fft[fft_freqs < 0]

                FFT_signal_frames.append(buffer_plot_signal_fft.copy())
                FFT_noise_frames.append(buffer_plot_noise_fft.copy())
                FFT_freqs_frames.append(fft_plot_freqs.copy())

        if pinger_found:
            break

    print(f"Simulation complete")
    if __name__ == "__main__":
        print(f"Captured {len(buffer_frames)} frames. Starting animation...")

        # Compute signal range for proper axis scaling
        min_val = min(np.min(adc) for adc in ADC_out)
        max_val = max(np.max(adc) for adc in ADC_out)

        noise_power_min = min(np.min(noise) for noise in noise_power_frames)
        noise_power_max = max(np.max(noise) for noise in noise_power_frames)

        signal_power_min = min(np.min(signal) for signal in signal_power_frames)
        signal_power_max = max(np.max(signal) for signal in signal_power_frames)

        SNR_min = min(np.min(SNR) for SNR in SNR_frames)
        SNR_max = max(np.max(SNR) for SNR in SNR_frames)

        FFT_min = min(np.min(FFT) for FFT in FFT_signal_frames)
        FFT_max = max(np.max(FFT) for FFT in FFT_signal_frames)

        # Ensure the FFT plot y-limits include both signal and noise traces.
        if len(FFT_noise_frames) > 0:
            FFT_min = min(FFT_min, min(np.min(FFT) for FFT in FFT_noise_frames))
            FFT_max = max(FFT_max, max(np.max(FFT) for FFT in FFT_noise_frames))

        hilbert_envelope_min = min(np.min(env) for env in hilbert_envelope_frames)
        hilbert_envelope_max = max(np.max(env) for env in hilbert_envelope_frames)

        hilbert_envelope_envelope_min = min(np.min(env) for env in hilbert_envelope_envelope_frames)
        hilbert_envelope_envelope_max = max(np.max(env) for env in hilbert_envelope_envelope_frames)

        buffer_envelope_min = min(np.min(env) for env in buffer_envelope_frames)
        buffer_envelope_max = max(np.max(env) for env in buffer_envelope_frames)

        working_space_envelope_min = min(np.min(env) for env in working_space_envelope_frames)
        working_space_envelope_max = max(np.max(env) for env in working_space_envelope_frames)

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

        # Extra overlay line for FFT noise (axis 4). Hidden in buffer view.
        fft_noise_line, = axes[1].plot([], [], color='tab:orange', alpha=0.75, linewidth=1.0)
        fft_noise_line.set_visible(False)

        # Detection marker (used in matched-filter view)
        det_vline = axes[1].axvline(0, color='r', linewidth=1, alpha=0.9)
        det_vline.set_visible(False)

        det_vlines = [ax.axvline(0, color='r', linewidth=1, alpha=0.6) for ax in axes]
        for vline in det_vlines:
            vline.set_visible(False)

        skip_n = 1  # Number of frames to skip on skip button press

        # Animation state
        animation_state = {
            'paused': False,
            'current_frame': 0,
            'show_detection_algorithm': False,
            'show_hilbert': False,
            'buffer_show_envelope': False,
        }

        def animate(frame_num):
            if not animation_state['paused']:
                animation_state['current_frame'] = frame_num
            else:
                frame_num = animation_state['current_frame']
            
            if animation_state['show_hilbert']:
                lines[0].set_ydata(reference_buffer_frames[frame_num])
                axes[0].set_title('Hydrophone 1 - Reference Buffer Output')
                axes[0].set_ylim(min_val, max_val)

                fft_noise_line.set_visible(False)
                det_vline.set_visible(False)
                lines[1].set_xdata(np.arange(working_space_size))
                lines[1].set_ydata(hilbert_envelope_frames[frame_num])
                axes[1].set_title('Hydrophone 1 - Hilbert Envelope')
                axes[1].set_xlim(0, working_space_size)
                axes[1].set_ylim(hilbert_envelope_min, hilbert_envelope_max)

                lines[2].set_xdata(np.arange(working_space_size))
                lines[2].set_ydata(hilbert_envelope_envelope_frames[frame_num])
                axes[2].set_title('Hydrophone 1 - Envelope Envelope Output')
                axes[2].set_ylim(hilbert_envelope_envelope_min, hilbert_envelope_envelope_max)
                axes[2].set_xlim(0, working_space_size)

                lines[3].set_xdata(np.arange(frame_number))
                lines[3].set_ydata(noise_power_frames[frame_num])
                axes[3].set_title('Hydrophone 1 - Noise Power Output')
                axes[3].set_ylim(noise_power_min, noise_power_max)
                axes[3].set_xlim(0, frame_number)

                lines[4].set_xdata(np.arange(frame_number))
                lines[4].set_ydata(SNR_frames[frame_num])
                axes[4].set_title('Hydrophone 1 - SNR Output')
                axes[4].set_ylim(SNR_min, SNR_max)
                axes[4].set_xlim(0, frame_number)

            elif animation_state['show_detection_algorithm']:
                # Display only matched filter output for reference hydrophone
                lines[0].set_ydata(reference_buffer_frames[frame_num])
                axes[0].set_title('Hydrophone 1 - Straigt Buffer Output')
                axes[0].set_ylim(min_val, max_val)

                lines[1].set_xdata(FFT_freqs_frames[frame_num])
                lines[1].set_ydata(FFT_signal_frames[frame_num])
                fft_noise_line.set_xdata(FFT_freqs_frames[frame_num])
                fft_noise_line.set_ydata(FFT_noise_frames[frame_num])
                fft_noise_line.set_visible(True)
                axes[1].set_title('Hydrophone 1 - FFT Output')
                axes[1].set_xlim(FFT_freqs_frames[frame_num][0], FFT_freqs_frames[frame_num][-1])
                axes[1].set_ylim(FFT_min, FFT_max)

                lines[2].set_xdata(np.arange(frame_number))
                lines[2].set_ydata(signal_power_frames[frame_num])
                axes[2].set_title('Hydrophone 1 - Signal Power Output')
                axes[2].set_ylim(signal_power_min, signal_power_max)
                axes[2].set_xlim(0, frame_number)


                lines[3].set_xdata(np.arange(frame_number))
                lines[3].set_ydata(noise_power_frames[frame_num])
                axes[3].set_title('Hydrophone 1 - Noise Power Output')
                axes[3].set_ylim(noise_power_min, noise_power_max)
                axes[3].set_xlim(0, frame_number)

                lines[4].set_xdata(np.arange(frame_number))
                lines[4].set_ydata(SNR_frames[frame_num])
                axes[4].set_title('Hydrophone 1 - SNR Output')
                axes[4].set_ylim(SNR_min, SNR_max)
                axes[4].set_xlim(0, frame_number)

            else:
                # Display buffer data for all hydrophones
                det_vline.set_visible(False)
                fft_noise_line.set_visible(False)
                for vline in det_vlines:
                    vline.set_visible(False)

                show_workspace = (
                    pinger_found and
                    frame_num == len(buffer_frames) - 1 and
                    frame_num < len(working_space_frames)
                )
                if show_workspace:
                    workspace_len = working_space_frames[frame_num].shape[1]
                    env_min = working_space_envelope_min
                    env_max = working_space_envelope_max
                else:
                    workspace_len = block_size
                    env_min = buffer_envelope_min
                    env_max = buffer_envelope_max
                for j in range(5):
                    lines[j].set_xdata(np.arange(workspace_len))
                    if show_workspace:
                        if animation_state['buffer_show_envelope']:
                            lines[j].set_ydata(working_space_envelope_frames[frame_num][j])
                        else:
                            lines[j].set_ydata(working_space_frames[frame_num][j])
                    else:
                        if animation_state['buffer_show_envelope']:
                            lines[j].set_ydata(buffer_envelope_frames[frame_num][j])
                        else:
                            lines[j].set_ydata(buffer_frames[frame_num][j])

                    if animation_state['buffer_show_envelope']:
                        axes[j].set_title(f'Hydrophone {j+1} Envelope')
                        axes[j].set_ylim(env_min, env_max)
                    else:
                        axes[j].set_title(f'Hydrophone {j+1} Buffer')
                        axes[j].set_ylim(min_val, max_val)

                    axes[j].set_visible(True)
                    axes[j].set_xlim(0, workspace_len)

                    if show_workspace:
                        det_idx = detected_index_frames[frame_num][j]
                        if det_idx is not None and det_idx >= 0:
                            det_vlines[j].set_xdata([det_idx, det_idx])
                            det_vlines[j].set_visible(True)
            
            status = 'PAUSED' if animation_state['paused'] else 'PLAYING'
            if animation_state['show_hilbert']:
                view_mode = 'Hilbert Envelope'
            elif animation_state['show_detection_algorithm']:
                view_mode = 'Detection Algorithm'
            else:
                view_mode = 'Buffers'
            fig.suptitle(
                f'{view_mode} - Frame {frame_num}/{len(buffer_frames)-1} '
                f'(Iteration {warmup_samples + (frame_num + 1) * frame_skip - 1}) [{status}]'
            )
            return lines + [fft_noise_line]

        ani = FuncAnimation(fig, animate, frames=len(buffer_frames), interval=animation_interval, repeat=True)

        # Create control buttons
        ax_play = plt.axes([0.2, 0.02, 0.08, 0.04])
        ax_skip_back = plt.axes([0.1, 0.02, 0.08, 0.04])
        ax_skip_fwd = plt.axes([0.3, 0.02, 0.08, 0.04])
        ax_buffer = plt.axes([0.40, 0.02, 0.10, 0.04])
        ax_env = plt.axes([0.51, 0.02, 0.12, 0.04])
        ax_detect = plt.axes([0.64, 0.02, 0.12, 0.04])
        ax_hilbert = plt.axes([0.77, 0.02, 0.12, 0.04])
        ax_save = plt.axes([0.90, 0.02, 0.08, 0.04])

        btn_play = Button(ax_play, 'Pause/Play')
        btn_skip_back = Button(ax_skip_back, 'Skip Back')
        btn_skip_fwd = Button(ax_skip_fwd, 'Skip Fwd')
        btn_buffer = Button(ax_buffer, 'Buffer View')
        btn_env = Button(ax_env, 'Buffer Envelope')
        btn_detect = Button(ax_detect, 'Detection View')
        btn_hilbert = Button(ax_hilbert, 'Hilbert View')
        btn_save = Button(ax_save, 'Save GIF')

        def on_play(event):
            animation_state['paused'] = not animation_state['paused']

        def on_skip_back(event):
            animation_state['current_frame'] = max(0, animation_state['current_frame'] - skip_n)
            animation_state['paused'] = True

        def on_skip_fwd(event):
            animation_state['current_frame'] = min(len(buffer_frames) - 1, animation_state['current_frame'] + skip_n)
            animation_state['paused'] = True

        def on_buffer_view(event):
            animation_state['show_detection_algorithm'] = False
            animation_state['show_hilbert'] = False
            animation_state['paused'] = True

        def on_buffer_envelope(event):
            animation_state['buffer_show_envelope'] = not animation_state['buffer_show_envelope']
            animation_state['show_detection_algorithm'] = False
            animation_state['show_hilbert'] = False
            animation_state['paused'] = True

        def on_detection_view(event):
            animation_state['show_detection_algorithm'] = True
            animation_state['show_hilbert'] = False
            animation_state['buffer_show_envelope'] = False
            animation_state['paused'] = True

        def on_hilbert(event):
            animation_state['show_hilbert'] = not animation_state['show_hilbert']
            if animation_state['show_hilbert']:
                animation_state['show_detection_algorithm'] = False
                animation_state['buffer_show_envelope'] = False
            animation_state['paused'] = True

        def on_save(event):
            print("Saving animation as GIF...")
            writer = PillowWriter(fps=desired_fps)
            ani.save('buffer_animation.gif', writer=writer)
            print("Animation saved as 'buffer_animation.gif'")

        btn_play.on_clicked(on_play)
        btn_skip_back.on_clicked(on_skip_back)
        btn_skip_fwd.on_clicked(on_skip_fwd)
        btn_buffer.on_clicked(on_buffer_view)
        btn_env.on_clicked(on_buffer_envelope)
        btn_detect.on_clicked(on_detection_view)
        btn_hilbert.on_clicked(on_hilbert)
        btn_save.on_clicked(on_save)

        plt.subplots_adjust(bottom=0.12)
        plt.show()

    return estimated_position, position_error, estimated_direction, direction_error

if __name__ == "__main__":
    main()