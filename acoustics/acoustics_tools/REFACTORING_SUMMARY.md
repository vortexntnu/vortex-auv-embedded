# GUI Refactoring Implementation Summary

## What Was Fixed

### Problem
The original GUI implementation shared mutable `Line2D` objects across panels. When switching between fundamentally different plot types (e.g., from line plots to bar plots), the old artists remained on axes causing visual glitches and breaking the animation.

### Root Cause
- All panels shared `self.lines` and `self.overlap_lines` created in `GuiApp.__init__()`
- `deactivate()` methods were essentially no-ops, not actually removing artists
- Y-axis ranges were computed per-frame, causing jitter
- Bar plots tried to coexist with Line2D objects on same axes

### Solution Implemented

#### 1. **Each Panel Now Owns Its Artists**
```python
class TimeDomainPanel(Panel):
    def __init__(self, axes, is_workspace, store):
        self.lines = []  # Panel owns its lines
        # Precompute y-range across ALL frames
        self.y_min = ...
        self.y_max = ...
    
    def plot(self):
        """Create Line2D objects - called once during activate()"""
        self.lines = [ax.plot([], [])[0] for ax in self.axes]
    
    def activate(self):
        """When switching TO this panel"""
        for ax in self.axes:
            ax.clear()  # Remove old artists
            ax.set_visible(True)
        self.plot()  # Create fresh artists
    
    def deactivate(self):
        """When switching FROM this panel"""
        for ax in self.axes:
            ax.clear()  # Remove artists
            ax.set_visible(False)
        self.lines = []  # Clear references
    
    def update(self, frame_idx, ...):
        """Modify data only - don't create new artists"""
        for i in range(5):
            self.lines[i].set_xdata(...)
            self.lines[i].set_ydata(...)
```

#### 2. **Precomputed Y-Axis Ranges**
Each panel computes min/max once in `__init__()` across ALL frames:
```python
# NOT computed per-frame in update()
if is_workspace:
    data_source = store.arrays["working_space_frames"]  # All frames!
else:
    data_source = store.arrays["buffer_frames"]         # All frames!

self.y_min = float(np.min(data_source))
self.y_max = float(np.max(data_source))
```

#### 3. **Special Handling for Complex Panels**

**OverlapPanel**: Creates 5 lines per axis (for 5 hydrophones)
```python
def plot(self):
    self.lines = []
    for plot_idx in range(5):
        ax_lines = []
        for hydro_idx in range(5):
            line, = self.axes[plot_idx].plot([], [])
            ax_lines.append(line)
        self.lines.append(ax_lines)
```

**DetectionPanel**: Handles bar charts (recreated each frame, not pre-created)
```python
def plot(self):
    # Only create lines for non-bar plots (0, 2, 3, 4)
    for idx in [0, 2, 3, 4]:
        line, = self.axes[idx].plot([], [])
        self.lines[idx] = line

def update(self, ...):
    # Bar plot (axis 1) recreated each frame
    self.axes[1].clear()
    self.axes[1].bar(freqs, signal, ...)
```

**HilbertPanel**: Handles 3D axis substitution
```python
def update(self, ...):
    # Plot 1 needs 3D axes
    self.axes[1].clear()  # Remove old 2D axis
    self.ax_3d = self.fig.add_subplot(5, 1, 2, projection='3d')
    self.axes[1] = self.ax_3d  # Replace with 3D axis
    self.ax_3d.plot(...)
```

#### 4. **GuiApp Cleanup**
Removed from `__init__()`:
- Pre-created `self.lines`
- Pre-created `self.overlap_lines`
- Pre-created `self.snr_threshold_line`
- Placeholder objects like `self.bar_container`, `self.scatter_3d`

Panels now receive `store` and `fig` in constructor:
```python
panel = TimeDomainPanel(self.axes, is_workspace=True, store=self.store)
panel = DetectionPanel(self.axes, store=self.store, fig=self.fig)
```

---

## Architecture Improvements

| Aspect | Before | After |
|--------|--------|-------|
| Artist ownership | Shared globally | Each panel owns |
| Artist lifecycle | Created once, never cleared | Created in `activate()`, cleared in `deactivate()` |
| Y-axis ranges | Per-frame computation | Precomputed in `__init__()` |
| Panel switching | Visual glitches | Clean slate each time |
| Bar chart handling | Broken (conflicts with lines) | Proper `clear()` and recreation |
| 3D axes | Attempted in `plot()` | Properly created in `update()` |
| Code clarity | Unclear data flow | Clear `plot`→`activate`→`update`→`deactivate` flow |

---

## Testing the Implementation

### Basic Sanity Check
```python
python fullstack_prototype.py --config simulation_config.json --tdoa_method envelope_envelope
```

### Test Progression (simplest to most complex)
1. **Time → Frequency** (both line plots)
   - Tests basic `plot()`/`activate()`/`deactivate()` flow

2. **Frequency → Hilbert Envelope** (line plots, different data)
   - Tests y-range precomputation

3. **Envelope → Overlap** (multiple lines per axis)
   - Tests special line creation logic

4. **Overlap → Detection** (bar charts)
   - Tests bar chart clear/recreate logic

5. **Detection → Hilbert Reference** (3D axes)
   - Tests 3D axis substitution

6. **Reference Hilbert → Workspace Time** (full reset)
   - Tests clean switching between fundamentally different data/axes

### What to Look For
- ✅ No ghosted/overlapped plots
- ✅ Y-axis consistent across frame changes
- ✅ Smooth animation without flicker
- ✅ Bar charts visible and clear
- ✅ 3D plot renders correctly
- ✅ Legends appear properly
- ✅ Button state updates correctly

---

## If Issues Persist

**See [DEBUGGING_GUIDE.md](DEBUGGING_GUIDE.md)** for detailed troubleshooting organized by:
- Common symptoms and solutions
- Where to look in the code
- Data structure reference
- Quick debugging checklist

### Quick Reference for Issues

| Symptom | Likely Cause | Check |
|---------|-------------|-------|
| Ghosted plots | Missing `ax.clear()` | Panel's `activate()` |
| Jumping Y-axis | Per-frame range computation | Panel's `__init__()` |
| No FFT/Overlap lines | Wrong line creation count | Panel's `plot()` method |
| Bar chart invisible | Missing `axes[1].clear()` | Detection's `update()` |
| 3D plot flat/broken | 3D axis not created properly | HilbertPanel's `update()` |

---

## Code Structure

```
Panel (base class)
├── plot() - Create artists
├── activate() - Show and create
├── deactivate() - Clear and hide
└── update() - Modify data

TimeDomainPanel (and similar)
├── __init__() - Precompute ranges
├── plot() - Create 5 Line2D objects
├── activate() - Clear, create, show
├── deactivate() - Clear, hide
└── update() - Update data

OverlapPanel
├── __init__() - Precompute 4 ranges
├── plot() - Create 5×5 Line2D objects
└── update() - Update all 20 lines

DetectionPanel
├── __init__() - Precompute 3 ranges
├── plot() - Create lines for plots 0,2,3,4
├── activate() - Clear, create, show
└── update() - Update lines AND recreate bars

HilbertPanel
├── __init__() - Precompute ranges
├── plot() - Create 4 Line2D objects
├── activate() - Clear, create, show
└── update() - Update lines AND create 3D axis

GuiApp
├── _build_panels() - Create all 13 panel instances
├── _set_active_panel() - Switch between panels
└── _animate() - Call panel.update() with precomputed ranges
```

---

## Files Modified

- `fullstack_gui.py` - Complete refactoring of Panel classes and GuiApp
- `DEBUGGING_GUIDE.md` - Created for troubleshooting (this file)
- `GUI_STATE_MACHINE_GUIDE.md` - Existing state machine documentation (no changes needed)

