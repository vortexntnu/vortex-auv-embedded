import numpy as np

def tdoa_solve(R, t, c):
    """
    R : (N,3) hydrophone positions
    t : (N,) arrival times
    c : wave speed
    returns estimated source position (3,)
    """

    R = np.asarray(R, dtype=float)
    t = np.asarray(t, dtype=float)

    N = len(t)
    r0 = R[0]
    t0 = t[0]

    A = []
    b = []

    for i in range(1, N):
        ri = R[i]
        ti = t[i]

        Ai = 2 * (r0 - ri)
        bi = c**2 * (ti**2 - t0**2) \
             + np.dot(r0, r0) - np.dot(ri, ri) 

        A.append(Ai)
        b.append(bi)

    A = np.vstack(A)      # (N-1,3)
    b = np.array(b)       # (N-1,)

    # Least squares solution
    s, residuals, rank, _ = np.linalg.lstsq(A, b, rcond=None)

    return s


# Hydrophone positions
R = np.array([
    [0,0,0],
    [1,0,0],
    [0,1,0],
    [0,0,1],
    [1,1,1]
])

# True source
s_true = np.array([0.3, 0.4, 0.2])

c = 1500.0  # speed of sound

# Generate arrival times
t = np.linalg.norm(R - s_true, axis=1) / c

# Add noise
t += np.random.normal(0, 1e-5, size=len(t))

# Solve
s_est = tdoa_solve(R, t, c)

print("True:", s_true)
print("Estimated:", s_est)