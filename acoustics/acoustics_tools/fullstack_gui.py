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
    HILBERT_3D = "hilbert_3d"


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
    """Base class for all visualization panels.
    
    Each panel owns its own artists and is responsible for:
    - plot(): Create and initialize artists
    - activate(): Show axes and artists
    - deactivate(): Clear axes and remove artists
    - update(): Modify artist data for current frame
    """
    
    def plot(self) -> None:
        """Create the artists for this panel. Called once during activate()."""
        raise NotImplementedError

    def activate(self) -> None:
        """Called when switching TO this panel."""
        raise NotImplementedError

    def deactivate(self) -> None:
        """Called when switching AWAY from this panel."""
        raise NotImplementedError

    def update(self, frame_idx: int, store: FrameStore, state: AppState) -> list[Artist]:
        """Update artist data. Return list of artists to animate."""
        raise NotImplementedError


# ============================================================================
# Workspace / Working Block Panels (shared structure, different data source)
# ============================================================================

class TimeDomainPanel(Panel):
    """Shows raw time-domain data for all 5 hydrophones."""
    
    def __init__(self, axes: list[Axes], is_workspace: bool, store: FrameStore):
        self.axes = axes
        self.is_workspace = is_workspace
        self.store = store
        self.lines: list[Line2D] = []
        
        # Precompute y-range across all frames
        if is_workspace:
            all_data = store.arrays["working_space_frames"]
        else:
            all_data = store.arrays["buffer_frames"]
        self.y_min = float(np.min(all_data))
        self.y_max = float(np.max(all_data))
        if self.y_min == self.y_max:
            self.y_max = self.y_min + 1e-12

    def plot(self) -> None:
        """Create the 5 line objects, one per hydrophone."""
        self.lines = []
        for ax in self.axes:
            line, = ax.plot([], [], linewidth=1)
            self.lines.append(line)

    def activate(self) -> None:
        """Clear axes, create artists, and show."""
        for ax in self.axes:
            ax.clear()
            ax.set_visible(True)
        self.plot()

    def deactivate(self) -> None:
        """Clear axes and hide."""
        for ax in self.axes:
            ax.clear()
            ax.set_visible(False)
        self.lines = []

    def update(self, frame_idx: int, store: FrameStore, state: AppState) -> list[Artist]:
        if self.is_workspace:
            data = store.arrays["working_space_frames"][frame_idx]
            x_len = int(data.shape[1])
            title_suffix = "Workspace"
        else:
            data = store.arrays["buffer_frames"][frame_idx]
            x_len = store.meta.block_size
            title_suffix = "Working Block"

        x = np.arange(x_len)
        artists: list[Artist] = []

        for i in range(5):
            self.lines[i].set_xdata(x)
            self.lines[i].set_ydata(data[i])
            self.axes[i].set_title(f"Hydrophone {i+1} - {title_suffix}")
            self.axes[i].set_xlim(0, x_len)
            self.axes[i].set_ylim(self.y_min, self.y_max)
            self.axes[i].set_xlabel("Sample Index")
            self.axes[i].set_ylabel("Amplitude")
            artists.append(self.lines[i])
            # Draw block boundary lines if workspace
            if self.is_workspace:
                block_size = store.meta.block_size
                ws_len = store.meta.working_space_size
                n_blocks = ws_len // block_size
                block_boundaries = [block_size * j for j in range(1, n_blocks)]
                for boundary in block_boundaries:
                    self.axes[i].axvline(boundary, color='gray', linestyle='--', linewidth=0.8, alpha=0.5, zorder=0)

        return artists


class FrequencyDomainPanel(Panel):
    """Shows FFT magnitude for all 5 hydrophones."""
    
    def __init__(self, axes: list[Axes], is_workspace: bool, store: FrameStore):
        self.axes = axes
        self.is_workspace = is_workspace
        self.store = store
        self.lines: list[Line2D] = []
        
        # Precompute y-range across all frames
        fs = store.meta.effective_sampling_rate
        if is_workspace:
            data_source = store.arrays["working_space_frames"]
        else:
            data_source = store.arrays["buffer_frames"]
        
        max_fft = 0.0
        for frame_data in data_source:
            for hydro_data in frame_data:
                fft_vals = np.abs(np.fft.fft(hydro_data)) / len(hydro_data)
                max_fft = max(max_fft, np.max(fft_vals))
        
        self.y_max = max_fft * 1.1 if max_fft > 0 else 1.0

    def plot(self) -> None:
        """Create the 5 line objects for FFT plots."""
        self.lines = []
        for ax in self.axes:
            line, = ax.plot([], [], linewidth=1)
            self.lines.append(line)

    def activate(self) -> None:
        """Clear axes, create artists, and show."""
        for ax in self.axes:
            ax.clear()
            ax.set_visible(True)
        self.plot()

    def deactivate(self) -> None:
        """Clear axes and hide."""
        for ax in self.axes:
            ax.clear()
            ax.set_visible(False)
        self.lines = []

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

            self.axes[i].clear()
            width = (plot_freqs[-1] - plot_freqs[0]) / len(plot_freqs) * 0.8 if len(plot_freqs) > 1 else 0.8
            self.axes[i].bar(plot_freqs, plot_mag, width=width)
            self.axes[i].set_title(f"Hydrophone {i+1} FFT - {title_suffix}")
            self.axes[i].set_xlim(0, fs / 2)
            self.axes[i].set_ylim(0, self.y_max)
            self.axes[i].set_xlabel("Frequency (Hz)")
            self.axes[i].set_ylabel("Magnitude")

            artists.append(self.lines[i])

        return artists


class HilbertEnvelopePanel(Panel):
    """Shows Hilbert envelope for all 5 hydrophones."""
    
    def __init__(self, axes: list[Axes], is_workspace: bool, store: FrameStore):
        self.axes = axes
        self.is_workspace = is_workspace
        self.store = store
        self.lines: list[Line2D] = []
        
        # Precompute y-range across all frames
        if is_workspace:
            data_source = store.arrays["working_space_envelope_frames"]
        else:
            data_source = store.arrays["buffer_envelope_frames"]
        
        self.y_min = float(np.min(data_source))
        self.y_max = float(np.max(data_source))
        if self.y_min == self.y_max:
            self.y_max = self.y_min + 1e-12

    def plot(self) -> None:
        """Create the 5 line objects for envelope plots."""
        self.lines = []
        for ax in self.axes:
            line, = ax.plot([], [], linewidth=1)
            self.lines.append(line)

    def activate(self) -> None:
        """Clear axes, create artists, and show."""
        for ax in self.axes:
            ax.clear()
            ax.set_visible(True)
        self.plot()

    def deactivate(self) -> None:
        """Clear axes and hide."""
        for ax in self.axes:
            ax.clear()
            ax.set_visible(False)
        self.lines = []

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

        for i in range(5):
            self.lines[i].set_xdata(x)
            self.lines[i].set_ydata(data[i])
            self.axes[i].set_title(f"Hydrophone {i+1} Envelope - {title_suffix}")
            self.axes[i].set_xlim(0, x_len)
            self.axes[i].set_ylim(self.y_min, self.y_max)
            self.axes[i].set_xlabel("Sample Index")
            self.axes[i].set_ylabel("Envelope Magnitude")
            artists.append(self.lines[i])
            # Draw block boundary lines if workspace
            if self.is_workspace:
                block_size = store.meta.block_size
                ws_len = store.meta.working_space_size
                n_blocks = ws_len // block_size
                block_boundaries = [block_size * j for j in range(1, n_blocks)]
                for boundary in block_boundaries:
                    self.axes[i].axvline(boundary, color='gray', linestyle='--', linewidth=0.8, alpha=0.5, zorder=0)

        return artists


class EnvelopeEdgePanel(Panel):
    """Shows envelope edge (imaginary part of Hilbert of envelope) for all 5 hydrophones."""
    
    def __init__(self, axes: list[Axes], is_workspace: bool, store: FrameStore):
        self.axes = axes
        self.is_workspace = is_workspace
        self.store = store
        self.lines: list[Line2D] = []
        # Use workspace envelope edge frames for both workspace and working block
        if is_workspace:
            data_source = store.arrays["working_space_envelope_edge_frames"]
        else:
            # For working block, take the middle block from workspace envelope edge frames
            ws_edge = store.arrays["working_space_envelope_edge_frames"]
            block_size = store.meta.block_size
            # Middle block is always the block at index block_size:2*block_size
            data_source = ws_edge[:, :, block_size:2*block_size]
        self.y_min = float(np.min(data_source))
        self.y_max = float(np.max(data_source))
        if self.y_min == self.y_max:
            self.y_max = self.y_min + 1e-12

    def plot(self) -> None:
        """Create the 5 line objects for envelope edge plots."""
        self.lines = []
        for ax in self.axes:
            line, = ax.plot([], [], linewidth=1)
            self.lines.append(line)

    def activate(self) -> None:
        """Clear axes, create artists, and show."""
        for ax in self.axes:
            ax.clear()
            ax.set_visible(True)
        self.plot()

    def deactivate(self) -> None:
        """Clear axes and hide."""
        for ax in self.axes:
            ax.clear()
            ax.set_visible(False)
        self.lines = []

    def update(self, frame_idx: int, store: FrameStore, state: AppState) -> list[Artist]:
        if self.is_workspace:
            data = store.arrays["working_space_envelope_edge_frames"][frame_idx]
            x_len = int(data.shape[1])
            title_suffix = "Workspace"
        else:
            ws_edge = store.arrays["working_space_envelope_edge_frames"][frame_idx]
            block_size = store.meta.block_size
            data = ws_edge[:, block_size:2*block_size]
            x_len = block_size
            title_suffix = "Working Block"

        x = np.arange(x_len)
        artists: list[Artist] = []

        for i in range(5):
            self.lines[i].set_xdata(x)
            self.lines[i].set_ydata(data[i])
            self.axes[i].set_title(f"Hydrophone {i+1} Envelope Edge - {title_suffix}")
            self.axes[i].set_xlim(0, x_len)
            self.axes[i].set_ylim(self.y_min, self.y_max)
            self.axes[i].set_xlabel("Sample Index")
            self.axes[i].set_ylabel("Envelope Edge")
            artists.append(self.lines[i])
            # Draw block boundary lines if workspace
            if self.is_workspace:
                block_size = store.meta.block_size
                ws_len = store.meta.working_space_size
                n_blocks = ws_len // block_size
                block_boundaries = [block_size * j for j in range(1, n_blocks)]
                for boundary in block_boundaries:
                    self.axes[i].axvline(boundary, color='gray', linestyle='--', linewidth=0.8, alpha=0.5, zorder=0)

        return artists


class OverlapPanel(Panel):
    """Shows all 5 hydrophones overlapped in different transform plots."""
    
    def __init__(self, axes: list[Axes], is_workspace: bool, store: FrameStore):
        self.axes = axes
        self.is_workspace = is_workspace
        self.store = store
        self.lines: list[list[Line2D]] = []
        self.colors = ['tab:blue', 'tab:orange', 'tab:green', 'tab:red', 'tab:purple']
        # Precompute y-ranges
        if is_workspace:
            raw_data_source = store.arrays["working_space_frames"]
            env_data_source = store.arrays["working_space_envelope_frames"]
            edge_data_source = store.arrays["working_space_envelope_edge_frames"]
        else:
            ws_raw = store.arrays["working_space_frames"]
            ws_env = store.arrays["working_space_envelope_frames"]
            ws_edge = store.arrays["working_space_envelope_edge_frames"]
            block_size = store.meta.block_size
            # Take middle block for working block
            raw_data_source = ws_raw[:, :, block_size:2*block_size]
            env_data_source = ws_env[:, :, block_size:2*block_size]
            edge_data_source = ws_edge[:, :, block_size:2*block_size]
        self.y_min_raw = float(np.min(raw_data_source))
        self.y_max_raw = float(np.max(raw_data_source))
        if self.y_min_raw == self.y_max_raw:
            self.y_max_raw = self.y_min_raw + 1e-12
        self.y_min_env = float(np.min(env_data_source))
        self.y_max_env = float(np.max(env_data_source))
        if self.y_min_env == self.y_max_env:
            self.y_max_env = self.y_min_env + 1e-12
        self.y_min_edge = float(np.min(edge_data_source))
        self.y_max_edge = float(np.max(edge_data_source))
        if self.y_min_edge == self.y_max_edge:
            self.y_max_edge = self.y_min_edge + 1e-12

    def plot(self) -> None:
        """Create 5 hydrophone lines for each of 4 plots + empty 5th plot."""
        self.lines = []
        for plot_idx in range(5):
            if plot_idx == 4:
                # Plot 4 will be used for legend, no lines needed
                self.lines.append([])
            else:
                ax_lines = []
                for hydro_idx in range(5):
                    line, = self.axes[plot_idx].plot([], [], alpha=0.7, linewidth=1)
                    line.set_color(self.colors[hydro_idx])
                    ax_lines.append(line)
                self.lines.append(ax_lines)

    def activate(self) -> None:
        """Clear axes, create artists, and show."""
        for ax in self.axes:
            ax.clear()
            ax.set_visible(True)
        self.plot()

    def deactivate(self) -> None:
        """Clear axes and hide."""
        for ax in self.axes:
            ax.clear()
            ax.set_visible(False)
        self.lines = []

    def update(self, frame_idx: int, store: FrameStore, state: AppState) -> list[Artist]:
        if self.is_workspace:
            raw_data = store.arrays["working_space_frames"][frame_idx]
            env_data = store.arrays["working_space_envelope_frames"][frame_idx]
            edge_data = store.arrays["working_space_envelope_edge_frames"][frame_idx]
            title_suffix = "Workspace"
        else:
            ws_raw = store.arrays["working_space_frames"][frame_idx]
            ws_env = store.arrays["working_space_envelope_frames"][frame_idx]
            ws_edge = store.arrays["working_space_envelope_edge_frames"][frame_idx]
            block_size = store.meta.block_size
            raw_data = ws_raw[:, block_size:2*block_size]
            env_data = ws_env[:, block_size:2*block_size]
            edge_data = ws_edge[:, block_size:2*block_size]
            title_suffix = "Working Block"

        x_len = int(raw_data.shape[1])
        x = np.arange(x_len)
        fs = store.meta.effective_sampling_rate
        artists: list[Artist] = []

        # Plot 0: Time domain - all hydrophones overlapped
        self.axes[0].set_title(f"All Hydrophones - Time Domain - {title_suffix}")
        self.axes[0].set_xlim(0, x_len)
        self.axes[0].set_ylim(self.y_min_raw, self.y_max_raw)
        self.axes[0].set_xlabel("Sample Index")
        self.axes[0].set_ylabel("Amplitude")
        # Draw block boundary lines if workspace
        if self.is_workspace:
            block_size = store.meta.block_size
            ws_len = store.meta.working_space_size
            n_blocks = ws_len // block_size
            block_boundaries = [block_size * j for j in range(1, n_blocks)]
            for boundary in block_boundaries:
                self.axes[0].axvline(boundary, color='gray', linestyle='--', linewidth=0.8, alpha=0.5, zorder=0)
        for i in range(5):
            self.lines[0][i].set_xdata(x)
            self.lines[0][i].set_ydata(raw_data[i])
            artists.append(self.lines[0][i])

        # Plot 1: Frequency domain - all hydrophones overlapped
        self.axes[1].set_title(f"All Hydrophones - Frequency Domain - {title_suffix}")
        self.axes[1].set_xlabel("Frequency (Hz)")
        self.axes[1].set_ylabel("Magnitude")
        max_fft = 0.0
        self.axes[1].clear()
        for i in range(5):
            fft_vals = np.fft.fft(raw_data[i])
            freqs = np.fft.fftfreq(len(raw_data[i]), d=1/fs)
            pos_mask = freqs >= 0
            plot_freqs = freqs[pos_mask]
            plot_mag = np.abs(fft_vals[pos_mask]) / len(raw_data[i])
            max_fft = max(max_fft, np.max(plot_mag))
            width = (plot_freqs[-1] - plot_freqs[0]) / len(plot_freqs) * 0.8 if len(plot_freqs) > 1 else 0.8
            self.axes[1].bar(plot_freqs, plot_mag, width=width, alpha=0.7)
        
        self.axes[1].set_xlim(0, fs / 2)
        self.axes[1].set_ylim(0, max_fft * 1.1 if max_fft > 0 else 1)

        # Plot 2: Hilbert envelope - all hydrophones overlapped
        self.axes[2].set_title(f"All Hydrophones - Hilbert Envelope - {title_suffix}")
        self.axes[2].set_xlim(0, x_len)
        self.axes[2].set_ylim(self.y_min_env, self.y_max_env)
        self.axes[2].set_xlabel("Sample Index")
        self.axes[2].set_ylabel("Envelope Magnitude")
        if self.is_workspace:
            block_size = store.meta.block_size
            ws_len = store.meta.working_space_size
            n_blocks = ws_len // block_size
            block_boundaries = [block_size * j for j in range(1, n_blocks)]
            for boundary in block_boundaries:
                self.axes[2].axvline(boundary, color='gray', linestyle='--', linewidth=0.8, alpha=0.5, zorder=0)
        for i in range(5):
            self.lines[2][i].set_xdata(x)
            self.lines[2][i].set_ydata(env_data[i])
            artists.append(self.lines[2][i])

        # Plot 3: Envelope edge - all hydrophones overlapped
        self.axes[3].set_title(f"All Hydrophones - Envelope Edge - {title_suffix}")
        self.axes[3].set_xlim(0, x_len)
        self.axes[3].set_ylim(self.y_min_edge, self.y_max_edge)
        self.axes[3].set_xlabel("Sample Index")
        self.axes[3].set_ylabel("Envelope Edge")
        if self.is_workspace:
            block_size = store.meta.block_size
            ws_len = store.meta.working_space_size
            n_blocks = ws_len // block_size
            block_boundaries = [block_size * j for j in range(1, n_blocks)]
            for boundary in block_boundaries:
                self.axes[3].axvline(boundary, color='gray', linestyle='--', linewidth=0.8, alpha=0.5, zorder=0)
        for i in range(5):
            self.lines[3][i].set_xdata(x)
            self.lines[3][i].set_ydata(edge_data[i])
            artists.append(self.lines[3][i])

        # Plot 4: Legend (horizontal layout)
        self.axes[4].clear()
        self.axes[4].axis('off')
        legend_lines = [plt.Line2D([0], [0], color=self.colors[i], lw=2) for i in range(5)]
        legend_labels = [f'Hydrophone {i+1}' for i in range(5)]
        self.axes[4].legend(
            legend_lines,
            legend_labels,
            loc='center',
            fontsize=12,
            frameon=True,
            ncol=5,
            columnspacing=1.5,
            handletextpad=0.8,
            borderaxespad=0.5
        )

        return artists


# ============================================================================
# Reference Hydrophone Panels
# ============================================================================

class DetectionPanel(Panel):
    """Detection view for reference hydrophone - uses bar plots so owns its artists."""
    
    def __init__(self, axes: list[Axes], store: FrameStore, fig: Figure):
        self.axes = axes
        self.store = store
        self.fig = fig
        # Lines for time, power, SNR plots (not bar plots)
        self.lines: list[Line2D | None] = [None, None, None, None, None]
        self.threshold_line: Line2D | None = None
        
        # Precompute y-ranges for power and SNR
        self.y_min_sig, self.y_max_sig = store.range("signal_power_frames")
        self.y_min_noi, self.y_max_noi = store.range("noise_power_frames")
        self.y_min_snr, self.y_max_snr = store.range("SNR_frames")

    def plot(self) -> None:
        """Create line artists for non-bar plots (plots 0, 2, 3, 4)."""
        for idx in [0, 2, 3, 4]:
            line, = self.axes[idx].plot([], [], linewidth=1)
            self.lines[idx] = line
        
        # Create threshold line (will be on axes[4])
        self.threshold_line = self.axes[4].axhline(0, color='red', linestyle='--', linewidth=1, alpha=0.6)

    def activate(self) -> None:
        """Clear axes, create artists, and show."""
        for ax in self.axes:
            ax.clear()
            ax.set_visible(True)
        self.plot()

    def deactivate(self) -> None:
        """Clear axes and hide."""
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

        # Plot 0: Reference buffer (time domain)
        self.lines[0].set_xdata(np.arange(meta.block_size))
        self.lines[0].set_ydata(store.arrays["reference_buffer_frames"][frame_idx])
        self.axes[0].set_title("Reference Hydrophone - Working Block")
        self.axes[0].set_xlim(0, meta.block_size)
        self.axes[0].set_ylim(adc_min, adc_max)
        self.axes[0].set_xlabel("Sample Index")
        self.axes[0].set_ylabel("Amplitude")
        artists.append(self.lines[0])

        # Plot 1: FFT bar plot (signal vs noise) - recreated each frame
        freqs = store.arrays["FFT_freqs_frames"][frame_idx]
        sig = store.arrays["FFT_signal_frames"][frame_idx]
        noi = store.arrays["FFT_noise_frames"][frame_idx]
        
        self.axes[1].clear()
        width = (freqs[-1] - freqs[0]) / len(freqs) * 0.8 if len(freqs) > 1 else 0.8
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
        self.axes[2].set_title("Signal Power Over Frames")
        self.axes[2].set_xlim(0, n_frames)
        self.axes[2].set_ylim(self.y_min_sig, self.y_max_sig)
        self.axes[2].set_xlabel("Frame Index")
        self.axes[2].set_ylabel("Power")
        artists.append(self.lines[2])

        # Plot 3: Noise power over frames
        self.lines[3].set_xdata(np.arange(n_frames))
        self.lines[3].set_ydata(store.arrays["noise_power_frames"][frame_idx])
        self.axes[3].set_title("Noise Power Over Frames")
        self.axes[3].set_xlim(0, n_frames)
        self.axes[3].set_ylim(self.y_min_noi, self.y_max_noi)
        self.axes[3].set_xlabel("Frame Index")
        self.axes[3].set_ylabel("Power")
        artists.append(self.lines[3])

        # Plot 4: SNR over frames with threshold line
        self.lines[4].set_xdata(np.arange(n_frames))
        self.lines[4].set_ydata(store.arrays["SNR_frames"][frame_idx])
        self.axes[4].set_title("SNR (dB) Over Frames")
        self.axes[4].set_xlim(0, n_frames)
        self.axes[4].set_ylim(self.y_min_snr, self.y_max_snr)
        self.axes[4].set_xlabel("Frame Index")
        self.axes[4].set_ylabel("SNR (dB)")
        
        # Threshold line at 0 dB
        self.threshold_line.set_xdata([0, n_frames])
        self.threshold_line.set_ydata([0, 0])
        self.threshold_line.set_visible(True)
        
        artists.append(self.lines[4])
        artists.append(self.threshold_line)

        return artists


class HilbertPanel(Panel):
    """Hilbert view for reference hydrophone with 3D plot."""
    
    def __init__(self, axes: list[Axes], store: FrameStore, fig: Figure):
        import scipy.signal as scpy
        self.axes = axes
        self.store = store
        self.fig = fig
        self.lines: list[Line2D | None] = [None, None, None, None, None]
        
        # Precompute y-ranges for envelope and edge
        ws_data = store.arrays["working_space_frames"]
        self.ws_len = ws_data.shape[2]
        self.y_min_raw = float(np.min(ws_data))
        self.y_max_raw = float(np.max(ws_data))
        if self.y_min_raw == self.y_max_raw:
            self.y_max_raw = self.y_min_raw + 1e-12
        
        # Compute envelope ranges
        envelope_data = []
        edge_data = []
        for frame in ws_data:
            analytic = scpy.hilbert(frame[0])  # Reference is index 0
            env = np.abs(analytic)
            envelope_data.append(env)
            
            env_analytic = scpy.hilbert(env)
            edge = env_analytic.imag
            edge_data.append(edge)
        
        envelope_data = np.array(envelope_data)
        edge_data = np.array(edge_data)
        
        self.y_min_env = float(np.min(envelope_data))
        self.y_max_env = float(np.max(envelope_data))
        if self.y_min_env == self.y_max_env:
            self.y_max_env = self.y_min_env + 1e-12
        
        self.y_min_edge = float(np.min(edge_data))
        self.y_max_edge = float(np.max(edge_data))
        if self.y_min_edge == self.y_max_edge:
            self.y_max_edge = self.y_min_edge + 1e-12

    def plot(self) -> None:
        """Create line artists for plots 0, 2, 3, 4 (plot 1 reserved)."""
        for idx in [0, 2, 3, 4]:
            line, = self.axes[idx].plot([], [], linewidth=1)
            self.lines[idx] = line

    def activate(self) -> None:
        """Clear axes, create artists, and show."""
        for ax in self.axes:
            ax.clear()
            ax.set_visible(True)
        self.plot()

    def deactivate(self) -> None:
        """Clear axes and hide."""
        for ax in self.axes:
            ax.clear()
            ax.set_visible(False)
        self.lines = [None, None, None, None, None]

    def update(self, frame_idx: int, store: FrameStore, state: AppState) -> list[Artist]:
        import scipy.signal as scpy
        
        artists: list[Artist] = []
        
        # Get workspace data for reference hydrophone (index 0)
        ws_data = store.arrays["working_space_frames"][frame_idx][0]
        ws_len = len(ws_data)
        x = np.arange(ws_len)
        
        # Compute Hilbert transform
        analytic_signal = scpy.hilbert(ws_data)
        envelope = np.abs(analytic_signal)
        phase = np.angle(analytic_signal)
        
        # Envelope edge
        envelope_analytic = scpy.hilbert(envelope)
        envelope_edge = envelope_analytic.imag

        # Plot 0: Raw time data
        self.lines[0].set_xdata(x)
        self.lines[0].set_ydata(ws_data)
        self.axes[0].set_title("Reference Hydrophone - Workspace (Time Domain)")
        self.axes[0].set_xlim(0, ws_len)
        self.axes[0].set_ylim(self.y_min_raw, self.y_max_raw)
        self.axes[0].set_xlabel("Sample Index")
        self.axes[0].set_ylabel("Amplitude")
        artists.append(self.lines[0])

        # Plot 1: Reserved (empty)
        self.axes[1].clear()
        self.axes[1].set_title("")
        self.axes[1].set_xlim(0, 1)
        self.axes[1].set_ylim(0, 1)
        self.axes[1].axis('off')

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
        self.axes[3].set_title("Magnitude of Hilbert Transform")
        self.axes[3].set_xlim(0, ws_len)
        self.axes[3].set_ylim(self.y_min_env, self.y_max_env)
        self.axes[3].set_xlabel("Sample Index")
        self.axes[3].set_ylabel("Magnitude")
        artists.append(self.lines[3])

        # Plot 4: Envelope edge
        self.lines[4].set_xdata(x)
        self.lines[4].set_ydata(envelope_edge)
        self.axes[4].set_title("Envelope Edge (Imag of Hilbert of Magnitude)")
        self.axes[4].set_xlim(0, ws_len)
        self.axes[4].set_ylim(self.y_min_edge, self.y_max_edge)
        self.axes[4].set_xlabel("Sample Index")
        self.axes[4].set_ylabel("Envelope Edge")
        artists.append(self.lines[4])

        block_size = store.meta.block_size
        ws_len = store.meta.working_space_size
        n_blocks = ws_len // block_size
        block_boundaries = [block_size * j for j in range(1, n_blocks)]
        for boundary in block_boundaries:
            self.axes[0].axvline(boundary, color='gray', linestyle='--', linewidth=0.8, alpha=0.5, zorder=0)
            self.axes[2].axvline(boundary, color='gray', linestyle='--', linewidth=0.8, alpha=0.5, zorder=0)
            self.axes[3].axvline(boundary, color='gray', linestyle='--', linewidth=0.8, alpha=0.5, zorder=0)
            self.axes[4].axvline(boundary, color='gray', linestyle='--', linewidth=0.8, alpha=0.5, zorder=0)

        return artists


class Hilbert3DPanel(Panel):
    """Dedicated 3D Hilbert view for reference hydrophone (full-size plot)."""

    def __init__(self, axes: list[Axes], store: FrameStore, fig: Figure):
        import scipy.signal as scpy
        self.axes = axes
        self.store = store
        self.fig = fig
        self.ax_3d: Axes3D | None = None
        self.base_axes: list[Axes] = axes

        ws_data = store.arrays["working_space_frames"]
        self.ws_len = ws_data.shape[2]
        self.y_min_raw = float(np.min(ws_data))
        self.y_max_raw = float(np.max(ws_data))
        if self.y_min_raw == self.y_max_raw:
            self.y_max_raw = self.y_min_raw + 1e-12

        envelope_data = []
        edge_data = []
        for frame in ws_data:
            analytic = scpy.hilbert(frame[0])
            env = np.abs(analytic)
            envelope_data.append(env)
            env_analytic = scpy.hilbert(env)
            edge = env_analytic.imag
            edge_data.append(edge)

        envelope_data = np.array(envelope_data)
        edge_data = np.array(edge_data)
        self.y_min_env = float(np.min(envelope_data))
        self.y_max_env = float(np.max(envelope_data))
        if self.y_min_env == self.y_max_env:
            self.y_max_env = self.y_min_env + 1e-12

        self.y_min_edge = float(np.min(edge_data))
        self.y_max_edge = float(np.max(edge_data))
        if self.y_min_edge == self.y_max_edge:
            self.y_max_edge = self.y_min_edge + 1e-12

    def plot(self) -> None:
        # No line artists needed; 3D plot is drawn directly.
        return None

    def activate(self) -> None:
        # Hide all base axes to give full space to the 3D plot.
        for ax in self.axes:
            ax.clear()
            ax.set_visible(False)

        if self.ax_3d is None or self.ax_3d not in self.fig.axes:
            self.ax_3d = self.fig.add_subplot(1, 1, 1, projection='3d')
        else:
            self.ax_3d.set_visible(True)

        self.ax_3d.set_xlim3d(0, self.ws_len - 1)
        self.ax_3d.set_ylim3d(self.y_min_env, self.y_max_env)
        self.ax_3d.set_zlim3d(self.y_min_edge, self.y_max_edge)

    def deactivate(self) -> None:
        if self.ax_3d is not None:
            self.ax_3d.clear()
            self.ax_3d.set_visible(False)

        for ax in self.axes:
            ax.clear()
            ax.set_visible(False)

    def update(self, frame_idx: int, store: FrameStore, state: AppState) -> list[Artist]:
        import scipy.signal as scpy

        ws_data = store.arrays["working_space_frames"][frame_idx][0]
        ws_len = len(ws_data)
        x = np.arange(ws_len)

        analytic_signal = scpy.hilbert(ws_data)

        if self.ax_3d is None or self.ax_3d not in self.fig.axes:
            self.ax_3d = self.fig.add_subplot(1, 1, 1, projection='3d')

        self.ax_3d.clear()
        self.ax_3d.plot(x, analytic_signal.real, analytic_signal.imag, linewidth=0.5, color='tab:blue')
        self.ax_3d.set_title("3D Hilbert Transform (Real, Imag vs Time)")
        self.ax_3d.set_xlabel("Sample Index")
        self.ax_3d.set_ylabel("Real Part")
        self.ax_3d.set_zlabel("Imaginary Part")
        self.ax_3d.set_xlim3d(0, ws_len - 1)
        self.ax_3d.set_ylim3d(self.y_min_env, self.y_max_env)
        self.ax_3d.set_zlim3d(self.y_min_edge, self.y_max_edge)

        return []

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

        # Build all panels - they now own their artists
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
        """Build all panel instances - pass store and fig to each."""
        self.panels: dict[tuple[MainMode, SubMode], Panel] = {}
        
        # Workspace panels
        self.panels[(MainMode.WORKSPACE, SubMode.TIME_DOMAIN)] = TimeDomainPanel(
            self.axes, is_workspace=True, store=self.store
        )
        self.panels[(MainMode.WORKSPACE, SubMode.FREQUENCY_DOMAIN)] = FrequencyDomainPanel(
            self.axes, is_workspace=True, store=self.store
        )
        self.panels[(MainMode.WORKSPACE, SubMode.HILBERT_ENVELOPE)] = HilbertEnvelopePanel(
            self.axes, is_workspace=True, store=self.store
        )
        self.panels[(MainMode.WORKSPACE, SubMode.ENVELOPE_EDGE)] = EnvelopeEdgePanel(
            self.axes, is_workspace=True, store=self.store
        )
        self.panels[(MainMode.WORKSPACE, SubMode.OVERLAP)] = OverlapPanel(
            self.axes, is_workspace=True, store=self.store
        )
        
        # Working Block panels
        self.panels[(MainMode.WORKING_BLOCK, SubMode.TIME_DOMAIN)] = TimeDomainPanel(
            self.axes, is_workspace=False, store=self.store
        )
        self.panels[(MainMode.WORKING_BLOCK, SubMode.FREQUENCY_DOMAIN)] = FrequencyDomainPanel(
            self.axes, is_workspace=False, store=self.store
        )
        self.panels[(MainMode.WORKING_BLOCK, SubMode.HILBERT_ENVELOPE)] = HilbertEnvelopePanel(
            self.axes, is_workspace=False, store=self.store
        )
        self.panels[(MainMode.WORKING_BLOCK, SubMode.ENVELOPE_EDGE)] = EnvelopeEdgePanel(
            self.axes, is_workspace=False, store=self.store
        )
        self.panels[(MainMode.WORKING_BLOCK, SubMode.OVERLAP)] = OverlapPanel(
            self.axes, is_workspace=False, store=self.store
        )
        
        # Reference panels - pass fig for 3D axes in HilbertPanel
        self.panels[(MainMode.REFERENCE, SubMode.DETECTION)] = DetectionPanel(
            self.axes, store=self.store, fig=self.fig
        )
        self.panels[(MainMode.REFERENCE, SubMode.HILBERT)] = HilbertPanel(
            self.axes, store=self.store, fig=self.fig
        )
        self.panels[(MainMode.REFERENCE, SubMode.HILBERT_3D)] = Hilbert3DPanel(
            self.axes, store=self.store, fig=self.fig
        )

    def _setup_buttons(self) -> None:
        """Setup button interface with state machine logic."""
        # Always-visible buttons
        ax_play = plt.axes([0.02, 0.02, 0.08, 0.04])
        ax_skip_back = plt.axes([0.11, 0.02, 0.05, 0.04])
        ax_skip_fwd = plt.axes([0.17, 0.02, 0.05, 0.04])
        ax_save = plt.axes([0.23, 0.02, 0.08, 0.04])

        # Main mode buttons
        ax_workspace = plt.axes([0.34, 0.02, 0.08, 0.04])
        ax_working = plt.axes([0.43, 0.02, 0.08, 0.04])
        ax_reference = plt.axes([0.52, 0.02, 0.08, 0.04])

        # Add extra space before sub-mode buttons
        ax_sub1 = plt.axes([0.63, 0.02, 0.06, 0.04])
        ax_sub2 = plt.axes([0.70, 0.02, 0.06, 0.04])
        ax_sub3 = plt.axes([0.77, 0.02, 0.06, 0.04])
        ax_sub4 = plt.axes([0.84, 0.02, 0.06, 0.04])
        ax_sub5 = plt.axes([0.91, 0.02, 0.06, 0.04])

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
            if self.state.paused:
                self.ani.event_source.stop()
            else:
                self.ani.event_source.start()

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
            sub_modes = [SubMode.DETECTION, SubMode.HILBERT, SubMode.HILBERT_3D]
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
            labels = ["Detect", "Hilbert", "Hilbert 3D"]
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
        # Only advance frame if not paused
        if not self.state.paused:
            if frame_idx >= len(self.store) - 1:
                self.state.current_frame = len(self.store) - 1
                self.state.paused = True
            else:
                self.state.current_frame = frame_idx
        # Always use self.state.current_frame for display and update
        display_frame = int(self.state.current_frame)

        panel_key = (self.state.main_mode, self.state.sub_mode)
        self._set_active_panel(panel_key)
        artists = self.panels[panel_key].update(display_frame, self.store, self.state)

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
            SubMode.HILBERT: "Hilbert Transform",
            SubMode.HILBERT_3D: "Hilbert Transform (3D)"
        }

        title = f"{main_mode_names[self.state.main_mode]} - {sub_mode_names[self.state.sub_mode]}"
        meta = self.store.meta
        self.fig.suptitle(
            f"{title} | Frame {display_frame}/{len(self.store) - 1} "
            f"(Sample {meta.warmup_samples + (display_frame + 1) * meta.frame_skip - 1}) [{status}]"
        )

        return artists

    def show(self) -> None:
        plt.show()


def launch_gui(store: FrameStore, *, desired_fps: int = 24) -> None:
    app = GuiApp(store=store, desired_fps=desired_fps)
    app.show()