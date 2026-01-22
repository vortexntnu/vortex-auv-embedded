# Source - https://stackoverflow.com/a
# Posted by jlandercy, modified by community. See post 'Timeline' for change history
# Retrieved 2026-01-22, License - CC BY-SA 4.0

import numpy as np
from scipy import fft

fs = 12000
t = 505/fs
f = 500
samples = np.arange(t * fs) / fs
signal = np.sin(2 * np.pi * f * samples)

# Source - https://stackoverflow.com/a
# Posted by jlandercy, modified by community. See post 'Timeline' for change history
# Retrieved 2026-01-22, License - CC BY-SA 4.0

np.random.seed(12345)
noise = 0.25*np.random.normal(size=signal.size)

# Source - https://stackoverflow.com/a
# Posted by jlandercy, modified by community. See post 'Timeline' for change history
# Retrieved 2026-01-22, License - CC BY-SA 4.0

def rms(x):
    return np.sqrt(np.sum(x**2)/x.size)

SNR = (rms(signal)/rms(noise))**2  # 7.76816527364244

# Source - https://stackoverflow.com/a
# Posted by jlandercy, modified by community. See post 'Timeline' for change history
# Retrieved 2026-01-22, License - CC BY-SA 4.0

dt = np.diff(samples)[0]  # 1/f
G = fft.fft(signal + noise)
freq = fft.fftfreq(G.size, dt)

# Source - https://stackoverflow.com/a
# Posted by jlandercy, modified by community. See post 'Timeline' for change history
# Retrieved 2026-01-22, License - CC BY-SA 4.0

q = np.abs(np.abs(freq) - f) <= 20.
F = np.zeros(G.size)
F[q] = 1.

# Source - https://stackoverflow.com/a
# Posted by jlandercy, modified by community. See post 'Timeline' for change history
# Retrieved 2026-01-22, License - CC BY-SA 4.0

Gf = G*F
Nf = G*(1-F)
