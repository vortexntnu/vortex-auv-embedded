import argparse
import json
import shutil
import subprocess
from pathlib import Path

import numpy as np

from julia_functions import load_simulation_config_json, save_simulation_config_json


BASE_DIR = Path(__file__).resolve().parent
BASE_CONFIG = BASE_DIR / "simulation_config_for_testing.json"
SIMULATOR = BASE_DIR / "acoustic_data_simulator.jl"
OUT_DIR = BASE_DIR / "tests" / "test_data"


def _julia_string(path: Path) -> str:
    return path.as_posix()


def _run_julia_sim(*, config_path: Path, data_path: Path) -> None:
    julia = shutil.which("julia")
    if not julia:
        raise RuntimeError("Julia executable not found in PATH.")

    cmd = [
        julia,
        "--project=.",
        "-e",
        (
            f"ARGS=[\"{_julia_string(config_path)}\","
            f"\"{_julia_string(data_path)}\","
            f"\"false\"]; include(\"{_julia_string(SIMULATOR)}\"); main()"
        ),
    ]

    result = subprocess.run(cmd, capture_output=True, text=True, cwd=str(BASE_DIR))
    if result.returncode != 0:
        raise RuntimeError(
            "Julia simulator failed.\n"
            f"stdout:\n{result.stdout}\n\n"
            f"stderr:\n{result.stderr}\n"
        )


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


def _random_pinger_pos(
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
        direction = rng.normal(size=3)
        direction /= np.linalg.norm(direction)
        radius = rng.uniform(min_distance, max_distance)
        candidate = pinger + direction * radius
        if min_z <= candidate[2] <= max_z:
            return candidate.tolist()

    raise RuntimeError("Failed to sample a valid pinger position within constraints.")


def main() -> None:
    parser = argparse.ArgumentParser(description="Generate premade hydrophone CSV test data using the Julia simulator.")
    parser.add_argument("-n", "--count", type=int, default=100, help="Number of datasets to generate.")
    parser.add_argument("--seed", type=int, default=12345, help="RNG seed for reproducible pinger positions.")
    parser.add_argument("--min-distance", type=float, default=4.0, help="Minimum pinger distance from drone [m].")
    parser.add_argument("--max-distance", type=float, default=25.0, help="Maximum pinger distance from drone [m].")
    args = parser.parse_args()

    OUT_DIR.mkdir(parents=True, exist_ok=True)
    base_config = load_simulation_config_json(BASE_CONFIG)
    rng = np.random.default_rng(args.seed)

    # Keep pinger constant; randomize drone position.
    pinger_pos = [0.0, 0.0, -5.5]

    items: list[dict[str, str]] = []

    for i in range(args.count):
        drone_pos = _random_pinger_pos(
            rng,
            pinger_pos,
            base_config["sea_depth"],
            min_distance=args.min_distance,
            max_distance=args.max_distance,
        )

        config = dict(base_config)
        config["pinger_pos"] = pinger_pos
        config["drone_pos"] = drone_pos

        config_path = OUT_DIR / f"config_{i}.json"
        data_path = OUT_DIR / f"data_{i}.csv"
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

    print(f"Generated {args.count} configs. Running Julia once to create CSVs...")
    _run_julia_batch(batch_path=batch_path)

    print("All simulations completed.")


if __name__ == "__main__":
    main()