import numpy as np
from functions import TDOA_direction_solve

# Hydrophone positions
R = np.array([
    [0,0,0],
    [1,0,0],
    [0,1,0],
    [0,0,1],
    [1,1,1]
])

n = 1000
error_list = np.zeros(n)
for i in range(1000):
    # True source
    s_true = np.array([0.3, 0.4, 0.2])*100

    c = 1500.0  # speed of sound

    # Generate arrival times
    t = np.linalg.norm(R - s_true, axis=1) / c

    # Add noise
    t += np.random.normal(0, 10**-4, size=len(t))

    #solve
    p = TDOA_direction_solve(R,t,c)
    error_list[i] = np.linalg.norm(p/np.linalg.norm(p) - s_true/np.linalg.norm(s_true))


print("Estimated direction:", p/np.linalg.norm(p))
print("True direction:", s_true/np.linalg.norm(s_true))
print("Avg Error:", np.mean(error_list))