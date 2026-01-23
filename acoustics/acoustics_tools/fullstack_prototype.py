import argparse

from fullstack_capture import run_capture
from fullstack_gui import launch_gui

def main(config: str | None = None, tdoa_method: str | None = None, *, verbose: bool = True):
    """Run the fullstack pipeline (simulation + estimation).

    This function does NOT open the GUI (so tests/callers are safe).

    Returns
    -------
    tuple
        (estimated_position, position_error_deg, estimated_direction, direction_error_deg)
    """

    if config is None or tdoa_method is None:
        parser = argparse.ArgumentParser(description="Run the acoustic signal processing simulation.")
        parser.add_argument("--config", type=str, default="simulation_config.json")
        parser.add_argument("--tdoa_method", type=str, default="envelope_envelope")
        args = parser.parse_args()
        if config is None:
            config = args.config
        if tdoa_method is None:
            tdoa_method = args.tdoa_method

    _store, results = run_capture(config_path=config, tdoa_method=tdoa_method, verbose=verbose)
    return results

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Run the acoustic signal processing simulation + GUI.")
    parser.add_argument("--config", type=str, default="simulation_config.json", help="Path to simulation config JSON")
    parser.add_argument("--tdoa_method", type=str, default="envelope_envelope", help="TDOA method")
    parser.add_argument("--no-gui", action="store_true", help="Run capture only (no GUI)")
    parser.add_argument("--export", type=str, default="buffer_frames.npz", help="Save captured frames to .npz")
    parser.add_argument("--fps", type=int, default=24, help="GUI playback FPS")
    parser.add_argument("--quiet", action="store_true", help="Reduce printing")
    parser.add_argument("--hydrophone_data_path", type=str, default="hydrophones_data.csv", help="Path to hydrophone data CSV file")
    args = parser.parse_args()

    store, results = run_capture(config_path=args.config, tdoa_method=args.tdoa_method, verbose=not args.quiet, hydrophone_data_path=args.hydrophone_data_path)
    if args.export:
        store.save_npz(args.export)
        print(f"Saved capture to: {args.export}")

    if not args.no_gui:
        launch_gui(store, desired_fps=args.fps)

    # Preserve previous behavior: script exits after GUI closes / capture completes
    _ = results