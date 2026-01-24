import math
import shutil
import subprocess
from pathlib import Path
import sys
import os

import numpy as np
import pytest

# Ensure parent directory is in sys.path for imports
sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), "..")))
from fullstack_capture import run_capture
from julia_functions import load_simulation_config_json, save_simulation_config_json


BASE_DIR = Path(__file__).resolve().parents[1]
BASE_CONFIG = BASE_DIR / "simulation_config_for_testing.json"
SIMULATOR = BASE_DIR / "acoustic_data_simulator.jl"


def _julia_string(path: Path) -> str:
    return path.as_posix()


def _run_julia_sim(config_path: Path, data_path: Path) -> None:
    julia = shutil.which("julia")
    if not julia:
        pytest.skip("Julia executable not found in PATH.")

    cmd = [
    julia,
    "--project=.",  # <-- add this line
    "-e",
    (
        f"ARGS=[\"{_julia_string(config_path)}\","
        f"\"{_julia_string(data_path)}\","
        f"\"false\"]; include(\"{_julia_string(SIMULATOR)}\"); main()"
    ),
]

    result = subprocess.run(cmd, capture_output=True, text=True)
    if result.returncode != 0:
        raise RuntimeError(
            "Julia simulator failed.\n"
            f"stdout:\n{result.stdout}\n\n"
            f"stderr:\n{result.stderr}\n"
        )


def _random_pinger_pos(
    rng: np.random.Generator,
    drone_pos: list[float],
    sea_depth: float,
    min_distance: float = 4.0,
    max_distance: float = 25.0,
    max_attempts: int = 1000,
) -> list[float]:
    drone = np.asarray(drone_pos, dtype=float)
    min_z = -float(sea_depth) + 0.2
    max_z = -0.2

    for _ in range(max_attempts):
        direction = rng.normal(size=3)
        direction /= np.linalg.norm(direction)
        radius = rng.uniform(min_distance, max_distance)
        candidate = drone + direction * radius
        if min_z <= candidate[2] <= max_z:
            return candidate.tolist()

    raise RuntimeError("Failed to sample a valid pinger position within constraints.")


@pytest.mark.slow
def test_fullstack_random_pinger_positions(tmp_path: Path) -> None:
    base_config = load_simulation_config_json(BASE_CONFIG)
    rng = np.random.default_rng(12345)

    cases = 5
    for idx in range(cases):
        pinger_pos = _random_pinger_pos(
            rng,
            base_config["drone_pos"],
            base_config["sea_depth"],
            min_distance=4.0,
        )
        distance = float(np.linalg.norm(np.array(pinger_pos) - np.array(base_config["drone_pos"])))
        assert distance > 4.0, f"Case {idx}: Pinger distance too small: {distance} (pinger_pos={pinger_pos})"

        config = dict(base_config)
        config["pinger_pos"] = pinger_pos

        config_path = tmp_path / f"simulation_config_{idx}.json"
        data_path = tmp_path / f"hydrophones_data_{idx}.csv"
        save_simulation_config_json(config, config_path)

        _run_julia_sim(config_path, data_path)

        store, result = run_capture(
            config_path=str(config_path),
            tdoa_method="envelope_edge_correlation",
            verbose=False,
            hydrophone_data_path=str(data_path),
            inject_faulty_detection=False,
        )

        assert store.meta.pinger_found, f"Case {idx}: Pinger not found! (pinger_pos={pinger_pos}, distance={distance})"
        direction_error = float(result[3])
        assert math.isfinite(direction_error), f"Case {idx}: Direction error not finite: {direction_error} (pinger_pos={pinger_pos})"
        assert abs(direction_error) <= 180.0, f"Case {idx}: Direction error out of bounds: {direction_error} (pinger_pos={pinger_pos})"

