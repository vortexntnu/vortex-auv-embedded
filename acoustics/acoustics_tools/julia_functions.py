#from __future__ import annotations

import json
from pathlib import Path

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