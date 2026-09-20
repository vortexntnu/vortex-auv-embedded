## Fullstack Acoustics Tools Guide

Welcome to the Fullstack Acoustics Tools! This guide will help you understand the workflow, data structures, and how to use the simulation, capture, and visualization tools in this repository.
Remember to check out the notes at the bottom here.

---

### 1. Overview

This project simulates, processes, and visualizes hydrophone array data for underwater acoustics experiments. It includes:

- **Simulation** (Julia): Generates synthetic hydrophone data based on a physical and electrical model.
- **Capture & Processing** (Python): Loads, processes, and analyzes the data, extracting features like envelopes, SNR, and TDOA.
- **Visualization** (Python GUI): Interactive GUI for exploring the processed data frame-by-frame.

---

### 2. Prerequisites

You need to have Julia and python installed. If you get errors for missing packages then download them. 
If you don't know how to donwload them, then google it.

---

### 2. Workflow

1. Edit `simulation_config.json` to set up your experiment (hydrophone positions, pinger, noise, etc).
1. Run `acoustic_data_simulator.jl` in Julia. This will generate `hydrophones_data.csv`.
3. Run `fullstack_prototype.py` to capture and process the data:
	```sh
	python fullstack_capture.py --config simulation_config.json --tdoa_method envelope_correlation
	```
	This will process the data, estimate positions, and launch the GUI.

---

### 3. FrameStore Data Structure

This shit not important, skip it.

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


### 5. MEGA TESTING

You can run scripts to generate a lot of simulated hydrophone data for varying configs and then test algorithms on them to test their effectiveness.
How to:
1. First configure the simulation_config_for_testing.json as a base
2. Do some stuff with test_data_generator.py to change how it will randomize the simulations
3. Run test_data_generator.py (i think)
4. Run the tests with pytest (I don't remember how exactly but you can figure this out)
5. Observe the results

If you notice most succeeding but with some few exceptions you can use run_failed_config_simple.py (or not simple you choose) to see the view the interesting test configs in the GUI


---

### 6. Important Notes

The sim is very nice for testing algorithms and general aproaches for the acoustics related tasks, however keep in mind that what is implemented in the sim is not (necessarily) implemented in the actual firmware of the acoustics hardware.

At the time of writing this guide it has been a long time since i used this sim so uh good luck using it :-).

If at any time you are curious about what a python file does then just try running it. The ones that don't have os.delete("C:\Windows\System32") should be safe.

If you wish to change or test an algorithm then it is done in fullstack_capture.py, that's where the magic happens.

---

For more details, see the code comments and docstrings in each file. (these are ofcourse either non-existant or written by chat and claude)