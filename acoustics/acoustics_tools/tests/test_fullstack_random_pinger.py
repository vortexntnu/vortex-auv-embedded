import math
from pathlib import Path
import sys
import os
import re

import numpy as np
import pytest

# Ensure parent directory is in sys.path for imports
sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), "..")))
from fullstack_capture import run_capture
from julia_functions import load_simulation_config_json, save_simulation_config_json


BASE_DIR = Path(__file__).resolve().parents[1]
TEST_DATA_DIR = BASE_DIR / "tests" / "test_data"


def _discover_test_cases() -> list[tuple[Path, Path]]:
    if not TEST_DATA_DIR.exists():
        return []

    configs = sorted(TEST_DATA_DIR.glob("config_*.json"))
    pairs: list[tuple[Path, Path]] = []

    for cfg in configs:
        m = re.match(r"config_(\d+)\.json$", cfg.name)
        if not m:
            continue
        idx = m.group(1)
        csv_path = TEST_DATA_DIR / f"data_{idx}.csv"
        if csv_path.exists():
            pairs.append((cfg, csv_path))

    return pairs


_ALL_CASES = _discover_test_cases()
_LIMIT = int(os.getenv("FULLSTACK_PREMADE_CASES", "5"))
_CASES = _ALL_CASES if _LIMIT <= 0 else _ALL_CASES[:_LIMIT]


@pytest.mark.slow
@pytest.mark.parametrize("config_path,csv_path", _CASES, ids=[p[0].stem for p in _CASES])
def test_fullstack_premade_random_pinger_data(config_path: Path, csv_path: Path) -> None:
    if not _ALL_CASES:
        pytest.skip(
            "No premade test data found in tests/test_data. "
            "Generate it first with: python test_data_generator.py -n 100"
        )

    cfg = load_simulation_config_json(config_path)
    pinger_pos = cfg.get("pinger_pos")
    drone_pos = cfg.get("drone_pos")
    assert pinger_pos is not None and drone_pos is not None

    distance = float(np.linalg.norm(np.array(pinger_pos) - np.array(drone_pos)))
    assert distance > 4.0, f"Pinger distance too small: {distance} (pinger_pos={pinger_pos})"

    store, result = run_capture(
        config_path=str(config_path),
        tdoa_method="envelope_edge_correlation",
        verbose=False,
        hydrophone_data_path=str(csv_path),
        inject_faulty_detection=False,
    )

    assert store.meta.pinger_found, f"Pinger not found! (config={config_path.name}, data={csv_path.name})"
    direction_error = float(result[3])
    assert math.isfinite(direction_error), f"Direction error not finite: {direction_error} (config={config_path.name})"
    assert abs(direction_error) <= 180.0, f"Direction error out of bounds: {direction_error} (config={config_path.name})"

