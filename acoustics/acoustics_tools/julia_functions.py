#from __future__ import annotations

import json
from pathlib import Path
import numpy as np
import os

def save_simulation_config_json(config: dict, path: str | Path = "simulation_config.json") -> Path:
    """Write a simulator config JSON file that both Python and Julia can read.

    Expected schema
    ---------------
    - hydrophones_pos: list of 5 positions, each [x, y, z]
    - drone_pos: [x, y, z]
    - pinger_pos: [x, y, z]
    - sea_depth: number (optional)
    - noise_level: number (optional)
    - noise_type: "white" | "red" (optional)

    Returns
    -------
    Path
        Path to the written JSON file.
    """
    out_path = Path(path)
    with out_path.open("w", encoding="utf-8") as f:
        json.dump(config, f, indent=2)
        f.write("\n")
    return out_path


def load_simulation_config_json(path: str | Path = "simulation_config.json") -> dict:
    """Load a simulator config JSON file."""
    with Path(path).open("r", encoding="utf-8") as f:
        return json.load(f)
    
def load_hydrophone_data(file_path):
    """
    Loads hydrophone data from a CSV file and returns time and signal as numpy arrays.

    Parameters
    ----------
    file_path : str
        Path to the CSV file containing 'time,signal' columns.

    Returns
    -------
    tuple
        (time_array, signal_array) as numpy arrays.
    """
    data = np.loadtxt(file_path, delimiter=',', skiprows=1)
    time = data[:, 0]
    signal = data[:, 1]
    return time, signal


def load_combined_hydrophone_data(file_path: str = 'hydrophones_data.csv'):
    """Loads combined hydrophone data from a single CSV.

    Expected columns:
      time, hydrophone_1, hydrophone_2, ..., hydrophone_5

    Returns
    -------
    tuple
        (time_array, signals_list) where signals_list is a list of 1D numpy arrays.
    """
    data = np.loadtxt(file_path, delimiter=',', skiprows=1)
    if data.ndim != 2 or data.shape[1] < 2:
        raise ValueError(f"Invalid combined hydrophone CSV format: {file_path}")
    time = data[:, 0]
    signals = [data[:, i] for i in range(1, data.shape[1])]
    return time, signals

def load_all_hydrophone_data():
    """
    Loads hydrophone data.

    Prefers the new single-file format `hydrophones_data.csv` when present.
    Falls back to legacy files `hydrophone_1_data.csv`..`hydrophone_5_data.csv`.

    Returns
    -------
    list
        List of tuples [(time1, signal1), (time2, signal2), ..., (time5, signal5)].
    """
    combined_path = 'hydrophones_data.csv'
    if os.path.isfile(combined_path):
        time, signals = load_combined_hydrophone_data(combined_path)
        return [(time, sig) for sig in signals]

    hydro_data = []
    for i in range(1, 6):
        file_path = f'hydrophone_{i}_data.csv'
        time, signal = load_hydrophone_data(file_path)
        hydro_data.append((time, signal))
    return hydro_data