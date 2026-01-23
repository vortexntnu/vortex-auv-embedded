from __future__ import annotations

from dataclasses import dataclass
from enum import Enum
from pathlib import Path
from typing import Any

import numpy as np
import matplotlib.pyplot as plt

from matplotlib.animation import FuncAnimation, PillowWriter
from matplotlib.artist import Artist
from matplotlib.axes import Axes
from matplotlib.figure import Figure
from matplotlib.lines import Line2D
from matplotlib.widgets import Button
from mpl_toolkits.mplot3d import Axes3D


class MainMode(str, Enum):
    WORKSPACE = "workspace"
    WORKING_BLOCK = "working_block"
    REFERENCE = "reference"


class SubMode(str, Enum):
    # Sub-modes for Workspace and Working Block
    TIME_DOMAIN = "time_domain"
    FREQUENCY_DOMAIN = "frequency_domain"
    HILBERT_ENVELOPE = "hilbert_envelope"
    ENVELOPE_EDGE = "envelope_edge"
    OVERLAP = "overlap"
    
    # Sub-modes for Reference
    DETECTION = "detection"
    HILBERT = "hilbert"


@dataclass(frozen=True)
class CaptureMeta:
    block_size: int
    working_space_size: int
    frame_skip: int
    warmup_samples: int
    effective_sampling_rate: float
    pinger_frequency: float
    frame_number: int
    pinger_found: bool


class FrameStore:
    """Holds all captured arrays and provides cached min/max ranges."""

    def __init__(self, *, meta: CaptureMeta, arrays: dict[str, np.ndarray], scalars: dict[str, float] | None = None):
        self.meta = meta
        self.arrays = arrays
        self.scalars = scalars or {}
        self._ranges: dict[str, tuple[float, float]] = {}

    def __len__(self) -> int:
        # All frame-based arrays use axis=0 as frame index
        # Prefer "buffer_frames" as the canonical length.
        return int(self.arrays["buffer_frames"].shape[0])

    def range(self, key: str) -> tuple[float, float]:
        if key in self._ranges:
            return self._ranges[key]
        if key in self.scalars:
            v = float(self.scalars[key])
            self._ranges[key] = (v, v)
            return self._ranges[key]
        if key not in self.arrays:
            raise KeyError(f"FrameStore has no key: {key}")
        arr = self.arrays[key]
        lo = float(np.nanmin(arr))
        hi = float(np.nanmax(arr))
        if not np.isfinite(lo):
            lo = 0.0
        if not np.isfinite(hi):
            hi = 1.0
        if lo == hi:
            hi = lo + 1e-12
        self._ranges[key] = (lo, hi)
        return self._ranges[key]

    def save_npz(self, path: str | Path) -> None:
        path = Path(path)
        meta_dict = {
            "block_size": self.meta.block_size,
            "working_space_size": self.meta.working_space_size,
            "frame_skip": self.meta.frame_skip,
            "warmup_samples": self.meta.warmup_samples,
            "effective_sampling_rate": self.meta.effective_sampling_rate,
            "pinger_frequency": self.meta.pinger_frequency,
            "frame_number": self.meta.frame_number,
            "pinger_found": int(self.meta.pinger_found),
        }
        payload: dict[str, Any] = {}
        for k, v in meta_dict.items():
            payload[f"meta__{k}"] = np.array(v)
        for k, v in (self.scalars or {}).items():
            payload[f"scalar__{k}"] = np.array(float(v))
        for k, arr in self.arrays.items():
            payload[f"arr__{k}"] = arr
        np.savez_compressed(path, **payload)

    @staticmethod
    def load_npz(path: str | Path) -> "FrameStore":
        path = Path(path)
        data = np.load(path, allow_pickle=False)
        meta_kwargs: dict[str, Any] = {}
        scalars: dict[str, float] = {}
        arrays: dict[str, np.ndarray] = {}

        for k in data.files:
            if k.startswith("meta__"):
                meta_kwargs[k[len("meta__"):]] = data[k].item()
            elif k.startswith("scalar__"):
                scalars[k[len("scalar__"):]] = float(data[k].item())
            elif k.startswith("arr__"):
                arrays[k[len("arr__"):]] = data[k]

        meta = CaptureMeta(
            block_size=int(meta_kwargs["block_size"]),
            working_space_size=int(meta_kwargs["working_space_size"]),
            frame_skip=int(meta_kwargs["frame_skip"]),
            warmup_samples=int(meta_kwargs["warmup_samples"]),
            effective_sampling_rate=float(meta_kwargs["effective_sampling_rate"]),
            pinger_frequency=float(meta_kwargs["pinger_frequency"]),
            frame_number=int(meta_kwargs["frame_number"]),
            pinger_found=bool(int(meta_kwargs["pinger_found"])),
        )
        return FrameStore(meta=meta, arrays=arrays, scalars=scalars)


@dataclass
class AppState:
    paused: bool = False
    current_frame: int = 0
    main_mode: MainMode = MainMode.WORKING_BLOCK
    sub_mode: SubMode = SubMode.TIME_DOMAIN

    def set_main_mode(self, mode: MainMode) -> None:
        """Set main mode and reset to appropriate default sub-mode."""
        self.main_mode = mode
        if mode == MainMode.REFERENCE:
            self.sub_mode = SubMode.DETECTION
        else:
            self.sub_mode = SubMode.TIME_DOMAIN

    def set_sub_mode(self, mode: SubMode) -> None:
        """Set sub-mode (only valid if compatible with current main mode)."""
        self.sub_mode = mode


class Panel:
    def activate(self) -> None:
        raise NotImplementedError

    def deactivate(self) -> None:
        raise NotImplementedError

    def update(self, frame_idx: int, store: FrameStore, state: AppState) -> list[Artist]:
        raise NotImplementedError


# ============================================================================
# Workspace / Working Block Panels (shared structure, different data source)
# ============================================================================

class TimeDomainPanel(Panel):
    """Shows raw time-domain data for all 5 hydrophones."""
    
    def __init__(self, axes: list[Axes], lines: list[Line2D], is_workspace: bool):
        self.axes = axes
        self.lines = lines
        self.is_workspace = is_workspace

    def activate(self) -> None:
        for ax in self.axes:
            ax.set_visible(True)

    def deactivate(self) -> None:
        pass

    def update(self, frame_idx: int, store: FrameStore, state: AppState) -> list[Artist]:
        if self.is_workspace:
            data = store.arrays["working_space_frames"][frame_idx]
            x_len = int(data.shape[1])
            title_suffix = "Workspace"
        else:
            data = store.arrays["buffer_frames"][frame_idx]
            x_len = store.meta.block_size
            title_suffix = "Working Block"

        adc_min = store.scalars["adc_min"]
        adc_max = store.scalars["adc_max"]
        x = np.arange(x_len)
        artists: list[Artist] = []

        for i in range(5):
            self.lines[i].set_xdata(x)
            self.lines[i].set_ydata(data[i])
            self.axes[i].set_title(f"Hydrophone {i+1} - {title_suffix}")
            self.axes[i].set_xlim(0, x_len)
            self.axes[i].set_ylim(adc_min, adc_max)
            self.axes[i].set_xlabel("Sample Index")
            artists.append(self.lines[i])

        return artists


class FrequencyDomainPanel(Panel):
    """Shows FFT magnitude for all 5 hydrophones."""
    
    def __init__(self, axes: list[Axes], lines: list[Line2D], is_workspace: bool):
        self.axes = axes
        self.lines = lines
        self.is_workspace = is_workspace

    def activate(self) -> None:
        for ax in self.axes:
            ax.set_visible(True)

    def deactivate(self) -> None:
        pass

    def update(self, frame_idx: int, store: FrameStore, state: AppState) -> list[Artist]:
        if self.is_workspace:
            data = store.arrays["working_space_frames"][frame_idx]
            title_suffix = "Workspace"
        else:
            data = store.arrays["buffer_frames"][frame_idx]
            title_suffix = "Working Block"

        artists: list[Artist] = []
        fs = store.meta.effective_sampling_rate

        for i in range(5):
            signal = data[i]
            fft_vals = np.fft.fft(signal)
            freqs = np.fft.fftfreq(len(signal), d=1/fs)
            
            # Plot only positive frequencies
            pos_mask = freqs >= 0
            plot_freqs = freqs[pos_mask]
            plot_mag = np.abs(fft_vals[pos_mask]) / len(signal)

            self.lines[i].set_xdata(plot_freqs)
            self.lines[i].set_ydata(plot_mag)
            self.axes[i].set_title(f"Hydrophone {i+1} FFT - {title_suffix}")
            self.axes[i].set_xlim(0, fs / 2)
            self.axes[i].set_ylim(0, np.max(plot_mag) * 1.1 if np.max(plot_mag) > 0 else 1)
            self.axes[i].set_xlabel("Frequency (Hz)")
            self.axes[i].set_ylabel("Magnitude")
            artists.append(self.lines[i])

        return artists


class HilbertEnvelopePanel(Panel):
    """Shows Hilbert envelope for all 5 hydrophones."""
    
    def __init__(self, axes: list[Axes], lines: list[Line2D], is_workspace: bool):
        self.axes = axes
        self.lines = lines
        self.is_workspace = is_workspace

    def activate(self) -> None:
        for ax in self.axes:
            ax.set_visible(True)

    def deactivate(self) -> None:
        pass

    def update(self, frame_idx: int, store: FrameStore, state: AppState) -> list[Artist]:
        if self.is_workspace:
            data = store.arrays["working_space_envelope_frames"][frame_idx]
            x_len = int(data.shape[1])
            title_suffix = "Workspace"
        else:
            data = store.arrays["buffer_envelope_frames"][frame_idx]
            x_len = store.meta.block_size
            title_suffix = "Working Block"

        x = np.arange(x_len)
        artists: list[Artist] = []
        ylo, yhi = float(np.min(data)), float(np.max(data))
        if ylo == yhi:
            yhi = ylo + 1e-12

        for i in range(5):
            self.lines[i].set_xdata(x)
            self.lines[i].set_ydata(data[i])
            self.axes[i].set_title(f"Hydrophone {i+1} Envelope - {title_suffix}")
            self.axes[i].set_xlim(0, x_len)
            self.axes[i].set_ylim(ylo, yhi)
            self.axes[i].set_xlabel("Sample Index")
            artists.append(self.lines[i])

        return artists


class EnvelopeEdgePanel(Panel):
    """Shows envelope edge (imaginary part of Hilbert of envelope) for all 5 hydrophones."""
    
    def __init__(self, axes: list[Axes], lines: list[Line2D], is_workspace: bool):
        self.axes = axes
        self.lines = lines
        self.is_workspace = is_workspace

    def activate(self) -> None:
        for ax in self.axes:
            ax.set_visible(True)

    def deactivate(self) -> None:
        pass

    def update(self, frame_idx: int, store: FrameStore, state: AppState) -> list[Artist]:
        if self.is_workspace:
            data = store.arrays["working_space_envelope_edge_frames"][frame_idx]
            x_len = int(data.shape[1])
            title_suffix = "Workspace"
        else:
            # Need to compute envelope edge for working block
            import scipy.signal as scpy
            buffer_data = store.arrays["buffer_frames"][frame_idx]
            data = np.abs(scpy.hilbert(np.abs(scpy.hilbert(buffer_data, axis=1)), axis=1)).imag
            x_len = store.meta.block_size
            title_suffix = "Working Block"

        x = np.arange(x_len)
        artists: list[Artist] = []
        ylo, yhi = float(np.min(data)), float(np.max(data))
        if ylo == yhi:
            yhi = ylo + 1e-12

        for i in range(5):
            self.lines[i].set_xdata(x)
            self.lines[i].set_ydata(data[i])
            self.axes[i].set_title(f"Hydrophone {i+1} Envelope Edge - {title_suffix}")
            self.axes[i].set_xlim(0, x_len)
            self.axes[i].set_ylim(ylo, yhi)
            self.axes[i].set_xlabel("Sample Index")
            artists.append(self.lines[i])

        return artists


class OverlapPanel(Panel):
    """Shows all 5 hydrophones overlapped in 5 different transform plots."""
    
    def __init__(self, axes: list[Axes], lines: list[list[Line2D]], is_workspace: bool):
        self.axes = axes
        self.lines = lines  # lines[plot_idx][hydro_idx]
        self.is_workspace = is_workspace

    def activate(self) -> None:
        for ax in self.axes:
            ax.set_visible(True)

    def deactivate(self) -> None:
        pass

    def update(self, frame_idx: int, store: FrameStore, state: AppState) -> list[Artist]:
        import scipy.signal as scpy
        
        if self.is_workspace:
            raw_data = store.arrays["working_space_frames"][frame_idx]
            env_data = store.arrays["working_space_envelope_frames"][frame_idx]
            edge_data = store.arrays["working_space_envelope_edge_frames"][frame_idx]
            title_suffix = "Workspace"
        else:
            raw_data = store.arrays["buffer_frames"][frame_idx]
            env_data = store.arrays["buffer_envelope_frames"][frame_idx]
            edge_data = np.abs(scpy.hilbert(np.abs(scpy.hilbert(raw_data, axis=1)), axis=1)).imag
            title_suffix = "Working Block"

        x_len = int(raw_data.shape[1])
        x = np.arange(x_len)
        fs = store.meta.effective_sampling_rate
        artists: list[Artist] = []

        colors = ['tab:blue', 'tab:orange', 'tab:green', 'tab:red', 'tab:purple']

        # Plot 0: Time domain
        ylo, yhi = store.scalars["adc_min"], store.scalars["adc_max"]
        self.axes[0].set_title(f"All Hydrophones - Time Domain - {title_suffix}")
        self.axes[0].set_xlim(0, x_len)
        self.axes[0].set_ylim(ylo, yhi)
        self.axes[0].set_xlabel("Sample Index")
        for i in range(5):
            self.lines[0][i].set_xdata(x)
            self.lines[0][i].set_ydata(raw_data[i])
            self.lines[0][i].set_color(colors[i])
            artists.append(self.lines[0][i])

        # Plot 1: Frequency domain
        self.axes[1].set_title(f"All Hydrophones - Frequency Domain - {title_suffix}")
        for i in range(5):
            fft_vals = np.fft.fft(raw_data[i])
            freqs = np.fft.fftfreq(len(raw_data[i]), d=1/fs)
            pos_mask = freqs >= 0
            plot_freqs = freqs[pos_mask]
            plot_mag = np.abs(fft_vals[pos_mask]) / len(raw_data[i])
            
            self.lines[1][i].set_xdata(plot_freqs)
            self.lines[1][i].set_ydata(plot_mag)
            self.lines[1][i].set_color(colors[i])
            artists.append(self.lines[1][i])
        
        self.axes[1].set_xlim(0, fs / 2)
        max_mag = max(np.max(np.abs(np.fft.fft(raw_data[i])[:len(raw_data[i])//2]) / len(raw_data[i])) for i in range(5))
        self.axes[1].set_ylim(0, max_mag * 1.1 if max_mag > 0 else 1)
        self.axes[1].set_xlabel("Frequency (Hz)")

        # Plot 2: Hilbert envelope
        ylo, yhi = float(np.min(env_data)), float(np.max(env_data))
        if ylo == yhi:
            yhi = ylo + 1e-12
        self.axes[2].set_title(f"All Hydrophones - Hilbert Envelope - {title_suffix}")
        self.axes[2].set_xlim(0, x_len)
        self.axes[2].set_ylim(ylo, yhi)
        self.axes[2].set_xlabel("Sample Index")
        for i in range(5):
            self.lines[2][i].set_xdata(x)
            self.lines[2][i].set_ydata(env_data[i])
            self.lines[2][i].set_color(colors[i])
            artists.append(self.lines[2][i])

        # Plot 3: Envelope edge
        ylo, yhi = float(np.min(edge_data)), float(np.max(edge_data))
        if ylo == yhi:
            yhi = ylo + 1e-12
        self.axes[3].set_title(f"All Hydrophones - Envelope Edge - {title_suffix}")
        self.axes[3].set_xlim(0, x_len)
        self.axes[3].set_ylim(ylo, yhi)
        self.axes[3].set_xlabel("Sample Index")
        for i in range(5):
            self.lines[3][i].set_xdata(x)
            self.lines[3][i].set_ydata(edge_data[i])
            self.lines[3][i].set_color(colors[i])
            artists.append(self.lines[3][i])

        # Plot 4: Legend
        self.axes[4].clear()
        self.axes[4].axis('off')
        legend_lines = [plt.Line2D([0], [0], color=colors[i], lw=2) for i in range(5)]
        legend_labels = [f'Hydrophone {i+1}' for i in range(5)]
        self.axes[4].legend(legend_lines, legend_labels, loc='center', fontsize=12, frameon=True)

        return artists


# ============================================================================
# Reference Hydrophone Panels
# ============================================================================

class DetectionPanel(Panel):
    """Detection view for reference hydrophone."""
    
    def __init__(self, axes: list[Axes], lines: list[Line2D], bar_container, threshold_line: Line2D):
        self.axes = axes
        self.lines = lines
        self.bar_container = bar_container
        self.threshold_line = threshold_line

    def activate(self) -> None:
        for ax in self.axes:
            ax.set_visible(True)
        self.threshold_line.set_visible(True)

    def deactivate(self) -> None:
        self.threshold_line.set_visible(False)

    def update(self, frame_idx: int, store: FrameStore, state: AppState) -> list[Artist]:
        meta = store.meta
        artists: list[Artist] = []

        adc_min = store.scalars["adc_min"]
        adc_max = store.scalars["adc_max"]

        # Plot 0: Reference buffer (time domain)
        self.lines[0].set_xdata(np.arange(meta.block_size))
        self.lines[0].set_ydata(store.arrays["reference_buffer_frames"][frame_idx])
        self.axes[0].set_title("Reference Hydrophone - Working Block")
        self.axes[0].set_xlim(0, meta.block_size)
        self.axes[0].set_ylim(adc_min, adc_max)
        self.axes[0].set_xlabel("Sample Index")
        artists.append(self.lines[0])

        # Plot 1: FFT bar plot (signal vs noise)
        freqs = store.arrays["FFT_freqs_frames"][frame_idx]
        sig = store.arrays["FFT_signal_frames"][frame_idx]
        noi = store.arrays["FFT_noise_frames"][frame_idx]
        
        self.axes[1].clear()
        width = (freqs[-1] - freqs[0]) / len(freqs) * 0.8
        self.axes[1].bar(freqs, sig, width=width, color='tab:blue', alpha=0.7, label='Signal')
        self.axes[1].bar(freqs, noi, width=width, color='tab:orange', alpha=0.7, label='Noise')
        self.axes[1].set_title("FFT - Signal vs Noise Bins")
        self.axes[1].set_xlabel("Frequency (Hz)")
        self.axes[1].set_ylabel("Magnitude")
        self.axes[1].legend()
        self.axes[1].set_xlim(float(freqs[0]), float(freqs[-1]))

        # Plot 2: Signal power over frames
        n_frames = meta.frame_number
        self.lines[2].set_xdata(np.arange(n_frames))
        self.lines[2].set_ydata(store.arrays["signal_power_frames"][frame_idx])
        ylo, yhi = store.range("signal_power_frames")
        self.axes[2].set_title("Signal Power Over Frames")
        self.axes[2].set_xlim(0, n_frames)
        self.axes[2].set_ylim(ylo, yhi)
        self.axes[2].set_xlabel("Frame Index")
        artists.append(self.lines[2])

        # Plot 3: Noise power over frames
        self.lines[3].set_xdata(np.arange(n_frames))
        self.lines[3].set_ydata(store.arrays["noise_power_frames"][frame_idx])
        ylo, yhi = store.range("noise_power_frames")
        self.axes[3].set_title("Noise Power Over Frames")
        self.axes[3].set_xlim(0, n_frames)
        self.axes[3].set_ylim(ylo, yhi)
        self.axes[3].set_xlabel("Frame Index")
        artists.append(self.lines[3])

        # Plot 4: SNR over frames with threshold line
        self.lines[4].set_xdata(np.arange(n_frames))
        self.lines[4].set_ydata(store.arrays["SNR_frames"][frame_idx])
        ylo, yhi = store.range("SNR_frames")
        self.axes[4].set_title("SNR (dB) Over Frames")
        self.axes[4].set_xlim(0, n_frames)
        self.axes[4].set_ylim(ylo, yhi)
        self.axes[4].set_xlabel("Frame Index")
        
        # SNR threshold line at 0 dB (SNR = 1.0 linear)
        self.threshold_line.set_xdata([0, n_frames])
        self.threshold_line.set_ydata([0, 0])
        self.threshold_line.set_visible(True)
        
        artists.append(self.lines[4])
        artists.append(self.threshold_line)

        return artists


class HilbertPanel(Panel):
    """Hilbert view for reference hydrophone (full workspace)."""
    
    def __init__(self, axes: list[Axes], lines: list[Line2D], scatter_3d):
        self.axes = axes
        self.lines = lines
        self.scatter_3d = scatter_3d

    def activate(self) -> None:
        for ax in self.axes:
            ax.set_visible(True)

    def deactivate(self) -> None:
        pass

    def update(self, frame_idx: int, store: FrameStore, state: AppState) -> list[Artist]:
        import scipy.signal as scpy
        
        artists: list[Artist] = []
        
        # Get workspace data for reference hydrophone
        ws_data = store.arrays["working_space_frames"][frame_idx][0]  # Reference is index 0
        ws_len = len(ws_data)
        x = np.arange(ws_len)
        
        # Compute Hilbert transform
        analytic_signal = scpy.hilbert(ws_data)
        envelope = np.abs(analytic_signal)
        phase = np.angle(analytic_signal)
        
        # Envelope edge
        envelope_analytic = scpy.hilbert(envelope)
        envelope_edge = envelope_analytic.imag
        
        adc_min = store.scalars["adc_min"]
        adc_max = store.scalars["adc_max"]

        # Plot 0: Raw time data
        self.lines[0].set_xdata(x)
        self.lines[0].set_ydata(ws_data)
        self.axes[0].set_title("Reference Hydrophone - Workspace (Time Domain)")
        self.axes[0].set_xlim(0, ws_len)
        self.axes[0].set_ylim(adc_min, adc_max)
        self.axes[0].set_xlabel("Sample Index")
        artists.append(self.lines[0])

        # Plot 1: 3D plot of complex Hilbert transform
        self.axes[1].clear()
        ax_3d = self.fig.add_subplot(5, 1, 2, projection='3d')
        self.axes[1] = ax_3d
        ax_3d.plot(x, analytic_signal.real, analytic_signal.imag, linewidth=0.5)
        ax_3d.set_title("3D Hilbert Transform (Real, Imag vs Time)")
        ax_3d.set_xlabel("Sample Index")
        ax_3d.set_ylabel("Real Part")
        ax_3d.set_zlabel("Imaginary Part")

        # Plot 2: Phase
        self.lines[2].set_xdata(x)
        self.lines[2].set_ydata(phase)
        self.axes[2].set_title("Phase of Hilbert Transform")
        self.axes[2].set_xlim(0, ws_len)
        self.axes[2].set_ylim(-np.pi, np.pi)
        self.axes[2].set_xlabel("Sample Index")
        self.axes[2].set_ylabel("Phase (radians)")
        artists.append(self.lines[2])

        # Plot 3: Magnitude
        self.lines[3].set_xdata(x)
        self.lines[3].set_ydata(envelope)
        ylo, yhi = float(np.min(envelope)), float(np.max(envelope))
        if ylo == yhi:
            yhi = ylo + 1e-12
        self.axes[3].set_title("Magnitude of Hilbert Transform")
        self.axes[3].set_xlim(0, ws_len)
        self.axes[3].set_ylim(ylo, yhi)
        self.axes[3].set_xlabel("Sample Index")
        artists.append(self.lines[3])

        # Plot 4: Envelope edge
        self.lines[4].set_xdata(x)
        self.lines[4].set_ydata(envelope_edge)
        ylo, yhi = float(np.min(envelope_edge)), float(np.max(envelope_edge))
        if ylo == yhi:
            yhi = ylo + 1e-12
        self.axes[4].set_title("Envelope Edge (Imag of Hilbert of Magnitude)")
        self.axes[4].set_xlim(0, ws_len)
        self.axes[4].set_ylim(ylo, yhi)
        self.axes[4].set_xlabel("Sample Index")
        artists.append(self.lines[4])

        return artists


class GuiApp:
    def __init__(
        self,
        *,
        store: FrameStore,
        desired_fps: int = 24,
    ):
        self.store = store
        self.state = AppState()

        self.desired_fps = desired_fps
        self.animation_interval = int(1000 // desired_fps)

        self.fig: Figure = plt.figure(figsize=(14, 10))
        self.axes: list[Axes] = [plt.subplot(5, 1, i + 1) for i in range(5)]

        # Create 5 reusable lines
        self.lines: list[Line2D] = []
        first = store.arrays["buffer_frames"][0]
        for i in range(5):
            (line,) = self.axes[i].plot(first[i])
            self.lines.append(line)
            self.axes[i].set_xlabel("Sample Index")

        # Create overlap panel lines (5 plots x 5 hydrophones)
        self.overlap_lines: list[list[Line2D]] = []
        for i in range(5):
            hydro_lines = []
            for j in range(5):
                (line,) = self.axes[i].plot([], [], alpha=0.7)
                hydro_lines.append(line)
            self.overlap_lines.append(hydro_lines)

        # Threshold line for SNR plot in Detection panel
        self.snr_threshold_line = self.axes[4].axhline(0, color='red', linestyle='--', linewidth=1, alpha=0.6)
        self.snr_threshold_line.set_visible(False)

        # Bar container placeholder for Detection FFT
        self.bar_container = None
        
        # 3D scatter placeholder for Hilbert panel
        self.scatter_3d = None

        # Build all panels
        self._build_panels()
        self._active_panel_key: tuple[MainMode, SubMode] | None = None

        self._setup_buttons()

        self.ani = FuncAnimation(
            self.fig,
            self._animate,
            frames=len(store),
            interval=self.animation_interval,
            repeat=True,
        )

    def _build_panels(self) -> None:
        """Build all panel instances."""
        self.panels: dict[tuple[MainMode, SubMode], Panel] = {}
        
        # Workspace panels
        self.panels[(MainMode.WORKSPACE, SubMode.TIME_DOMAIN)] = TimeDomainPanel(
            self.axes, self.lines, is_workspace=True
        )
        self.panels[(MainMode.WORKSPACE, SubMode.FREQUENCY_DOMAIN)] = FrequencyDomainPanel(
            self.axes, self.lines, is_workspace=True
        )
        self.panels[(MainMode.WORKSPACE, SubMode.HILBERT_ENVELOPE)] = HilbertEnvelopePanel(
            self.axes, self.lines, is_workspace=True
        )
        self.panels[(MainMode.WORKSPACE, SubMode.ENVELOPE_EDGE)] = EnvelopeEdgePanel(
            self.axes, self.lines, is_workspace=True
        )
        self.panels[(MainMode.WORKSPACE, SubMode.OVERLAP)] = OverlapPanel(
            self.axes, self.overlap_lines, is_workspace=True
        )
        
        # Working Block panels
        self.panels[(MainMode.WORKING_BLOCK, SubMode.TIME_DOMAIN)] = TimeDomainPanel(
            self.axes, self.lines, is_workspace=False
        )
        self.panels[(MainMode.WORKING_BLOCK, SubMode.FREQUENCY_DOMAIN)] = FrequencyDomainPanel(
            self.axes, self.lines, is_workspace=False
        )
        self.panels[(MainMode.WORKING_BLOCK, SubMode.HILBERT_ENVELOPE)] = HilbertEnvelopePanel(
            self.axes, self.lines, is_workspace=False
        )
        self.panels[(MainMode.WORKING_BLOCK, SubMode.ENVELOPE_EDGE)] = EnvelopeEdgePanel(
            self.axes, self.lines, is_workspace=False
        )
        self.panels[(MainMode.WORKING_BLOCK, SubMode.OVERLAP)] = OverlapPanel(
            self.axes, self.overlap_lines, is_workspace=False
        )
        
        # Reference panels
        detection_panel = DetectionPanel(
            self.axes, self.lines, self.bar_container, self.snr_threshold_line
        )
        detection_panel.fig = self.fig  # Needed for 3D axes
        self.panels[(MainMode.REFERENCE, SubMode.DETECTION)] = detection_panel
        
        hilbert_panel = HilbertPanel(
            self.axes, self.lines, self.scatter_3d
        )
        hilbert_panel.fig = self.fig  # Needed for 3D axes
        self.panels[(MainMode.REFERENCE, SubMode.HILBERT)] = hilbert_panel

    def _setup_buttons(self) -> None:
        """Setup button interface with state machine logic."""
        # Always-visible buttons
        ax_play = plt.axes([0.02, 0.02, 0.08, 0.04])
        ax_skip_back = plt.axes([0.11, 0.02, 0.08, 0.04])
        ax_skip_fwd = plt.axes([0.20, 0.02, 0.08, 0.04])
        ax_save = plt.axes([0.29, 0.02, 0.08, 0.04])

        # Main mode buttons
        ax_workspace = plt.axes([0.40, 0.02, 0.08, 0.04])
        ax_working = plt.axes([0.49, 0.02, 0.08, 0.04])
        ax_reference = plt.axes([0.58, 0.02, 0.08, 0.04])

        # Sub-mode buttons (5 buttons for Workspace/Working, 2 for Reference)
        ax_sub1 = plt.axes([0.69, 0.02, 0.06, 0.04])
        ax_sub2 = plt.axes([0.76, 0.02, 0.06, 0.04])
        ax_sub3 = plt.axes([0.83, 0.02, 0.06, 0.04])
        ax_sub4 = plt.axes([0.90, 0.02, 0.06, 0.04])
        ax_sub5 = plt.axes([0.97, 0.02, 0.06, 0.04])

        self.btn_play = Button(ax_play, "Play/Pause")
        self.btn_skip_back = Button(ax_skip_back, "◄")
        self.btn_skip_fwd = Button(ax_skip_fwd, "►")
        self.btn_save = Button(ax_save, "Save GIF")

        self.btn_workspace = Button(ax_workspace, "Workspace")
        self.btn_working = Button(ax_working, "Working")
        self.btn_reference = Button(ax_reference, "Reference")

        self.btn_sub1 = Button(ax_sub1, "")
        self.btn_sub2 = Button(ax_sub2, "")
        self.btn_sub3 = Button(ax_sub3, "")
        self.btn_sub4 = Button(ax_sub4, "")
        self.btn_sub5 = Button(ax_sub5, "")

        self.sub_buttons = [self.btn_sub1, self.btn_sub2, self.btn_sub3, self.btn_sub4, self.btn_sub5]
        self.sub_button_axes = [ax_sub1, ax_sub2, ax_sub3, ax_sub4, ax_sub5]

        self.skip_n = 1

        # Button callbacks
        def on_play(event):
            self.state.paused = not self.state.paused

        def on_skip_back(event):
            self.state.current_frame = max(0, self.state.current_frame - self.skip_n)
            self.state.paused = True

        def on_skip_fwd(event):
            self.state.current_frame = min(len(self.store) - 1, self.state.current_frame + self.skip_n)
            self.state.paused = True

        def on_save(event):
            print("Saving animation as GIF...")
            writer = PillowWriter(fps=self.desired_fps)
            self.ani.save("buffer_animation.gif", writer=writer)
            print("Animation saved as 'buffer_animation.gif'")

        # Main mode callbacks
        def on_workspace(event):
            self.state.set_main_mode(MainMode.WORKSPACE)
            self._update_button_states()
            self.state.paused = True

        def on_working(event):
            self.state.set_main_mode(MainMode.WORKING_BLOCK)
            self._update_button_states()
            self.state.paused = True

        def on_reference(event):
            self.state.set_main_mode(MainMode.REFERENCE)
            self._update_button_states()
            self.state.paused = True

        # Sub-mode callbacks
        def on_sub1(event):
            self._handle_sub_button(0)

        def on_sub2(event):
            self._handle_sub_button(1)

        def on_sub3(event):
            self._handle_sub_button(2)

        def on_sub4(event):
            self._handle_sub_button(3)

        def on_sub5(event):
            self._handle_sub_button(4)

        # Connect callbacks
        self.btn_play.on_clicked(on_play)
        self.btn_skip_back.on_clicked(on_skip_back)
        self.btn_skip_fwd.on_clicked(on_skip_fwd)
        self.btn_save.on_clicked(on_save)

        self.btn_workspace.on_clicked(on_workspace)
        self.btn_working.on_clicked(on_working)
        self.btn_reference.on_clicked(on_reference)

        self.btn_sub1.on_clicked(on_sub1)
        self.btn_sub2.on_clicked(on_sub2)
        self.btn_sub3.on_clicked(on_sub3)
        self.btn_sub4.on_clicked(on_sub4)
        self.btn_sub5.on_clicked(on_sub5)

        plt.subplots_adjust(bottom=0.12)
        
        # Initialize button states
        self._update_button_states()

    def _handle_sub_button(self, index: int) -> None:
        """Handle sub-mode button clicks based on current main mode."""
        if self.state.main_mode in [MainMode.WORKSPACE, MainMode.WORKING_BLOCK]:
            sub_modes = [
                SubMode.TIME_DOMAIN,
                SubMode.FREQUENCY_DOMAIN,
                SubMode.HILBERT_ENVELOPE,
                SubMode.ENVELOPE_EDGE,
                SubMode.OVERLAP,
            ]
            if index < len(sub_modes):
                self.state.set_sub_mode(sub_modes[index])
        elif self.state.main_mode == MainMode.REFERENCE:
            sub_modes = [SubMode.DETECTION, SubMode.HILBERT]
            if index < len(sub_modes):
                self.state.set_sub_mode(sub_modes[index])
        
        self.state.paused = True
        self._update_button_states()

    def _update_button_states(self) -> None:
        """Update button labels and visibility based on current state."""
        # Update sub-mode buttons based on current main mode
        if self.state.main_mode in [MainMode.WORKSPACE, MainMode.WORKING_BLOCK]:
            labels = ["Time", "Freq", "Env", "Edge", "Overlap"]
            for i, (btn, ax, label) in enumerate(zip(self.sub_buttons, self.sub_button_axes, labels)):
                btn.label.set_text(label)
                ax.set_visible(True)
        elif self.state.main_mode == MainMode.REFERENCE:
            labels = ["Detect", "Hilbert"]
            for i in range(len(labels)):
                self.sub_buttons[i].label.set_text(labels[i])
                self.sub_button_axes[i].set_visible(True)
            # Hide unused buttons
            for i in range(len(labels), 5):
                self.sub_button_axes[i].set_visible(False)
        
        self.fig.canvas.draw_idle()

    def _set_active_panel(self, key: tuple[MainMode, SubMode]) -> None:
        """Switch to the specified panel."""
        if self._active_panel_key == key:
            return
        if self._active_panel_key is not None:
            self.panels[self._active_panel_key].deactivate()
        self.panels[key].activate()
        self._active_panel_key = key

    def _animate(self, frame_idx: int):
        if not self.state.paused:
            self.state.current_frame = frame_idx
        frame_idx = int(self.state.current_frame)

        panel_key = (self.state.main_mode, self.state.sub_mode)
        self._set_active_panel(panel_key)
        artists = self.panels[panel_key].update(frame_idx, self.store, self.state)

        status = "PAUSED" if self.state.paused else "PLAYING"
        
        # Generate title
        main_mode_names = {
            MainMode.WORKSPACE: "Workspace",
            MainMode.WORKING_BLOCK: "Working Block",
            MainMode.REFERENCE: "Reference Hydrophone"
        }
        sub_mode_names = {
            SubMode.TIME_DOMAIN: "Time Domain",
            SubMode.FREQUENCY_DOMAIN: "Frequency Domain",
            SubMode.HILBERT_ENVELOPE: "Hilbert Envelope",
            SubMode.ENVELOPE_EDGE: "Envelope Edge",
            SubMode.OVERLAP: "Overlap View",
            SubMode.DETECTION: "Detection",
            SubMode.HILBERT: "Hilbert Transform"
        }
        
        title = f"{main_mode_names[self.state.main_mode]} - {sub_mode_names[self.state.sub_mode]}"
        meta = self.store.meta
        self.fig.suptitle(
            f"{title} | Frame {frame_idx}/{len(self.store) - 1} "
            f"(Sample {meta.warmup_samples + (frame_idx + 1) * meta.frame_skip - 1}) [{status}]"
        )

        return artists

    def show(self) -> None:
        plt.show()


def launch_gui(store: FrameStore, *, desired_fps: int = 24) -> None:
    app = GuiApp(store=store, desired_fps=desired_fps)
    app.show()
