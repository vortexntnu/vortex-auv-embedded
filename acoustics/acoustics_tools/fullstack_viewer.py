import argparse

from fullstack_gui import FrameStore, launch_gui


def main():
    parser = argparse.ArgumentParser(description="View a saved fullstack capture.")
    parser.add_argument("capture", type=str, help="Path to .npz capture file saved by fullstack_prototype.py")
    parser.add_argument("--fps", type=int, default=24, help="Playback FPS")
    args = parser.parse_args()

    store = FrameStore.load_npz(args.capture)
    launch_gui(store, desired_fps=args.fps)


if __name__ == "__main__":
    main()
