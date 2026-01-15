import pandas as pd
import matplotlib.pyplot as plt

# List of hydrophone data files
files = [f'hydrophone_{i}_data.csv' for i in range(1, 6)]

# Create subplots
fig, axes = plt.subplots(5, 1, figsize=(10, 12), sharex=True)
fig.suptitle('Hydrophone Signals Over Time')

for i, file in enumerate(files):
    # Read the CSV
    data = pd.read_csv(file)
    time = data['time']
    signal = data['signal']

    # Plot
    axes[i].plot(time, signal)
    axes[i].set_title(f'Hydrophone {i+1}')
    axes[i].set_ylabel('Signal Amplitude')
    if i == 4:
        axes[i].set_xlabel('Time (s)')

plt.tight_layout()
plt.show()
plt.savefig('hydrophone_signals.png', dpi=300)
print("Plot saved as hydrophone_signals.png")