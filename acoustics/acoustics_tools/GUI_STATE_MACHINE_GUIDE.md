# GUI State Machine Architecture Guide

## Overview

The GUI has been completely overhauled with a state machine architecture featuring three main modes, each with their own sub-modes.

## Main Modes

### 1. **Workspace View** (`MainMode.WORKSPACE`)
Shows data from the full 3-block workspace window.

### 2. **Working Block View** (`MainMode.WORKING_BLOCK`)
Shows data from a single working block (detection window).

### 3. **Reference Hydrophone View** (`MainMode.REFERENCE`)
Shows detailed analysis of the reference hydrophone (hydrophone 1).

## Sub-Modes

### For Workspace & Working Block Views

Both main modes share these 5 sub-modes:

1. **Time Domain** (`SubMode.TIME_DOMAIN`)
   - Shows raw time-domain signals for all 5 hydrophones
   - Separate plot for each hydrophone

2. **Frequency Domain** (`SubMode.FREQUENCY_DOMAIN`)
   - Shows FFT magnitude spectrum for all 5 hydrophones
   - Only positive frequencies displayed
   - Separate plot for each hydrophone

3. **Hilbert Envelope** (`SubMode.HILBERT_ENVELOPE`)
   - Shows the Hilbert envelope (magnitude of analytic signal) for all 5 hydrophones
   - Separate plot for each hydrophone

4. **Envelope Edge** (`SubMode.ENVELOPE_EDGE`)
   - Shows the imaginary part of the Hilbert transform of the envelope
   - Used for edge detection in the algorithm
   - Separate plot for each hydrophone

5. **Overlap** (`SubMode.OVERLAP`)
   - All 5 hydrophones overlapped in each plot
   - 5 plots showing:
     - Plot 1: Time domain (all hydrophones overlapped)
     - Plot 2: Frequency domain (all hydrophones overlapped)
     - Plot 3: Hilbert envelope (all hydrophones overlapped)
     - Plot 4: Envelope edge (all hydrophones overlapped)
     - Plot 5: Legend

### For Reference Hydrophone View

1. **Detection** (`SubMode.DETECTION`) - Default when entering Reference mode
   - Plot 1: Reference hydrophone working block (raw time data)
   - Plot 2: FFT bar plot showing signal bins (blue) vs noise bins (orange)
   - Plot 3: Signal power over frames
   - Plot 4: Noise power over frames
   - Plot 5: SNR (dB) over frames with threshold line at 0 dB

2. **Hilbert** (`SubMode.HILBERT`)
   - Plot 1: Reference hydrophone workspace (raw time data)
   - Plot 2: 3D plot of complex Hilbert transform (Real vs Imag vs Time)
   - Plot 3: Phase of Hilbert transform
   - Plot 4: Magnitude of Hilbert transform (envelope)
   - Plot 5: Envelope edge

## Button Layout

### Always-Visible Buttons (Left Section)
- **Play/Pause**: Toggle animation playback
- **◄ (Skip Back)**: Go back one frame
- **► (Skip Forward)**: Go forward one frame
- **Save GIF**: Export animation as GIF

### Main Mode Buttons (Center Section)
- **Workspace**: Switch to Workspace main mode
- **Working**: Switch to Working Block main mode
- **Reference**: Switch to Reference Hydrophone main mode

### Sub-Mode Buttons (Right Section)
Dynamic buttons that change based on current main mode:

**When in Workspace or Working Block:**
- **Time**: Time domain view
- **Freq**: Frequency domain view
- **Env**: Hilbert envelope view
- **Edge**: Envelope edge view
- **Overlap**: Overlap view

**When in Reference:**
- **Detect**: Detection view
- **Hilbert**: Hilbert transform view
- (Remaining buttons hidden)

## State Machine Rules

1. **Navigation**:
   - You can always switch between main modes
   - Sub-mode buttons only allow switching within the current main mode
   - Switching main modes resets to the default sub-mode:
     - Workspace/Working Block → Time Domain
     - Reference → Detection

2. **Control Buttons**:
   - Skip forward/backward, pause/play, and save GIF are always active
   - Animation pauses when switching views

3. **Button Visibility**:
   - Sub-mode buttons dynamically update based on current main mode
   - Unused sub-mode buttons are hidden in Reference mode

## Panel Classes

Each view combination (main mode + sub-mode) has its own Panel class:

### Workspace/Working Block Panels
- `TimeDomainPanel`: Raw signal display
- `FrequencyDomainPanel`: FFT magnitude display
- `HilbertEnvelopePanel`: Envelope display
- `EnvelopeEdgePanel`: Envelope edge display
- `OverlapPanel`: Multi-view overlay display

### Reference Panels
- `DetectionPanel`: Detection algorithm visualization
- `HilbertPanel`: Advanced Hilbert transform analysis with 3D plot

## Code Structure

```python
# State tracking
class AppState:
    main_mode: MainMode
    sub_mode: SubMode
    paused: bool
    current_frame: int

# Panel management
panels: dict[tuple[MainMode, SubMode], Panel]

# Active panel switching
_set_active_panel(key: tuple[MainMode, SubMode])
```

## Usage Example

1. Start application → Default: Working Block / Time Domain
2. Click "Reference" → Switches to: Reference / Detection
3. Click "Hilbert" → Switches to: Reference / Hilbert
4. Click "Workspace" → Switches to: Workspace / Time Domain
5. Click "Overlap" → Switches to: Workspace / Overlap
6. Click "Working" → Switches to: Working Block / Time Domain

## Testing

To test the new GUI:

```bash
python fullstack_prototype.py --config simulation_config.json --tdoa_method envelope_envelope
```

The GUI will start in Working Block / Time Domain mode. Use the buttons to navigate between different views.
