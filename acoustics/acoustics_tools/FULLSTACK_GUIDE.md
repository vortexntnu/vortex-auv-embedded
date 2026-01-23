## Frame Data Structure Reference

```python
# Understanding what data is available in FrameStore

# Raw signal data
store.arrays["buffer_frames"]              # Shape: (n_frames, 5_hydros, block_size)
store.arrays["working_space_frames"]       # Shape: (n_frames, 5_hydros, workspace_size)

# Pre-computed envelopes (Hilbert magnitude)
store.arrays["buffer_envelope_frames"]     # Shape: (n_frames, 5_hydros, block_size)
store.arrays["working_space_envelope_frames"]  # Shape: (n_frames, 5_hydros, workspace_size)

# Envelope edges (Hilbert of Hilbert)
store.arrays["working_space_envelope_edge_frames"]  # Shape: (n_frames, 5_hydros, workspace_size)
# NOTE: buffer_envelope_edge not pre-computed, must compute in EnvelopeEdgePanel

# Reference hydrophone FFT analysis
store.arrays["reference_buffer_frames"]    # Shape: (n_frames, block_size)
store.arrays["FFT_freqs_frames"]          # Shape: (n_frames, n_freqs)
store.arrays["FFT_signal_frames"]         # Shape: (n_frames, n_freqs)
store.arrays["FFT_noise_frames"]          # Shape: (n_frames, n_freqs)

# Power metrics over time
store.arrays["signal_power_frames"]       # Shape: (n_frames, frame_number)
store.arrays["noise_power_frames"]        # Shape: (n_frames, frame_number)
store.arrays["SNR_frames"]                # Shape: (n_frames, frame_number)

# Metadata
store.meta.block_size              # Integer
store.meta.effective_sampling_rate # Float in Hz
```

---

## Quick Debugging Checklist

- [ ] Does panel have `plot()`, `activate()`, `deactivate()`, `update()` methods?
- [ ] Does `activate()` call `ax.clear()` AND `self.plot()`?
- [ ] Does `deactivate()` call `ax.clear()` AND set `self.lines = []`?
- [ ] Are y-ranges precomputed in `__init__()` using full data?
- [ ] Are bar plots cleared in `update()` before recreation?
- [ ] Is 3D axis created in `update()`, not `plot()`?
- [ ] Are correct data arrays being used (workspace vs buffer)?
- [ ] Are tuple unpacking operators `,` present in line creation?

