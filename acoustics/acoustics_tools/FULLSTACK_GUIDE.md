## Fullstack Acoustics Tools Guide

Welcome to the Fullstack Acoustics Tools! This guide will help you understand the workflow, data structures, and how to use the simulation, capture, and visualization tools in this repository.

---

### 1. Overview

This project simulates, processes, and visualizes hydrophone array data for underwater acoustics experiments. It includes:

- **Simulation** (Julia): Generates synthetic hydrophone data based on a physical and electrical model.
- **Capture & Processing** (Python): Loads, processes, and analyzes the data, extracting features like envelopes, SNR, and TDOA.
- **Visualization** (Python GUI): Interactive GUI for exploring the processed data frame-by-frame.

---

### 2. Workflow

**A. Simulate Data (Julia)**
1. Edit `simulation_config.json` to set up your experiment (hydrophone positions, pinger, noise, etc).
2. Run `acoustic_data_simulator.jl` in Julia. This will generate `hydrophones_data.csv`.

**B. Run Fullstack Processing (Python)**
1. Run `fullstack_capture.py` to capture and process the data:
	```sh
	python fullstack_capture.py --config simulation_config.json --tdoa_method envelope_correlation
	```
	- This will process the data, estimate positions, and optionally launch the GUI.
	- Use `--export my_capture.npz` to save the processed frames for later viewing.

**C. View Results (Python GUI)**
1. To view a saved capture, run:
	```sh
	python fullstack_viewer.py my_capture.npz
	```
	- This opens the interactive GUI for exploring all frames and signal features.

---

### 3. FrameStore Data Structure

All processed data is stored in a `FrameStore` object, which contains arrays for each frame and hydrophone. Key arrays include:

#### Raw Signal Data
- `buffer_frames`: (n_frames, 5, block_size) — Working block for each hydrophone
- `working_space_frames`: (n_frames, 5, workspace_size) — 3-block workspace window

#### Pre-computed Features
- `buffer_envelope_frames`: (n_frames, 5, block_size) — Envelope (Hilbert magnitude) of working block
- `working_space_envelope_frames`: (n_frames, 5, workspace_size) — Envelope of workspace
- `working_space_envelope_edge_frames`: (n_frames, 5, workspace_size) — Envelope edge (imaginary part of Hilbert of envelope)

#### FFT and Power Analysis (Reference Hydrophone)
- `reference_buffer_frames`: (n_frames, block_size)
- `FFT_freqs_frames`: (n_frames, n_freqs)
- `FFT_signal_frames`: (n_frames, n_freqs)
- `FFT_noise_frames`: (n_frames, n_freqs)

#### Power Metrics
- `signal_power_frames`: (n_frames, frame_number)
- `noise_power_frames`: (n_frames, frame_number)
- `SNR_frames`: (n_frames, frame_number)

#### Metadata
- `meta.block_size`: Integer, number of samples per block
- `meta.effective_sampling_rate`: Float, Hz

---

### 4. GUI Usage

- Use the navigation buttons to play/pause, skip frames, and switch between different data views (time, frequency, envelope, edge, overlap, detection, etc).
- The GUI visualizes all hydrophones and key features for each frame.
- You can save the animation as a GIF from the GUI.

---

### 5. Debugging & Development Tips

- Each visualization panel should implement `plot()`, `activate()`, `deactivate()`, and `update()` methods.
- Precompute y-axis ranges in `__init__()` for smooth plotting.
- For bar plots, always clear the axis before redrawing in `update()`.
- Use the correct data arrays for each panel (workspace vs buffer).
- For 3D plots, create the axis in `update()` (not `plot()`).

---

### 6. Extending the System

- To add new features or panels, follow the structure in `fullstack_gui.py`.
- To add new signal processing methods, edit `functions.py` and update the capture pipeline.
- For new simulation scenarios, edit `simulation_config.json` and rerun the Julia simulator.

---

For more details, see the code comments and docstrings in each file.

