import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
import matplotlib.ticker as ticker
import matplotlib.widgets as mwidgets


def _fft_amp_db(time: np.ndarray, signal: np.ndarray) -> tuple[np.ndarray, np.ndarray]:
    dt = float(time[1] - time[0])
    n = int(len(signal))
    freq = np.fft.rfftfreq(n, d=dt)
    freq = freq[freq > 0] # Exclude DC component
    fft_values = np.fft.rfft(signal)
    fft_values = fft_values[1:] # Exclude DC component
    amplitude = np.abs(fft_values) / n
    dB_amplitude = 20 * np.log10(amplitude + np.finfo(float).tiny)
    return freq, dB_amplitude

# List of hydrophone data files
files = [f'hydrophone_{i}_data.csv' for i in range(1, 6)]

# Create subplots
fig, axes = plt.subplots(5, 1, figsize=(10, 10), sharex=False)
fig.subplots_adjust(hspace=0.05, bottom=0.18, top=0.93, left=0.12, right=0.97)
default_positions = [ax.get_position() for ax in axes]

# Sync zoom/pan across time-domain plots
current_mode = 'Time Domain'
_syncing_xlim = False


def _on_xlim_changed(changed_ax):
    global _syncing_xlim
    if _syncing_xlim:
        return
    if current_mode != 'Time Domain':
        return
    if not changed_ax.get_visible():
        return

    _syncing_xlim = True
    xlim = changed_ax.get_xlim()
    for ax in axes:
        if ax is changed_ax or not ax.get_visible():
            continue
        ax.set_xlim(xlim)
    fig.canvas.draw_idle()
    _syncing_xlim = False


def _on_button_release(event):
    global _syncing_xlim
    if _syncing_xlim:
        return
    if current_mode != 'Time Domain':
        return
    if event.inaxes not in axes:
        return

    _syncing_xlim = True
    xlim = event.inaxes.get_xlim()
    for ax in axes:
        if ax is event.inaxes or not ax.get_visible():
            continue
        ax.set_xlim(xlim)
    fig.canvas.draw_idle()
    _syncing_xlim = False


for ax in axes:
    ax.callbacks.connect('xlim_changed', _on_xlim_changed)
fig.canvas.mpl_connect('button_release_event', _on_button_release)
try:
    supylabel = fig.supylabel('')
except AttributeError:
    supylabel = fig.text(0.04, 0.5, '', va='center', rotation='vertical')

# Add buttons
button_ax1 = plt.axes([0.01, 0.02, 0.19, 0.05])
button1 = mwidgets.Button(button_ax1, 'Time Domain')
button_ax2 = plt.axes([0.21, 0.02, 0.19, 0.05])
button2 = mwidgets.Button(button_ax2, 'FFT Amplitude (dB)')
button_ax4 = plt.axes([0.41, 0.02, 0.19, 0.05])
button4 = mwidgets.Button(button_ax4, 'Hydrophone 1 Only')
button_ax5 = plt.axes([0.61, 0.02, 0.19, 0.05])
button5 = mwidgets.Button(button_ax5, 'Overlay All')

def on_click1(event):
    plot_mode('Time Domain')

def on_click2(event):
    plot_mode('FFT Amplitude (dB)')

def on_click4(event):
    plot_mode('Hydrophone 1 Only')

def on_click5(event):
    plot_mode('Overlay All Hydrophones')

button1.on_clicked(on_click1)
button2.on_clicked(on_click2)
button4.on_clicked(on_click4)
button5.on_clicked(on_click5)

def plot_mode(mode):
    global current_mode
    current_mode = mode
    for i, ax in enumerate(axes):
        ax.set_visible(True)
        ax.set_position(default_positions[i])
    # Clear first (and reset log axes) before drawing
    for i in range(5):
        if axes[i].get_xscale() == 'log':
            axes[i].set_xscale('linear')
        axes[i].clear()

    if mode == 'Hydrophone 1 Only':
        supylabel.set_visible(False)
        data = pd.read_csv(files[0])
        time = data['time'].to_numpy()
        signal = data['signal'].to_numpy()

        top = 0.93
        bottom = 0.18
        hspace = 0.07
        available = top - bottom
        height = (available - hspace) / 2
        y0 = top - height
        y1 = y0 - hspace - height

        axes[0].set_position([0.125, y0, 0.775, height])
        axes[1].set_position([0.125, y1, 0.775, height])

        axes[0].plot(time, signal, label='Hydrophone 1')
        axes[0].set_ylabel('Voltage (V)')
        axes[0].legend()
        axes[0].set_xlabel('Time (s)')

        freq_plot, dB_amplitude_plot = _fft_amp_db(time, signal)
        axes[1].plot(freq_plot, dB_amplitude_plot, label='Hydrophone 1')
        axes[1].set_xscale('log')
        axes[1].xaxis.set_minor_locator(ticker.LogLocator(subs=(2,3,4,5,6,7,8,9)))
        #axes[1].set_ylim(bottom=dB_amplitude_plot[0])
        axes[1].set_ylabel('Amplitude (dB)')
        axes[1].legend()
        axes[1].set_xlabel('Frequency (Hz)')

        axes[0].grid(True, which='both')
        axes[1].grid(True, which='both')
        axes[2].set_visible(False)
        axes[3].set_visible(False)
        axes[4].set_visible(False)

        fig.suptitle(f'Hydrophone Signals - {mode}')
        plt.draw()
        return

    if mode == 'Overlay All Hydrophones':
        supylabel.set_visible(False)

        top = 0.93
        bottom = 0.18
        hspace = 0.07
        available = top - bottom
        height = (available - hspace) / 2
        y0 = top - height
        y1 = y0 - hspace - height

        axes[0].set_position([0.125, y0, 0.775, height])
        axes[1].set_position([0.125, y1, 0.775, height])

        # Load all hydrophones once
        times = []
        signals = []
        for file in files:
            d = pd.read_csv(file)
            times.append(d['time'].to_numpy())
            signals.append(d['signal'].to_numpy())

        for i in range(5):
            axes[0].plot(times[i], signals[i], label=f'Hydrophone {i+1}')
        axes[0].set_ylabel('Voltage (V)')
        axes[0].set_xlabel('Time (s)')
        axes[0].grid(True, which='both')
        axes[0].legend(loc='upper right', ncol=2)

        for i in range(5):
            f, a = _fft_amp_db(times[i], signals[i])
            axes[1].plot(f, a, label=f'Hydrophone {i+1}')
        axes[1].set_xscale('log')
        axes[1].xaxis.set_minor_locator(ticker.LogLocator(subs=(2,3,4,5,6,7,8,9)))
        axes[1].set_ylabel('Amplitude (dB)')
        axes[1].set_xlabel('Frequency (Hz)')
        axes[1].grid(True, which='both')
        axes[1].legend(loc='upper right', ncol=2)

        axes[2].set_visible(False)
        axes[3].set_visible(False)
        axes[4].set_visible(False)

        fig.suptitle(f'Hydrophone Signals - {mode}')
        plt.draw()
        return

    supylabel.set_visible(True)
    if mode == 'Time Domain':
        supylabel.set_text('Voltage (V)')

        times = []
        signals = []
        for file in files:
            d = pd.read_csv(file)
            times.append(d['time'].to_numpy())
            signals.append(d['signal'].to_numpy())

        t_min = min(t.min() for t in times)
        t_max = max(t.max() for t in times)
        y_min = min(s.min() for s in signals)
        y_max = max(s.max() for s in signals)
        pad = 0.05 * (y_max - y_min) if y_max > y_min else 1.0

        for i in range(5):
            axes[i].plot(times[i], signals[i], label=f'Hydrophone {i+1}')
            axes[i].legend(loc='upper right')
            axes[i].set_xlim(t_min, t_max)
            axes[i].set_ylim(y_min - pad, y_max + pad)
            axes[i].grid(True, which='both')
            if i == 4:
                axes[i].set_xlabel('Time (s)')

    elif mode == 'FFT Amplitude (dB)':
        supylabel.set_text('Amplitude (dB)')
        for i in range(5):
            data = pd.read_csv(files[i])
            time = data['time'].to_numpy()
            signal = data['signal'].to_numpy()
            freq_plot, dB_amplitude_plot = _fft_amp_db(time, signal)

            axes[i].plot(freq_plot, dB_amplitude_plot, label=f'Hydrophone {i+1}')
            axes[i].set_xscale('log')
            axes[i].xaxis.set_minor_locator(ticker.LogLocator(subs=(2,3,4,5,6,7,8,9)))
            axes[i].legend(loc='upper right')
            axes[i].grid(True, which='both')
            if i == 4:
                axes[i].set_xlabel('Frequency (Hz)')

    fig.suptitle(f'Hydrophone Signals - {mode}')
    plt.draw()

# Initial plot
plot_mode('Time Domain')

plt.show()