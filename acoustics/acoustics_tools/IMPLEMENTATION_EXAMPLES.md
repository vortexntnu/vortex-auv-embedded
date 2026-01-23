# Panel Class Implementation Examples

This document shows concrete examples of how the refactored panels work.

---

## Example 1: Simple Time Domain Panel

```python
class TimeDomainPanel(Panel):
    """Shows raw signal for all 5 hydrophones"""
    
    def __init__(self, axes: list[Axes], is_workspace: bool, store: FrameStore):
        self.axes = axes                    # Shared reference to 5 subplots
        self.is_workspace = is_workspace    # Which data source to use
        self.store = store                  # Reference to data
        self.lines: list[Line2D] = []       # OWNS its Line2D objects
        
        # PRECOMPUTE Y-RANGE ONCE (not per-frame!)
        if is_workspace:
            data_source = store.arrays["working_space_frames"]  # Shape: (1000, 5, 2048)
        else:
            data_source = store.arrays["buffer_frames"]         # Shape: (1000, 5, 512)
        
        # Compute min/max across ALL frames and hydrophones
        self.y_min = float(np.min(data_source))
        self.y_max = float(np.max(data_source))
        if self.y_min == self.y_max:
            self.y_max = self.y_min + 1e-12

    def plot(self) -> None:
        """Create the Line2D objects. Called once during activate()"""
        self.lines = []
        for ax in self.axes:
            line, = ax.plot([], [], linewidth=1)  # Empty line, will update later
            self.lines.append(line)
        print(f"✓ Created {len(self.lines)} Line2D objects")

    def activate(self) -> None:
        """Called when switching TO this panel"""
        # Step 1: Remove any old artists from axes
        for ax in self.axes:
            ax.clear()
            ax.set_visible(True)
        
        # Step 2: Create fresh Line2D objects
        self.plot()
        
        print("✓ TimeDomainPanel activated - axes cleared and lines created")

    def deactivate(self) -> None:
        """Called when switching FROM this panel"""
        # Step 1: Remove all artists
        for ax in self.axes:
            ax.clear()
            ax.set_visible(False)
        
        # Step 2: Clear our reference list
        self.lines = []
        
        print("✓ TimeDomainPanel deactivated - cleaned up")

    def update(self, frame_idx: int, store: FrameStore, state: AppState) -> list[Artist]:
        """Update artist data for the current frame"""
        # Get data for this frame
        if self.is_workspace:
            data = store.arrays["working_space_frames"][frame_idx]  # Shape: (5, 2048)
            x_len = int(data.shape[1])
            title = "Workspace"
        else:
            data = store.arrays["buffer_frames"][frame_idx]         # Shape: (5, 512)
            x_len = store.meta.block_size
            title = "Working Block"
        
        # Create x-axis data
        x = np.arange(x_len)
        artists: list[Artist] = []
        
        # Update each hydrophone line
        for i in range(5):
            # Set x and y data on existing Line2D object
            self.lines[i].set_xdata(x)
            self.lines[i].set_ydata(data[i])
            
            # Update axis appearance
            self.axes[i].set_title(f"Hydrophone {i+1} - {title}")
            self.axes[i].set_xlim(0, x_len)
            self.axes[i].set_ylim(self.y_min, self.y_max)  # Use precomputed range!
            self.axes[i].set_xlabel("Sample Index")
            self.axes[i].set_ylabel("Amplitude")
            
            # Return for animation
            artists.append(self.lines[i])
        
        return artists
```

### Key Points:
1. **Precomputed range** in `__init__()` - computed once, used for all frames
2. **`plot()`** - creates Line2D objects, called from `activate()`
3. **`activate()`** - clears axes, calls `plot()`, shows axes
4. **`deactivate()`** - clears axes, clears references, hides axes
5. **`update()`** - ONLY modifies existing artists, no creation

---

## Example 2: FFT Panel (Frequency Domain)

```python
class FrequencyDomainPanel(Panel):
    """Shows FFT magnitude for all 5 hydrophones"""
    
    def __init__(self, axes: list[Axes], is_workspace: bool, store: FrameStore):
        self.axes = axes
        self.is_workspace = is_workspace
        self.store = store
        self.lines: list[Line2D] = []
        
        # PRECOMPUTE Y-RANGE: compute FFT across ALL frames
        fs = store.meta.effective_sampling_rate
        if is_workspace:
            data_source = store.arrays["working_space_frames"]
        else:
            data_source = store.arrays["buffer_frames"]
        
        # Compute max FFT magnitude across all frames and hydrophones
        max_fft = 0.0
        for frame_data in data_source:        # Loop frame 1...N
            for hydro_data in frame_data:     # Loop hydro 1...5
                fft_vals = np.abs(np.fft.fft(hydro_data)) / len(hydro_data)
                max_fft = max(max_fft, np.max(fft_vals))
        
        self.y_max = max_fft * 1.1 if max_fft > 0 else 1.0

    def plot(self) -> None:
        """Create Line2D objects for FFT plots"""
        self.lines = []
        for ax in self.axes:
            line, = ax.plot([], [], linewidth=1)
            self.lines.append(line)

    def activate(self) -> None:
        for ax in self.axes:
            ax.clear()
            ax.set_visible(True)
        self.plot()

    def deactivate(self) -> None:
        for ax in self.axes:
            ax.clear()
            ax.set_visible(False)
        self.lines = []

    def update(self, frame_idx: int, store: FrameStore, state: AppState) -> list[Artist]:
        if self.is_workspace:
            data = store.arrays["working_space_frames"][frame_idx]
            title = "Workspace"
        else:
            data = store.arrays["buffer_frames"][frame_idx]
            title = "Working Block"
        
        artists: list[Artist] = []
        fs = store.meta.effective_sampling_rate
        
        # For each hydrophone
        for i in range(5):
            # Compute FFT
            signal = data[i]
            fft_vals = np.fft.fft(signal)
            freqs = np.fft.fftfreq(len(signal), d=1/fs)
            
            # Get positive frequencies only
            pos_mask = freqs >= 0
            plot_freqs = freqs[pos_mask]
            plot_mag = np.abs(fft_vals[pos_mask]) / len(signal)
            
            # Update Line2D
            self.lines[i].set_xdata(plot_freqs)
            self.lines[i].set_ydata(plot_mag)
            
            # Update axis
            self.axes[i].set_title(f"Hydrophone {i+1} FFT - {title}")
            self.axes[i].set_xlim(0, fs / 2)
            self.axes[i].set_ylim(0, self.y_max)  # Precomputed!
            self.axes[i].set_xlabel("Frequency (Hz)")
            self.axes[i].set_ylabel("Magnitude")
            
            artists.append(self.lines[i])
        
        return artists
```

### Key Differences from TimeDomainPanel:
- Y-range computation is more complex (involves FFT computation)
- Performed once in `__init__()`, not per-frame
- Data processing in `update()` is FFT-specific

---

## Example 3: Overlap Panel (Multiple Lines Per Axis)

```python
class OverlapPanel(Panel):
    """Shows all 5 hydrophones overlapped"""
    
    def __init__(self, axes: list[Axes], is_workspace: bool, store: FrameStore):
        self.axes = axes
        self.is_workspace = is_workspace
        self.store = store
        self.lines: list[list[Line2D]] = []  # 2D array: [plot][hydro]
        self.colors = ['tab:blue', 'tab:orange', 'tab:green', 'tab:red', 'tab:purple']
        
        # Precompute multiple ranges (one per plot type)
        if is_workspace:
            raw_data = store.arrays["working_space_frames"]
            env_data = store.arrays["working_space_envelope_frames"]
            edge_data = store.arrays["working_space_envelope_edge_frames"]
        else:
            raw_data = store.arrays["buffer_frames"]
            env_data = store.arrays["buffer_envelope_frames"]
            # Compute edge data
            import scipy.signal as scpy
            edge_data = []
            for buffer_frame in raw_data:
                edge = np.abs(scpy.hilbert(np.abs(scpy.hilbert(buffer_frame, axis=1)), axis=1)).imag
                edge_data.append(edge)
            edge_data = np.array(edge_data)
        
        # Store precomputed ranges
        self.y_min_raw = float(np.min(raw_data))
        self.y_max_raw = float(np.max(raw_data))
        self.y_min_env = float(np.min(env_data))
        self.y_max_env = float(np.max(env_data))
        self.y_min_edge = float(np.min(edge_data))
        self.y_max_edge = float(np.max(edge_data))

    def plot(self) -> None:
        """Create 5 lines for each of 4 plots"""
        self.lines = []
        for plot_idx in range(5):
            if plot_idx == 4:
                # Plot 4 is legend, no lines
                self.lines.append([])
            else:
                # Plots 0-3 each have 5 lines (one per hydrophone)
                ax_lines = []
                for hydro_idx in range(5):
                    line, = self.axes[plot_idx].plot([], [], alpha=0.7, linewidth=1)
                    line.set_color(self.colors[hydro_idx])
                    ax_lines.append(line)
                self.lines.append(ax_lines)

    def activate(self) -> None:
        for ax in self.axes:
            ax.clear()
            ax.set_visible(True)
        self.plot()

    def deactivate(self) -> None:
        for ax in self.axes:
            ax.clear()
            ax.set_visible(False)
        self.lines = []

    def update(self, frame_idx: int, store: FrameStore, state: AppState) -> list[Artist]:
        # Get data
        if self.is_workspace:
            raw_data = store.arrays["working_space_frames"][frame_idx]
            env_data = store.arrays["working_space_envelope_frames"][frame_idx]
            edge_data = store.arrays["working_space_envelope_edge_frames"][frame_idx]
        else:
            raw_data = store.arrays["buffer_frames"][frame_idx]
            env_data = store.arrays["buffer_envelope_frames"][frame_idx]
            # Compute edge
            import scipy.signal as scpy
            edge_data = np.abs(scpy.hilbert(np.abs(scpy.hilbert(raw_data, axis=1)), axis=1)).imag
        
        x_len = int(raw_data.shape[1])
        x = np.arange(x_len)
        artists: list[Artist] = []
        
        # Plot 0: Time domain overlapped
        self.axes[0].set_title(f"All Hydrophones - Time Domain")
        self.axes[0].set_xlim(0, x_len)
        self.axes[0].set_ylim(self.y_min_raw, self.y_max_raw)  # Precomputed!
        for i in range(5):
            self.lines[0][i].set_xdata(x)
            self.lines[0][i].set_ydata(raw_data[i])
            artists.append(self.lines[0][i])
        
        # Plot 1: Frequency overlapped
        self.axes[1].set_title(f"All Hydrophones - Frequency Domain")
        for i in range(5):
            fft_vals = np.fft.fft(raw_data[i])
            freqs = np.fft.fftfreq(len(raw_data[i]), d=1/store.meta.effective_sampling_rate)
            pos_mask = freqs >= 0
            plot_freqs = freqs[pos_mask]
            plot_mag = np.abs(fft_vals[pos_mask]) / len(raw_data[i])
            
            self.lines[1][i].set_xdata(plot_freqs)
            self.lines[1][i].set_ydata(plot_mag)
            artists.append(self.lines[1][i])
        
        # Plot 2: Envelope overlapped
        self.axes[2].set_title(f"All Hydrophones - Envelope")
        self.axes[2].set_xlim(0, x_len)
        self.axes[2].set_ylim(self.y_min_env, self.y_max_env)  # Precomputed!
        for i in range(5):
            self.lines[2][i].set_xdata(x)
            self.lines[2][i].set_ydata(env_data[i])
            artists.append(self.lines[2][i])
        
        # Plot 3: Edge overlapped
        self.axes[3].set_title(f"All Hydrophones - Envelope Edge")
        self.axes[3].set_xlim(0, x_len)
        self.axes[3].set_ylim(self.y_min_edge, self.y_max_edge)  # Precomputed!
        for i in range(5):
            self.lines[3][i].set_xdata(x)
            self.lines[3][i].set_ydata(edge_data[i])
            artists.append(self.lines[3][i])
        
        # Plot 4: Legend (no lines to update)
        self.axes[4].clear()
        self.axes[4].axis('off')
        legend_lines = [plt.Line2D([0], [0], color=self.colors[i], lw=2) for i in range(5)]
        legend_labels = [f'Hydrophone {i+1}' for i in range(5)]
        self.axes[4].legend(legend_lines, legend_labels, loc='center', fontsize=12)
        
        return artists
```

### Key Points:
- **2D line array** `self.lines[plot_idx][hydro_idx]`
- **Multiple precomputed ranges** (one per plot type)
- **Plot 4 is special** - legend, no lines to update
- **Colors applied** during `plot()` creation

---

## Example 4: Detection Panel (With Bar Charts)

```python
class DetectionPanel(Panel):
    """Reference hydrophone analysis with bar charts"""
    
    def __init__(self, axes: list[Axes], store: FrameStore, fig: Figure):
        self.axes = axes
        self.store = store
        self.fig = fig
        self.lines: list[Line2D | None] = [None, None, None, None, None]
        self.threshold_line: Line2D | None = None
        
        # Precompute ranges for power and SNR plots
        self.y_min_sig, self.y_max_sig = store.range("signal_power_frames")
        self.y_min_noi, self.y_max_noi = store.range("noise_power_frames")
        self.y_min_snr, self.y_max_snr = store.range("SNR_frames")

    def plot(self) -> None:
        """Create Line2D ONLY for non-bar plots"""
        # Plots 0, 2, 3, 4 use Line2D
        for idx in [0, 2, 3, 4]:
            line, = self.axes[idx].plot([], [], linewidth=1)
            self.lines[idx] = line
        
        # Plot 1 will be bar chart - no Line2D needed
        self.lines[1] = None
        
        # Create threshold line for Plot 4
        self.threshold_line = self.axes[4].axhline(0, color='red', linestyle='--', linewidth=1, alpha=0.6)

    def activate(self) -> None:
        for ax in self.axes:
            ax.clear()
            ax.set_visible(True)
        self.plot()

    def deactivate(self) -> None:
        for ax in self.axes:
            ax.clear()
            ax.set_visible(False)
        self.lines = [None, None, None, None, None]
        self.threshold_line = None

    def update(self, frame_idx: int, store: FrameStore, state: AppState) -> list[Artist]:
        meta = store.meta
        artists: list[Artist] = []
        
        adc_min = store.scalars["adc_min"]
        adc_max = store.scalars["adc_max"]
        
        # Plot 0: Reference buffer
        self.lines[0].set_xdata(np.arange(meta.block_size))
        self.lines[0].set_ydata(store.arrays["reference_buffer_frames"][frame_idx])
        self.axes[0].set_title("Reference - Working Block")
        self.axes[0].set_xlim(0, meta.block_size)
        self.axes[0].set_ylim(adc_min, adc_max)
        artists.append(self.lines[0])
        
        # Plot 1: FFT bar chart - RECREATE EACH FRAME
        freqs = store.arrays["FFT_freqs_frames"][frame_idx]
        sig = store.arrays["FFT_signal_frames"][frame_idx]
        noi = store.arrays["FFT_noise_frames"][frame_idx]
        
        self.axes[1].clear()  # ← MUST clear before bar()
        width = (freqs[-1] - freqs[0]) / len(freqs) * 0.8 if len(freqs) > 1 else 0.8
        self.axes[1].bar(freqs, sig, width=width, color='tab:blue', alpha=0.7, label='Signal')
        self.axes[1].bar(freqs, noi, width=width, color='tab:orange', alpha=0.7, label='Noise')
        self.axes[1].set_title("FFT - Signal vs Noise")
        self.axes[1].set_xlabel("Frequency (Hz)")
        self.axes[1].legend()
        
        # Plot 2: Signal power
        n_frames = meta.frame_number
        self.lines[2].set_xdata(np.arange(n_frames))
        self.lines[2].set_ydata(store.arrays["signal_power_frames"][frame_idx])
        self.axes[2].set_title("Signal Power")
        self.axes[2].set_xlim(0, n_frames)
        self.axes[2].set_ylim(self.y_min_sig, self.y_max_sig)  # Precomputed!
        artists.append(self.lines[2])
        
        # Plot 3: Noise power
        self.lines[3].set_xdata(np.arange(n_frames))
        self.lines[3].set_ydata(store.arrays["noise_power_frames"][frame_idx])
        self.axes[3].set_title("Noise Power")
        self.axes[3].set_xlim(0, n_frames)
        self.axes[3].set_ylim(self.y_min_noi, self.y_max_noi)  # Precomputed!
        artists.append(self.lines[3])
        
        # Plot 4: SNR with threshold
        self.lines[4].set_xdata(np.arange(n_frames))
        self.lines[4].set_ydata(store.arrays["SNR_frames"][frame_idx])
        self.axes[4].set_title("SNR (dB)")
        self.axes[4].set_xlim(0, n_frames)
        self.axes[4].set_ylim(self.y_min_snr, self.y_max_snr)  # Precomputed!
        
        self.threshold_line.set_xdata([0, n_frames])
        self.threshold_line.set_ydata([0, 0])
        
        artists.append(self.lines[4])
        artists.append(self.threshold_line)
        
        return artists
```

### Key Points:
- **`self.lines[1] = None`** because it's a bar chart
- **`self.axes[1].clear()`** in `update()` to remove old bars before creating new ones
- Bar plots are **recreated every frame**, not updated like Line2D
- Other plots use precomputed ranges

---

## Summary: The Pattern

Every panel follows this structure:

```
__init__():
    - Store references to axes and store
    - Precompute y-ranges across ALL frames
    - Initialize self.lines = []

plot():
    - Create Line2D/other artist objects
    - Append to self.lines
    - Don't try to render data yet

activate():
    for ax in self.axes:
        ax.clear()              # Remove old artists
        ax.set_visible(True)
    self.plot()                 # Create fresh artists

deactivate():
    for ax in self.axes:
        ax.clear()              # Remove artists
        ax.set_visible(False)
    self.lines = []             # Clear references

update(frame_idx, ...):
    - Get data for this frame
    - FOR EACH LINE OBJECT:
        - line.set_xdata(...)
        - line.set_ydata(...)
    - (Special handling: bars recreated, 3D axes swapped)
    - Return list of artists to animate
```

