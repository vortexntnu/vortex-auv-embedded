from __future__ import annotations

import subprocess
import sys
from pathlib import Path

# =====================
# Edit these variables
# =====================
INDEX = 6  # Dataset index N (config_N.json / data_N.csv)
TDOA_METHOD = "envelope_envelope"
SHOW_GUI = True
QUIET = False
FPS = 10
FAULTY = False
EXPORT_NPZ = ""  # e.g. "case_98.npz" (empty string disables)

# Optional: override paths (normally you don't need to)
BASE_DIR = Path(__file__).resolve().parent
TEST_DATA_DIR = BASE_DIR / "errors_analysis" / "test_data" #"tests" 
FULLSTACK_SCRIPT = BASE_DIR / "fullstack_prototype.py"

R = 15  # Range modifier (only for naming convenience)

def main() -> int:
    config_path = TEST_DATA_DIR / f"config_{INDEX}_{R}.json"
    data_path = TEST_DATA_DIR / f"data_{INDEX}_{R}.csv"

    if not config_path.exists():
        raise SystemExit(f"Config not found: {config_path}")
    if not data_path.exists():
        raise SystemExit(f"Data not found:   {data_path}")
    if not FULLSTACK_SCRIPT.exists():
        raise SystemExit(f"fullstack_prototype.py not found: {FULLSTACK_SCRIPT}")

    cmd: list[str] = [
        sys.executable,
        str(FULLSTACK_SCRIPT),
        "--config",
        str(config_path),
        "--hydrophone_data_path",
        str(data_path),
        "--tdoa_method",
        str(TDOA_METHOD),
        "--fps",
        str(FPS),
    ]

    if not SHOW_GUI:
        cmd.append("--no-gui")
    if QUIET:
        cmd.append("--quiet")
    if FAULTY:
        cmd.append("--faulty")
    if EXPORT_NPZ:
        cmd.extend(["--export", EXPORT_NPZ])

    print("Running:")
    print("  " + " ".join(cmd))
    result = subprocess.run(cmd, cwd=str(BASE_DIR))
    return int(result.returncode)


if __name__ == "__main__":
    raise SystemExit(main())
