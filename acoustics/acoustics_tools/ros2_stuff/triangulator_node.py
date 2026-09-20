# pinger_triangulator.py
"""
Least-squares triangulation of a stationary acoustic pinger from
UAV odometry + bearing measurements.

Each "measurement" is one drone position + one unit direction vector
(optionally weighted by signal strength / confidence).

The pinger location is estimated by solving the over-determined linear
system built from the line-of-closest-approach equations.

Usage
-----
    from pinger_triangulator import PingerTriangulator

    tri = PingerTriangulator(min_measurements=5, max_measurements=50)

    # call this on every ROS2 callback update
    result = tri.update(drone_pos, direction_vec, weight)
    if result is not None:
        pinger_x, pinger_y, pinger_z = result
"""

import numpy as np
from collections import deque
from typing import Optional, Tuple


class PingerTriangulator:
    """
    Weighted least-squares triangulation of a stationary point source.

    Parameters
    ----------
    min_measurements : int
        Minimum number of measurements before an estimate is returned.
    max_measurements : int
        Sliding window size. Older measurements are discarded.
    min_angular_spread_deg : float
        Reject the estimate if all bearing vectors are nearly parallel
        (geometry is too poor for a reliable fix).
    """

    def __init__(
        self,
        min_measurements: int = 2,
        max_measurements: int = 100,
        min_angular_spread_deg: float = 0.0,
    ):
        self.min_measurements = min_measurements
        self.max_measurements = max_measurements
        self.min_angular_spread_deg = min_angular_spread_deg

        # Each entry: (position np.array[3], direction np.array[3], weight float)
        self._measurements: deque = deque(maxlen=max_measurements)

    # ------------------------------------------------------------------
    # Public API
    # ------------------------------------------------------------------

    def update(
        self,
        drone_position,   # array-like (3,)
        direction_vector, # array-like (3,)
        weight: float = 1.0,
    ) -> Optional[np.ndarray]:
        """
        Add one measurement and return the current best pinger estimate,
        or None if there are not yet enough measurements / geometry is poor.

        Parameters
        ----------
        drone_position  : [x, y, z] in the global/ENU frame
        direction_vector: unit (or unnormalised) vector pointing toward pinger
        weight          : confidence / signal strength scalar (>0)

        Returns
        -------
        np.ndarray shape (3,) with estimated [x, y, z] of the pinger,
        or None.
        """
        pos = np.asarray(drone_position, dtype=float)
        d = np.asarray(direction_vector, dtype=float)

        norm = np.linalg.norm(d)
        if norm < 1e-9:
            return None  # degenerate direction
        d = d / norm

        if weight <= 0:
            weight = 1e-6

        self._measurements.append((pos, d, float(weight)))

        if len(self._measurements) < self.min_measurements:
            return None

        return self._solve()

    def reset(self):
        """Clear all stored measurements."""
        self._measurements.clear()

    def normalize_weights(self):
        """Scale all weights so that the maximum is 1.0."""
        if not self._measurements:
            return
        max_w = max(w for _, _, w in self._measurements)
        if max_w > 1e-9:
            for i in range(len(self._measurements)):
                pos, d, w = self._measurements[i]
                self._measurements[i] = (pos, d, w / max_w)
    
    def multiply_weights(self, factor: float):
        """Multiply all weights by a constant factor."""
        for i in range(len(self._measurements)):
            pos, d, w = self._measurements[i]
            self._measurements[i] = (pos, d, w * factor)

    @property
    def measurement_count(self) -> int:
        return len(self._measurements)

    # ------------------------------------------------------------------
    # Internals
    # ------------------------------------------------------------------

    def _solve(self) -> Optional[np.ndarray]:
        """
        Weighted least-squares solution.

        For a line through point p with unit direction d, the projection
        matrix onto the plane *perpendicular* to d is:

            A_i = I - d_i @ d_i^T

        The closest point on the line to a candidate pinger position x
        satisfies:

            A_i @ (x - p_i) = 0   =>   A_i @ x = A_i @ p_i

        Stacking N such equations (each weighted by w_i):

            [sqrt(w_i) * A_i] @ x = [sqrt(w_i) * A_i @ p_i]

        which is a standard Ax = b least-squares problem.
        """
        rows_A = []
        rows_b = []

        for pos, d, w in self._measurements:
            # 3×3 projection matrix perpendicular to d
            A_i = np.eye(3) - np.outer(d, d)
            sw = np.sqrt(w)
            rows_A.append(sw * A_i)          # 3 rows
            rows_b.append(sw * A_i @ pos)    # 3 entries

        A = np.vstack(rows_A)   # (3N, 3)
        b = np.concatenate(rows_b)  # (3N,)

        # Check angular spread to catch degenerate geometry
        if not self._geometry_ok():
            return None

        # Solve via least squares (uses SVD internally)
        result, _, _, _ = np.linalg.lstsq(A, b, rcond=None)
        return result

    def _geometry_ok(self) -> bool:
        """
        Return True if the bearing vectors span more than
        `min_angular_spread_deg` degrees (i.e. geometry is not degenerate).
        Uses the smallest singular value of the stacked direction matrix
        as a proxy for angular diversity.
        """
        D = np.array([d for _, d, _ in self._measurements])  # (N, 3)
        _, s, _ = np.linalg.svd(D, full_matrices=False)
        # s[2] is near zero if all vectors are coplanar / parallel
        threshold = np.sin(np.radians(self.min_angular_spread_deg))
        return s[-1] > threshold * s[0]