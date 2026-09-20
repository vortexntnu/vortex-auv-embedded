from __future__ import annotations

import argparse
import subprocess
import sys
from pathlib import Path


BASE_DIR = Path(__file__).resolve().parent
TEST_DATA_DIR = BASE_DIR / "tests" / "test_data"
FULLSTACK_SCRIPT = BASE_DIR / "fullstack_prototype.py"


def _paths_for_index(index: int) -> tuple[Path, Path]:
	config_path = TEST_DATA_DIR / f"config_{index}.json"
	data_path = TEST_DATA_DIR / f"data_{index}.csv"
	return config_path, data_path


def main() -> int:
	parser = argparse.ArgumentParser(
		description=(
			"Run fullstack_prototype.py for a specific premade test_data index "
			"(tests/test_data/config_N.json + data_N.csv)."
		)
	)
	parser.add_argument("index", type=int, help="Dataset index N (config_N.json / data_N.csv)")
	parser.add_argument("--tdoa_method", type=str, default="envelope_edge_correlation", help="TDOA method")
	parser.add_argument("--gui", action="store_true", help="Show GUI (default: no GUI)")
	parser.add_argument("--quiet", action="store_true", help="Reduce printing")
	parser.add_argument("--export", type=str, default="", help="Save capture to .npz (empty disables)")
	parser.add_argument("--fps", type=int, default=24, help="GUI playback FPS")
	parser.add_argument("--faulty", action="store_true", help="Inject faulty detections")
	args = parser.parse_args()

	config_path, data_path = _paths_for_index(args.index)
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
		args.tdoa_method,
		"--fps",
		str(args.fps),
	]

	if not args.gui:
		cmd.append("--no-gui")
	if args.quiet:
		cmd.append("--quiet")
	if args.faulty:
		cmd.append("--faulty")
	if args.export:
		cmd.extend(["--export", args.export])
	else:
		# Disable export by explicitly passing an empty value is not supported by argparse;
		# just omit --export so fullstack_prototype uses its default.
		pass

	print("Running:")
	print("  " + " ".join(cmd))
	result = subprocess.run(cmd, cwd=str(BASE_DIR))
	return int(result.returncode)


if __name__ == "__main__":
	raise SystemExit(main())

