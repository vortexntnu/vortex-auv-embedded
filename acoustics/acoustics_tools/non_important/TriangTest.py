import numpy as np
import matplotlib.pyplot as plt

# Speed of sound (meters per second)
c = 343  # in air at 20°C

# Microphone positions (in meters)
S = np.array([
    [0, 0, 0],  # S1 (reference)
    [1, 0, 0],  # S2
    [0, 1, 0],  # S3
    [0, 0, 1]   # S4
])

# True sound source position (for testing)
Q_true = np.array([2, 3, 4])

# Compute actual distances from source to each microphone
distances = np.linalg.norm(S - Q_true, axis=1)

# Compute time of arrival (TOA) at each microphone
T = distances / c  # Convert distances to times

# Select S1 as the reference microphone
T_ref = T[0]
T_diff = T[1:] - T_ref  # Compute time differences
D_diff = T_diff * c      # Convert to distance differences

# Construct A matrix
A = 2 * (S[1:] - S[0])  # Differences in microphone positions

# Construct B vector correctly
B = D_diff**2 - np.sum(S[1:]**2, axis=1) + np.sum(S[0]**2)

# Solve using least squares
Q_est, _, _, _ = np.linalg.lstsq(A, B, rcond=None)

print("Estimated Source Position:", Q_est)
print("True Source Position:", Q_true)
