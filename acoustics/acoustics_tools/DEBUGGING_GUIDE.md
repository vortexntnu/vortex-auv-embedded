# GUI Refactoring - Debugging Guide

## What Changed

Each panel now **owns its own artists** rather than sharing pre-created `Line2D` objects. This solves the clearing issues when switching between incompatible plot types (line plots vs bar plots).

### Key Architecture Principles

1. **Each Panel owns its artists** - created in `plot()` method
2. **activate()** - Clear axes, create fresh artists, show axes
3. **deactivate()** - Clear axes, remove artists, hide axes
4. **update()** - Modify artist data only (no creation!)

---

## Where to Look If Issues Persist

### Issue 1: Plots appear ghosted or overlapped

**Check:** Panel's `activate()` and `deactivate()` methods

```python
def activate(self):
    for ax in self.axes:
        ax.clear()  # ← MUST clear first
        ax.set_visible(True)
    self.plot()  # ← Then create artists

def deactivate(self):
    for ax in self.axes:
        ax.clear()  # ← Clear all artists
        ax.set_visible(False)
    self.lines = []  # ← Remove references
```

**If missing `ax.clear()`**: Old artists remain on axes
**If not calling `self.plot()`**: No new artists to draw

---

### Issue 2: Y-axis ranges jumping or showing wrong scale

**Check:** Panel's `__init__` method - y-range precomputation

```python
def __init__(self, axes, is_workspace, store):
    # Must compute across ALL frames, not just current frame
    if is_workspace:
        data_source = store.arrays["working_space_frames"]
    else:
        data_source = store.arrays["buffer_frames"]
    
    self.y_min = float(np.min(data_source))  # All frames
    self.y_max = float(np.max(data_source))  # All frames
```

**If you see jumping ranges**: You're computing in `update()` instead of `__init__()`
- Solution: Move range computation to `__init__()` and store as `self.y_min`, `self.y_max`

**If ranges too tight or too loose**: Check the data source
- For Workspace panels: Use `store.arrays["working_space_frames"]`
- For Working Block panels: Use `store.arrays["buffer_frames"]`
- Don't mix sources!

---

### Issue 3: FFT or Overlap panels not updating

**Check:** That panel's `plot()` method is being called

FFT and Overlap panels need **multiple Line2D objects**:

```python
# FFT: One line per hydrophone (5 total)
def plot(self):
    self.lines = []
    for ax in self.axes:
        line, = ax.plot([], [])  # ← Tuple unpacking
        self.lines.append(line)

# Overlap: 5 lines per plot (5 plots with 5 lines each = 25 total)
def plot(self):
    self.lines = []
    for plot_idx in range(5):
        ax_lines = []
        for hydro_idx in range(5):
            line, = self.axes[plot_idx].plot([], [])
            ax_lines.append(line)
        self.lines.append(ax_lines)
```

**If lines don't appear**: Check that tuple unpacking `line,` is correct (note the comma!)

---

### Issue 4: Detection panel bar chart not rendering

**Special case:** Detection panel uses bar charts (recreated each frame, not pre-created)

```python
def update(self, frame_idx, store, state):
    # Plots 0, 2, 3, 4 use regular lines
    self.lines[0].set_xdata(...)  # ← Works
    
    # Plot 1 is a bar chart - clear and recreate
    self.axes[1].clear()  # ← Must clear
    self.axes[1].bar(freqs, sig, ...)  # ← Recreate bars
```

**If bars don't show**: Check that `self.axes[1].clear()` is called before `self.axes[1].bar()`

---

### Issue 5: 3D Hilbert plot not showing

**Check:** HilbertPanel's `activate()` and `update()` methods

```python
def activate(self):
    for ax in self.axes:
        ax.clear()
        ax.set_visible(True)  # ← Clears the old 3D axis
    self.plot()  # Creates 2D lines only

def update(self, ...):
    # Plot 1 needs special handling
    self.axes[1].clear()  # Clear old 2D axis
    self.ax_3d = self.fig.add_subplot(5, 1, 2, projection='3d')
    self.axes[1] = self.ax_3d  # ← Replace old axis object
    self.ax_3d.plot(x, real, imag)  # ← Draw on new 3D axis
```

**If 3D doesn't appear**: Check that `self.fig.add_subplot()` is called in `update()`, not in `plot()`

---

## Testing Order

Test switching in this order to catch issues early:

1. **Time Domain ↔ Frequency Domain** (both simple line plots)
   - If this breaks: Problem in basic `plot()/activate()/deactivate()`

2. **Time Domain → Overlap** (multiple lines per axis)
   - If this breaks: Problem in Overlap's line creation

3. **Time Domain → Detection** (bar plots involved)
   - If this breaks: Problem in Detection's bar chart handling

4. **Detection → Hilbert** (3D axes involved)
   - If this breaks: Problem in HilbertPanel's 3D axis creation

---

## Common Error Messages & Solutions

### `IndexError: list index out of range` in `update()`
- Problem: Mismatch between number of lines created and number of axes
- Solution: In `plot()`, make sure you create exactly the right number of line objects

### `AttributeError: 'NoneType' object has no attribute 'set_xdata'`
- Problem: Panel's `plot()` wasn't called or set `self.lines` to `None`
- Solution: Check that `activate()` calls `self.plot()` and that `plot()` actually creates the lines

### Y-axis showing tiny range or entire data visible
- Problem: Y-range not precomputed or precomputed on wrong data
- Solution: Verify in `__init__()` that:
  - You're using the correct data source
  - You're using `np.min()`/`np.max()` on the full array
  - You handle the `y_min == y_max` edge case

### Bar chart overlapping with previous plot
- Problem: `axes[1].clear()` not called before `bar()`
- Solution: Always clear bar chart axes at start of `update()` for that axis

---

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

