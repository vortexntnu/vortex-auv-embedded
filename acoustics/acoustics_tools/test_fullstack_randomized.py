import json
import math
import os
import subprocess
import unittest
from dataclasses import dataclass
from pathlib import Path

import numpy as np

import fullstack_prototype
from julia_functions import load_simulation_config_json, save_simulation_config_json


# ----------------------------
# Configuration knobs
# ----------------------------
# Keep defaults modest so CI/dev runs don’t take forever.
DEFAULT_NUM_CASES = int(os.getenv("FULLSTACK_NUM_CASES", "8"))
DEFAULT_SEED = int(os.getenv("FULLSTACK_SEED", "1234"))
DEFAULT_TDOA_METHOD = os.getenv("FULLSTACK_TDOA_METHOD", "envelope_envelope")

# Performance thresholds (degrees). Adjust as your algorithms improve.
# These are intentionally not super strict to reduce flaky tests.
MAX_DIRECTION_ERROR_DEG_P90 = float(os.getenv("FULLSTACK_MAX_DIR_ERR_P90", "45"))
MAX_DIRECTION_ERROR_DEG_MEAN = float(os.getenv("FULLSTACK_MAX_DIR_ERR_MEAN", "25"))

# If too many scenarios fail to detect, the pipeline is considered broken.
MAX_DETECTION_FAILURE_RATE = float(os.getenv("FULLSTACK_MAX_FAIL_RATE", "0.25"))

ROOT_DIR = Path(__file__).resolve().parent
BASE_CONFIG_PATH = ROOT_DIR / "simulation_config_for_testing.json"
WORKING_CONFIG_PATH = ROOT_DIR / "simulation_config_for_testing.json"  # overwritten per scenario


@dataclass(frozen=True)
class ScenarioResult:
    seed: int
    direction_error_deg: float
    position_error_deg: float
    detected: bool


def _rotation_matrix_from_axis_angle(axis: np.ndarray, angle: float) -> np.ndarray:
    axis = np.asarray(axis, dtype=float)
    axis = axis / (np.linalg.norm(axis) + 1e-12)
    x, y, z = axis
    c = math.cos(angle)
    s = math.sin(angle)
    C = 1.0 - c
    return np.array(
        [
            [c + x * x * C, x * y * C - z * s, x * z * C + y * s],
            [y * x * C + z * s, c + y * y * C, y * z * C - x * s],
            [z * x * C - y * s, z * y * C + x * s, c + z * z * C],
        ],
        dtype=float,
    )


def _random_rotation(rng: np.random.Generator) -> np.ndarray:
    # Random axis (uniform on sphere) + random angle.
    v = rng.normal(size=3)
    v = v / (np.linalg.norm(v) + 1e-12)
    angle = float(rng.uniform(0.0, 2.0 * math.pi))
    return _rotation_matrix_from_axis_angle(v, angle)


def _rand_in_range(rng: np.random.Generator, lo: float, hi: float) -> float:
    return float(rng.uniform(lo, hi))


def _generate_config_variant(base_cfg: dict, rng: np.random.Generator) -> dict:
    cfg = json.loads(json.dumps(base_cfg))  # deep copy

    # Sea depth fixed per requirement.
    sea_depth = float(cfg.get("sea_depth", 6.0))

    # Random rotation of the hydrophone *structure* (relative coordinates).
    R = _random_rotation(rng)
    hydro = np.array(cfg["hydrophones_pos"], dtype=float)
    hydro_rot = (R @ hydro.T).T
    cfg["hydrophones_pos"] = hydro_rot.tolist()

    # Drone position random (keep it in a reasonable volume).
    # Coordinate convention in your configs appears to use negative z downward.
    drone_x = _rand_in_range(rng, -3.0, 3.0)
    drone_y = _rand_in_range(rng, -3.0, 3.0)
    drone_z = _rand_in_range(rng, -4.5, -1.0)
    cfg["drone_pos"] = [drone_x, drone_y, drone_z]

    # Pinger position random (keep it within the same rough neighborhood as the base config).
    pinger_x = _rand_in_range(rng, 8.0, 22.0)
    pinger_y = _rand_in_range(rng, 2.0, 18.0)
    # Keep pinger above seabed and below surface.
    pinger_z = _rand_in_range(rng, -(sea_depth - 0.2), -0.5)
    cfg["pinger_pos"] = [pinger_x, pinger_y, pinger_z]

    # Noise level random but around the base.
    base_noise = float(cfg.get("noise_level", 3.87e5))
    # Multiplicative jitter to keep it positive.
    noise_factor = float(rng.uniform(0.7, 1.3))
    cfg["noise_level"] = base_noise * noise_factor

    return cfg


def _run_julia_simulator(config_path: str | Path) -> None:
    # Use the local Project.toml/Manifest.toml environment.
    julia = os.getenv("JULIA", "julia")

    # Run simulator
    subprocess.run(
        [julia, "--project=.", "acoustic_data_simulator.jl", str(config_path)],
        check=True,
        cwd=str(ROOT_DIR),
    )


class TestFullstackRandomized(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if not BASE_CONFIG_PATH.is_file():
            raise unittest.SkipTest(f"Missing base config: {BASE_CONFIG_PATH}")

        # Basic sanity: Julia must be callable.
        julia = os.getenv("JULIA", "julia")
        try:
            subprocess.run([julia, "--version"], check=True, capture_output=True, text=True)
        except Exception as e:
            raise unittest.SkipTest(f"Julia not available ({julia}): {e}")

        # Ensure Julia deps are available once per test class.
        subprocess.run(
            [julia, "--project=.", "-e", "import Pkg; Pkg.instantiate()"],
            check=True,
            capture_output=True,
            text=True,
            cwd=str(ROOT_DIR),
        )

    def test_randomized_configs_direction_error(self):
        base_cfg = load_simulation_config_json(str(BASE_CONFIG_PATH))
        rng = np.random.default_rng(DEFAULT_SEED)

        results: list[ScenarioResult] = []

        for i in range(DEFAULT_NUM_CASES):
            seed_i = int(rng.integers(0, 2**31 - 1))
            rng_i = np.random.default_rng(seed_i)
            cfg_i = _generate_config_variant(base_cfg, rng_i)
            save_simulation_config_json(cfg_i, WORKING_CONFIG_PATH)

            # Run Julia to generate hydrophones_data.csv for this config.
            _run_julia_simulator(WORKING_CONFIG_PATH)

            est_pos, pos_err, est_dir, dir_err = fullstack_prototype.main(
                config=str(WORKING_CONFIG_PATH),
                tdoa_method=DEFAULT_TDOA_METHOD,
                verbose=False,
            )

            detected = math.isfinite(float(dir_err)) and (est_dir is not None)
            results.append(
                ScenarioResult(
                    seed=seed_i,
                    direction_error_deg=float(dir_err),
                    position_error_deg=float(pos_err),
                    detected=detected,
                )
            )

        failures = [r for r in results if not r.detected]
        failure_rate = len(failures) / max(1, len(results))
        self.assertLessEqual(
            failure_rate,
            MAX_DETECTION_FAILURE_RATE,
            msg=(
                f"Too many detection failures: {len(failures)}/{len(results)}\n"
                f"Seeds failed: {[r.seed for r in failures]}"
            ),
        )

        # Only score detected scenarios.
        scored = [r for r in results if r.detected]
        self.assertGreater(len(scored), 0, "No scenarios detected; cannot score performance")

        dir_errors = np.array([r.direction_error_deg for r in scored], dtype=float)
        mean_err = float(np.mean(dir_errors))
        p90_err = float(np.percentile(dir_errors, 90))

        self.assertLessEqual(
            mean_err,
            MAX_DIRECTION_ERROR_DEG_MEAN,
            msg=f"Mean direction error too high: mean={mean_err:.2f} deg, p90={p90_err:.2f} deg",
        )
        self.assertLessEqual(
            p90_err,
            MAX_DIRECTION_ERROR_DEG_P90,
            msg=f"P90 direction error too high: mean={mean_err:.2f} deg, p90={p90_err:.2f} deg",
        )


if __name__ == "__main__":
    unittest.main()
