from __future__ import annotations

from dataclasses import dataclass
from typing import Any

import numpy as np
import scipy.signal as scpy

from functions import (
    TDOA_direction_solve,
    TDOA_pos_solve,
    adc_oversampling,
    compute_snr_db,
    signal_fft_power,
)

from julia_functions import (
    load_all_hydrophone_data,
    load_simulation_config_json,
)

from fullstack_gui import CaptureMeta, FrameStore


def _working_block(buffer: np.ndarray, new_data_index: int, *, block_size: int, block_number: int, head_room_blocks: int) -> np.ndarray:
    new_data_block = new_data_index // block_size
    working_block = (new_data_block + head_room_blocks + 1) % block_number
    start_index = working_block * block_size
    return buffer[start_index : (start_index + block_size)]


def _working_space(buffer: np.ndarray, new_data_index: int, *, block_size: int, block_number: int, head_room_blocks: int) -> np.ndarray:
    """Return a fixed-length 3-block window (always exactly 3*block_size samples).

    The previous implementation used modulo len(buffer)+1 and could produce off-by-one lengths,
    which then breaks Matplotlib when switching views.
    """

    new_data_block = new_data_index // block_size
    working_block = (new_data_block + head_room_blocks) % block_number
    start = working_block * block_size
    total = block_size * 3
    idx = (start + np.arange(total)) % buffer.shape[0]
    return buffer[idx]


def run_capture(
    *,
    config_path: str,
    tdoa_method: str,
    verbose: bool = True,
    hydrophone_data_path: str | None = "hydrophones_data.csv",
    inject_faulty_detection: bool = False,
) -> tuple[FrameStore, tuple[Any, float, Any, float]]:
    config = load_simulation_config_json(config_path)
    hydro_pos = config["hydrophones_pos"]
    drone_pos = config["drone_pos"]
    pinger_pos = config["pinger_pos"]

    c = 1538.9235842

    signal_data = load_all_hydrophone_data(combined_path=hydrophone_data_path) if hydrophone_data_path else load_all_hydrophone_data()

    ADC_out = np.zeros((5,), dtype=object)
    oversampling_factor = 8
    for i, (_time, signal) in enumerate(signal_data):
        ADC_out[i] = adc_oversampling(signal, oversampling_factor)

    pinger_frequency = 30000
    sampling_rate = 1000000
    effective_sampling_rate = sampling_rate / oversampling_factor

    largest_distance_from_reference = max(
        np.linalg.norm(np.array(h) - np.array(hydro_pos[0])) for h in hydro_pos[1:]
    )
    detection_area_radius = (largest_distance_from_reference / c) * effective_sampling_rate
    detection_area_diameter = int(np.ceil(detection_area_radius)) * 2

    block_size = int(np.exp2(np.ceil(np.log2(detection_area_diameter))))

    head_room_blocks = 2
    block_number = 3 + head_room_blocks
    buffer_size = block_size * block_number
    working_space_size = block_size * 3
    warmup_samples = block_size * head_room_blocks

    if verbose:
        print("Loaded simulation configuration.")
        print(f"Block Size Set To: {block_size} samples")
        print(f"Total Buffer Size Per Hydrophone: {buffer_size} samples")

    # Positions relative to drone
    for i in range(5):
        hydro_pos[i] = np.array(hydro_pos[i]) - np.array(drone_pos)

    pinger_direction = np.array(pinger_pos) - hydro_pos[0]
    pinger_direction = pinger_direction / np.linalg.norm(pinger_direction)

    # Animation capture settings (kept consistent with your prior behavior)
    frame_skip = block_size
    frame_number = max(0, (ADC_out[0].shape[0] - warmup_samples) // frame_skip)

    buffers = np.zeros((5, buffer_size))

    def buffer_add_sample(signal: np.ndarray, buffer: np.ndarray, index: int) -> np.ndarray:
        buffer[index % buffer.shape[0]] = signal[index]
        return buffer

    # Frame capture lists
    buffer_frames: list[np.ndarray] = []
    buffer_envelope_frames: list[np.ndarray] = []
    working_space_frames: list[np.ndarray] = []
    working_space_envelope_frames: list[np.ndarray] = []
    working_space_envelope_edge_frames: list[np.ndarray] = []

    reference_buffer_frames: list[np.ndarray] = []
    hilbert_envelope_frames: list[np.ndarray] = []
    hilbert_envelope_edge_frames: list[np.ndarray] = []

    noise_power_frames: list[np.ndarray] = []
    signal_power_frames: list[np.ndarray] = []
    SNR_frames: list[np.ndarray] = []

    FFT_signal_frames: list[np.ndarray] = []
    FFT_noise_frames: list[np.ndarray] = []
    FFT_freqs_frames: list[np.ndarray] = []

    signal_power_buffer = np.zeros(frame_number)
    noise_buffer = np.zeros(frame_number)
    SNR_buffer = np.zeros(frame_number)

    pinger_found = False

    estimated_position = None
    position_error = float("inf")
    estimated_direction = None
    direction_error = float("inf")

    if verbose:
        print("Running simulation and collecting buffer states...")

    detected_indices_history = []

    for i in range(ADC_out[0].shape[0]):
        for j in range(ADC_out.shape[0]):
            buffers[j] = buffer_add_sample(ADC_out[j], buffers[j], i)

        buffer_is_full = i >= warmup_samples
        if buffer_is_full and (i % block_size == 0):
            reference_buffer = _working_block(
                buffers[0], i, block_size=block_size, block_number=block_number, head_room_blocks=head_room_blocks
            )

            buffer_fft = np.fft.fft(reference_buffer)
            fft_freqs = np.fft.fftfreq(len(reference_buffer), d=1 / effective_sampling_rate)

            q = np.abs(np.abs(fft_freqs) - pinger_frequency) <= effective_sampling_rate / block_size * 3
            F = np.zeros(buffer_fft.size)
            F[q] = 1.0

            buffer_signal_fft = buffer_fft * F
            buffer_noise_fft = buffer_fft * (1.0 - F)

            fft_plot_freqs = np.fft.fftshift(fft_freqs)[fft_freqs < 0]

            noise_power = signal_fft_power(buffer_noise_fft) / block_size
            pinger_power = signal_fft_power(buffer_signal_fft) / block_size

            SNR = pinger_power / noise_power if noise_power > 0 else 0

            reference_working_space = _working_space(
                buffers[0], i, block_size=block_size, block_number=block_number, head_room_blocks=head_room_blocks
            )
            reference_analytic_signal = scpy.hilbert(reference_working_space)
            reference_envelope = np.abs(reference_analytic_signal)
            reference_envelope_analytic_signal = scpy.hilbert(reference_envelope)
            reference_envelope_edge = reference_envelope_analytic_signal.imag

            detected_indices = np.full((5,), -1, dtype=int)

            if SNR > 1.0:
                detected_indices[0] = int(np.argmin(reference_envelope_edge))
                
                if inject_faulty_detection:
                    bad_one = np.random.randint(0, 5)
                else:
                    bad_one = -1
                if bad_one >= 0:
                    print(f"Intentionally corrupting hydrophone {bad_one} detection for testing.")
                if bad_one == 0:
                    offset = np.random.randint(-50, 50)
                    detected_indices[0] += offset
                    print(f"Corrupted by: {offset} indexes.")

                for k in range(1, 5):
                    ws_k = _working_space(
                        buffers[k], i, block_size=block_size, block_number=block_number, head_room_blocks=head_room_blocks
                    )
                    analytic_signal_k = scpy.hilbert(ws_k)
                    envelope_k = np.abs(analytic_signal_k)
                    envelope_analytic_signal_k = scpy.hilbert(envelope_k)
                    envelope_edge_k = envelope_analytic_signal_k.imag

                    if tdoa_method == "correlation":
                        correlation = scpy.correlate(ws_k, reference_working_space, mode="full")
                        lags = scpy.correlation_lags(len(ws_k), len(reference_working_space), mode="full")
                        detected_index = int(lags[np.argmax(correlation)])
                        detected_indices[k] = detected_index + detected_indices[0]

                    elif tdoa_method == "envelope_correlation":
                        correlation = scpy.correlate(envelope_k, reference_envelope, mode="full")
                        lags = scpy.correlation_lags(len(envelope_k), len(reference_envelope), mode="full")
                        detected_index = int(lags[np.argmax(correlation)])
                        detected_indices[k] = detected_index + detected_indices[0]

                    elif tdoa_method == "envelope_edge_correlation":
                        correlation = scpy.correlate(envelope_edge_k, reference_envelope_edge, mode="full")
                        lags = scpy.correlation_lags(
                            len(envelope_edge_k), len(reference_envelope_edge), mode="full"
                        )
                        detected_index = int(lags[np.argmax(correlation)])
                        detected_indices[k] = detected_index + detected_indices[0]

                    else:
                        detected_indices[k] = int(np.argmin(envelope_edge_k))
                        if k == bad_one:
                            offset = np.random.randint(-50, 50)
                            detected_indices[k] += offset
                            print(f"Corrupted by: {offset} indexes.")

                times_of_arrival = detected_indices.astype(float) / effective_sampling_rate
                estimated_position = TDOA_pos_solve(hydro_pos, times_of_arrival - times_of_arrival[0], c)
                estimated_position_normalized = estimated_position / np.linalg.norm(estimated_position)

                pos_error_cos = float(np.dot(estimated_position_normalized, pinger_direction))
                ortho_direction = estimated_position_normalized - pos_error_cos*pinger_direction
                ortho_direction = ortho_direction / np.linalg.norm(ortho_direction)
                pos_error_sin = float(np.dot(estimated_position_normalized,   ortho_direction))
                position_error = float(np.degrees(np.atan2(pos_error_sin, pos_error_cos)))

                estimated_direction = TDOA_direction_solve(hydro_pos, times_of_arrival - times_of_arrival[0], c)
                estimated_direction = estimated_direction / np.linalg.norm(estimated_direction)

                dir_error_cos = float(np.dot(estimated_direction, pinger_direction))
                ortho_direction = estimated_direction - dir_error_cos*pinger_direction
                ortho_direction = ortho_direction / np.linalg.norm(ortho_direction)
                dir_error_sin = float(np.dot(estimated_direction,   ortho_direction))
                direction_error = float(np.degrees(np.atan2(dir_error_sin, dir_error_cos)))

                if verbose:
                    print("")
                    print(
                        f"Frame {i // block_size}: "
                        f"Pos Error: {position_error:.2f} deg, "
                        f"Dir Error: {direction_error:.2f} deg, "
                        f"SNR: {10*np.log10(SNR):.2f} dB"
                    )
                    print(f"    Real Position:      {pinger_pos}")
                    print(f"    Estimated Position: {estimated_position}")
                    print(f"    Real Direction:     {pinger_direction}")
                    print(f"    Estimated Direction:{estimated_direction}")

                pinger_found = True

            # ==== Store frame data ====
            buffers_frame = np.stack(
                [
                    _working_block(
                        b, i, block_size=block_size, block_number=block_number, head_room_blocks=head_room_blocks
                    )
                    for b in buffers
                ],
                axis=0,
            )
            buffer_frames.append(buffers_frame.copy())
            buffer_envelope_frames.append(np.abs(scpy.hilbert(buffers_frame, axis=1)).copy())

            reference_buffer_frames.append(reference_buffer.copy())
            hilbert_envelope_frames.append(reference_envelope.copy())
            hilbert_envelope_edge_frames.append(reference_envelope_edge.copy())
            detected_indices_history.append(detected_indices.copy())

            ws_frame = np.stack(
                [
                    _working_space(
                        b, i, block_size=block_size, block_number=block_number, head_room_blocks=head_room_blocks
                    )
                    for b in buffers
                ],
                axis=0,
            )
            working_space_frames.append(ws_frame.copy())
            working_space_envelope_frames.append(np.abs(scpy.hilbert(ws_frame, axis=1)).copy())
            working_space_envelope_edge_frames.append(scpy.hilbert(np.abs(scpy.hilbert(ws_frame, axis=1)), axis=1).imag.copy())

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


    # Convert to arrays
    arrays: dict[str, np.ndarray] = {
        "buffer_frames": np.stack(buffer_frames, axis=0),
        "buffer_envelope_frames": np.stack(buffer_envelope_frames, axis=0),
        "working_space_frames": np.stack(working_space_frames, axis=0),
        "working_space_envelope_frames": np.stack(working_space_envelope_frames, axis=0),
        "reference_buffer_frames": np.stack(reference_buffer_frames, axis=0),
        "hilbert_envelope_frames": np.stack(hilbert_envelope_frames, axis=0),
        "hilbert_envelope_edge_frames": np.stack(hilbert_envelope_edge_frames, axis=0),
        "working_space_envelope_edge_frames": np.stack(working_space_envelope_edge_frames, axis=0),
        "noise_power_frames": np.stack(noise_power_frames, axis=0),
        "signal_power_frames": np.stack(signal_power_frames, axis=0),
        "SNR_frames": np.stack(SNR_frames, axis=0),
        "FFT_signal_frames": np.stack(FFT_signal_frames, axis=0),
        "FFT_noise_frames": np.stack(FFT_noise_frames, axis=0),
        "FFT_freqs_frames": np.stack(FFT_freqs_frames, axis=0),
        "detected_indices_frames": np.stack(detected_indices_history, axis=0),
    }

    # Save the final state of the raw working space to a CSV file
    # Shape: (5, working_space_size)
    final_working_space = working_space_frames[-1]  # shape (5, N)
    import csv
    csv_path = "final_workingspace_state.csv"
    with open(csv_path, "w", newline="") as csvfile:
        writer = csv.writer(csvfile)
        # Write header
        header = ["index"] + [f"hydrophone_{i}" for i in range(final_working_space.shape[0])]
        writer.writerow(header)
        # Write each hydrophone as a row
        for row in final_working_space:
            writer.writerow(row)

    adc_min = float(min(np.min(adc) for adc in ADC_out))
    adc_max = float(max(np.max(adc) for adc in ADC_out))

    meta = CaptureMeta(
        block_size=block_size,
        working_space_size=working_space_size,
        frame_skip=frame_skip,
        warmup_samples=warmup_samples,
        effective_sampling_rate=effective_sampling_rate,
        pinger_frequency=pinger_frequency,
        frame_number=frame_number,
        pinger_found=pinger_found,
    )

    store = FrameStore(
        meta=meta,
        arrays=arrays,
        scalars={
            "adc_min": adc_min,
            "adc_max": adc_max,
        },
    )

    return store, (estimated_position, position_error, estimated_direction, direction_error)
