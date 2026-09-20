import argparse
import json
import shutil
import subprocess
import matplotlib.pyplot as plt
from pathlib import Path

import numpy as np

from julia_functions import load_simulation_config_json, save_simulation_config_json
from fullstack_capture import run_capture


BASE_DIR = Path(__file__).resolve().parent
BASE_CONFIG = BASE_DIR / "simulation_config_for_testing.json"
SIMULATOR = BASE_DIR / "acoustic_data_simulator.jl"
OUT_DIR = BASE_DIR / "errors_analysis" / "test_data"
SEED = None

COUNT_PER_RANGE = 20
GENERATE = True

MIN_RANGE = 5
MAX_RANGE = 40
RANGE_STEP = 5


def _julia_string(path: Path) -> str:
    return path.as_posix()

def _run_julia_batch(*, batch_path: Path) -> None:
    julia = shutil.which("julia")
    if not julia:
        raise RuntimeError("Julia executable not found in PATH.")

    cmd = [
        julia,
        "--project=.",
        "-e",
        f"include(\"{_julia_string(SIMULATOR)}\"); run_test_batch(\"{_julia_string(batch_path)}\")",
    ]

    result = subprocess.run(cmd, capture_output=True, text=True, cwd=str(BASE_DIR))
    if result.returncode != 0:
        raise RuntimeError(
            "Julia batch simulator failed.\n"
            f"stdout:\n{result.stdout}\n\n"
            f"stderr:\n{result.stderr}\n"
        )

def _random_drone_pos2(
    rng: np.random.Generator,
    pinger_pos: list[float],
    sea_depth: float,
    *,
    min_distance: float = 4.0,
    max_distance: float = 25.0,
    max_attempts: int = 1000,
) -> list[float]:
    pinger = np.asarray(pinger_pos, dtype=float)
    min_z = -float(sea_depth) + 0.2
    max_z = -0.2
    for _ in range(max_attempts):
        r = rng.uniform(min_distance, max_distance)
        max_down = min(min_z - pinger[2],-r)
        max_up = min(max_z - pinger[2],r)
        max_phi = np.arcsin(max_up/r)
        min_phi = np.arcsin(max_down/r)
        theta = rng.uniform(0, 2*np.pi)
        phi = rng.uniform(min_phi, max_phi)
        x = r * np.cos(phi) * np.cos(theta)
        y = r * np.cos(phi) * np.sin(theta)
        z = r * np.sin(phi)
        candidate = pinger + np.array([x, y, z])
        if (min_z <= candidate[2] <= max_z) and (min_distance <= np.linalg.norm(candidate - pinger) <= max_distance):
            return candidate.tolist()

    raise RuntimeError("Failed to sample a valid pinger position within constraints.")


def main() -> None:

    OUT_DIR.mkdir(parents=True, exist_ok=True)
    base_config = load_simulation_config_json(BASE_CONFIG)
    rng = np.random.default_rng(SEED)

    # Keep pinger constant; randomize drone position.
    pinger_pos = [0.0, 0.0, -5.5]

    items: list[dict[str, str]] = []

    ranges_list = list(range(MIN_RANGE, MAX_RANGE + 1, RANGE_STEP))

    if GENERATE:
        for r in ranges_list:
            min_distance = r
            max_distance = r + RANGE_STEP
            print(f"Generating {COUNT_PER_RANGE} configs for range {min_distance}-{max_distance} m")

            for i in range(COUNT_PER_RANGE):
                drone_pos = _random_drone_pos2(
                    rng,
                    pinger_pos,
                    base_config["sea_depth"],
                    min_distance=min_distance,
                    max_distance=max_distance,
                )

                config = dict(base_config)
                config["pinger_pos"] = pinger_pos
                config["drone_pos"] = drone_pos

                config_path = OUT_DIR / f"config_{i}_{r}.json"
                data_path = OUT_DIR / f"data_{i}_{r}.csv"
                save_simulation_config_json(config, config_path)

                items.append(
                    {
                        "config_path": _julia_string(config_path),
                        "output_csv": _julia_string(data_path),
                    }
                )

        batch_path = OUT_DIR / "batch.json"
        with batch_path.open("w", encoding="utf-8") as f:
            json.dump({"verbose": False, "items": items}, f, indent=2)
            f.write("\n")

        print(f"Generated {COUNT_PER_RANGE} configs. Running Julia once to create CSVs...")
        _run_julia_batch(batch_path=batch_path)

        print("All simulations completed.")

    n = len(ranges_list)

    all_direction_errors = np.zeros((n,COUNT_PER_RANGE), dtype=object)
    all_position_errors_deg = np.zeros((n,COUNT_PER_RANGE), dtype=object)
    all_position_errors_abs = np.zeros((n,COUNT_PER_RANGE), dtype=object)

    fail_direction_counts = np.zeros(n, dtype=int)
    fail_position_counts = np.zeros(n, dtype=int)

    direction_errors_avg = np.zeros(n)
    position_errors_deg_avg = np.zeros(n)
    position_errors_abs_avg = np.zeros(n)

    direction_errors_std = np.zeros(n)
    position_errors_deg_std = np.zeros(n)
    position_errors_abs_std = np.zeros(n)

    
    for i in range(len(ranges_list)):
        r = ranges_list[i]
        print(f"Running captures for range {r}-{r+RANGE_STEP} m")
        direction_errors = np.zeros(COUNT_PER_RANGE)
        position_errors_deg = np.zeros(COUNT_PER_RANGE)
        position_errors_abs = np.zeros(COUNT_PER_RANGE)
        for j in range(COUNT_PER_RANGE):
            _, result = run_capture(
                config_path=OUT_DIR / f"config_{j}_{r}.json",
                tdoa_method="envelope_envelope",
                verbose=False,
                hydrophone_data_path=OUT_DIR / f"data_{j}_{r}.csv",
                inject_faulty_detection=False,
                skip=True,
            )

            direction_errors[j] = result[3]
            if result[3] is None or result[3] > 45.0:
                fail_direction_counts[i] += 1
            if result[3] is None:
                    direction_errors[j] = np.nan
                

            position_errors_deg[j] = result[1]
            position_errors_abs[j] = result[5]

            if result[1] is None or result[1] > 45.0:
                fail_position_counts[i] += 1
            if result[1] is None:
                    position_errors_deg[j] = np.nan
                    position_errors_abs[j] = np.nan
                
        all_direction_errors[i] = direction_errors
        all_position_errors_deg[i] = position_errors_deg
        all_position_errors_abs[i] = position_errors_abs

        direction_errors_avg[i] = np.nanmean(direction_errors)
        position_errors_deg_avg[i] = np.nanmean(position_errors_deg)
        position_errors_abs_avg[i] = np.nanmean(position_errors_abs)

        direction_errors_std[i] = np.nanstd(direction_errors)
        position_errors_deg_std[i] = np.nanstd(position_errors_deg)
        position_errors_abs_std[i] = np.nanstd(position_errors_abs)


    dictionary = {
        "ranges_m": ranges_list,
        "all_direction_errors_deg": [arr.tolist() for arr in all_direction_errors],
        "all_position_errors_deg": [arr.tolist() for arr in all_position_errors_deg],
        "all_position_errors_m": [arr.tolist() for arr in all_position_errors_abs],
        "fail_direction_counts": fail_direction_counts.tolist(),
        "fail_position_counts": fail_position_counts.tolist(),
    }

    np.savez(OUT_DIR / "fullstack_error_vs_range.npz", **dictionary)
    
    print("Results Summary:")
    print("Range (m) | Dir Error Avg (deg) | Dir Error Std (deg) | Pos Error Avg (deg) | Pos Error Std (deg) | Pos Error Avg (m) | Pos Error Std (m)")
    for idx, r in enumerate(ranges_list):
        print(f"{r:10} | {direction_errors_avg[idx]:18.2f} | {direction_errors_std[idx]:18.2f} | {position_errors_deg_avg[idx]:18.2f} | {position_errors_deg_std[idx]:18.2f} | {position_errors_abs_avg[idx]:16.2f} | {position_errors_abs_std[idx]:16.2f}")    

if __name__ == "__main__":
    main()