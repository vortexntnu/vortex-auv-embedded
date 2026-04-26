import argparse
import csv
from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np


def parse_args() -> argparse.Namespace:
    script_dir = Path(__file__).resolve().parent
    default_dump_dir = script_dir / "ros2_dump"

    parser = argparse.ArgumentParser(
        description=(
            "Visualize drone pose/orientation from odometry together with "
            "bearing marker orientation from ROS2 CSV exports."
        )
    )
    parser.add_argument(
        "--dump-dir",
        type=Path,
        default=default_dump_dir,
        help="Directory containing ROS2 exported topic CSV files.",
    )
    parser.add_argument(
        "--odom-csv",
        type=str,
        default="nautilus_odom.csv",
        help="Odometry CSV file name inside --dump-dir.",
    )
    parser.add_argument(
        "--marker-csv",
        type=str,
        default="nautilus_visualization_bearing_marker.csv",
        help="Bearing marker CSV file name inside --dump-dir.",
    )
    parser.add_argument(
        "--sample-step",
        type=int,
        default=20,
        help="Plot every Nth matched marker to reduce clutter.",
    )
    parser.add_argument(
        "--marker-length",
        type=float,
        default=None,
        help="Override marker length. If omitted, uses scale.x from marker CSV.",
    )
    parser.add_argument(
        "--max-time-diff-ms",
        type=float,
        default=150.0,
        help="Maximum allowed time difference between marker and matched odom sample.",
    )
    return parser.parse_args()


def safe_float(value: str, default: float = 0.0) -> float:
    if value is None:
        return default
    text = value.strip()
    if not text:
        return default
    try:
        return float(text)
    except ValueError:
        return default


def read_odom_csv(csv_path: Path) -> dict[str, np.ndarray]:
    timestamps = []
    px = []
    py = []
    pz = []
    qx = []
    qy = []
    qz = []
    qw = []

    with csv_path.open("r", encoding="utf-8", newline="") as file:
        reader = csv.DictReader(file)
        for row in reader:
            timestamps.append(int(row["bag_timestamp_ns"]))
            px.append(safe_float(row.get("pose.pose.position.x")))
            py.append(safe_float(row.get("pose.pose.position.y")))
            pz.append(safe_float(row.get("pose.pose.position.z")))
            qx.append(safe_float(row.get("pose.pose.orientation.x")))
            qy.append(safe_float(row.get("pose.pose.orientation.y")))
            qz.append(safe_float(row.get("pose.pose.orientation.z")))
            qw.append(safe_float(row.get("pose.pose.orientation.w"), default=1.0))

    data = {
        "t": np.asarray(timestamps, dtype=np.int64),
        "pos": np.column_stack(
            [
                np.asarray(px, dtype=np.float64),
                np.asarray(py, dtype=np.float64),
                np.asarray(pz, dtype=np.float64),
            ]
        ),
        "quat": np.column_stack(
            [
                np.asarray(qx, dtype=np.float64),
                np.asarray(qy, dtype=np.float64),
                np.asarray(qz, dtype=np.float64),
                np.asarray(qw, dtype=np.float64),
            ]
        ),
    }

    order = np.argsort(data["t"])
    data["t"] = data["t"][order]
    data["pos"] = data["pos"][order]
    data["quat"] = data["quat"][order]
    return data


def read_marker_csv(csv_path: Path) -> dict[str, np.ndarray]:
    timestamps = []
    qx = []
    qy = []
    qz = []
    qw = []
    sx = []

    with csv_path.open("r", encoding="utf-8", newline="") as file:
        reader = csv.DictReader(file)
        for row in reader:
            timestamps.append(int(row["bag_timestamp_ns"]))
            qx.append(safe_float(row.get("pose.orientation.x")))
            qy.append(safe_float(row.get("pose.orientation.y")))
            qz.append(safe_float(row.get("pose.orientation.z")))
            qw.append(safe_float(row.get("pose.orientation.w"), default=1.0))
            sx.append(safe_float(row.get("scale.x"), default=1.0))

    return {
        "t": np.asarray(timestamps, dtype=np.int64),
        "quat": np.column_stack(
            [
                np.asarray(qx, dtype=np.float64),
                np.asarray(qy, dtype=np.float64),
                np.asarray(qz, dtype=np.float64),
                np.asarray(qw, dtype=np.float64),
            ]
        ),
        "length": np.asarray(sx, dtype=np.float64),
    }


def normalize_quat(quat: np.ndarray) -> np.ndarray:
    n = np.linalg.norm(quat)
    if n < 1e-12:
        return np.array([0.0, 0.0, 0.0, 1.0], dtype=np.float64)
    return quat / n


def quat_multiply(q1: np.ndarray, q2: np.ndarray) -> np.ndarray:
    x1, y1, z1, w1 = q1
    x2, y2, z2, w2 = q2
    return np.array(
        [
            w1 * x2 + x1 * w2 + y1 * z2 - z1 * y2,
            w1 * y2 - x1 * z2 + y1 * w2 + z1 * x2,
            w1 * z2 + x1 * y2 - y1 * x2 + z1 * w2,
            w1 * w2 - x1 * x2 - y1 * y2 - z1 * z2,
        ],
        dtype=np.float64,
    )


def quat_rotate_vector(quat: np.ndarray, vec: np.ndarray) -> np.ndarray:
    q = normalize_quat(quat)
    vq = np.array([vec[0], vec[1], vec[2], 0.0], dtype=np.float64)
    qc = np.array([-q[0], -q[1], -q[2], q[3]], dtype=np.float64)
    return quat_multiply(quat_multiply(q, vq), qc)[:3]


def nearest_indices(source_t: np.ndarray, target_t: np.ndarray) -> np.ndarray:
    idx = np.searchsorted(target_t, source_t)
    idx = np.clip(idx, 0, len(target_t) - 1)
    prev_idx = np.clip(idx - 1, 0, len(target_t) - 1)

    next_dist = np.abs(target_t[idx] - source_t)
    prev_dist = np.abs(target_t[prev_idx] - source_t)
    choose_prev = prev_dist <= next_dist
    idx[choose_prev] = prev_idx[choose_prev]
    return idx


def build_world_vectors(
    odom: dict[str, np.ndarray],
    marker: dict[str, np.ndarray],
    max_time_diff_ns: int,
    marker_length_override: float | None,
) -> dict[str, np.ndarray]:
    idx = nearest_indices(marker["t"], odom["t"])
    dt = np.abs(odom["t"][idx] - marker["t"])
    valid = dt <= max_time_diff_ns

    marker_idx = np.nonzero(valid)[0]
    odom_idx = idx[valid]

    origins = odom["pos"][odom_idx]
    heading_world = np.zeros((len(marker_idx), 3), dtype=np.float64)
    bearing_world = np.zeros((len(marker_idx), 3), dtype=np.float64)
    lengths = marker["length"][marker_idx].copy()

    if marker_length_override is not None:
        lengths[:] = marker_length_override

    base_x = np.array([1.0, 0.0, 0.0], dtype=np.float64)

    for i, (mi, oi) in enumerate(zip(marker_idx, odom_idx, strict=True)):
        q_world_base = odom["quat"][oi]
        q_base_bearing = marker["quat"][mi]

        heading = quat_rotate_vector(q_world_base, base_x)
        local_bearing = quat_rotate_vector(q_base_bearing, base_x)
        world_bearing = quat_rotate_vector(q_world_base, local_bearing)

        hn = np.linalg.norm(heading)
        bn = np.linalg.norm(world_bearing)
        if hn > 1e-12:
            heading = heading / hn
        if bn > 1e-12:
            world_bearing = world_bearing / bn

        heading_world[i] = heading
        bearing_world[i] = world_bearing

    return {
        "marker_idx": marker_idx,
        "odom_idx": odom_idx,
        "origin": origins,
        "heading": heading_world,
        "bearing": bearing_world,
        "length": lengths,
        "dt_ns": dt[valid],
    }


def plot_pose_and_bearing(
    odom: dict[str, np.ndarray],
    vectors: dict[str, np.ndarray],
    sample_step: int,
) -> None:
    sample_step = max(sample_step, 1)
    sl = slice(None, None, sample_step)

    origin = vectors["origin"][sl]
    heading = vectors["heading"][sl]
    bearing = vectors["bearing"][sl]
    lengths = vectors["length"][sl]

    fig, ax = plt.subplots(figsize=(10, 8))
    ax.plot(odom["pos"][:, 0], odom["pos"][:, 1], color="0.85", linewidth=1.0, label="Odom path")

    # Heading arrows use a fixed visualization length so orientation is easy to read.
    heading_len = np.maximum(0.6, np.median(lengths) * 0.5 if len(lengths) else 0.6)
    ax.quiver(
        origin[:, 0],
        origin[:, 1],
        heading[:, 0],
        heading[:, 1],
        angles="xy",
        scale_units="xy",
        scale=1.0 / heading_len,
        color="tab:blue",
        alpha=0.9,
        width=0.003,
        label="Drone heading",
    )

    ax.quiver(
        origin[:, 0],
        origin[:, 1],
        bearing[:, 0] * lengths,
        bearing[:, 1] * lengths,
        angles="xy",
        scale_units="xy",
        scale=1.0,
        color="tab:red",
        alpha=0.85,
        width=0.003,
        label="Bearing marker",
    )

    ax.scatter(origin[:, 0], origin[:, 1], s=10, color="black", alpha=0.4)

    ax.set_title("Drone position/orientation with bearing marker")
    ax.set_xlabel("X [m]")
    ax.set_ylabel("Y [m]")
    ax.axis("equal")
    ax.grid(True, alpha=0.3)
    ax.legend(loc="best")
    plt.tight_layout()
    plt.show()


def main() -> None:
    args = parse_args()

    odom_path = args.dump_dir / args.odom_csv
    marker_path = args.dump_dir / args.marker_csv

    if not odom_path.exists():
        raise FileNotFoundError(f"Missing odometry CSV: {odom_path}")
    if not marker_path.exists():
        raise FileNotFoundError(f"Missing marker CSV: {marker_path}")

    odom = read_odom_csv(odom_path)
    marker = read_marker_csv(marker_path)

    if len(odom["t"]) == 0:
        raise RuntimeError("Odometry CSV has no rows.")
    if len(marker["t"]) == 0:
        raise RuntimeError("Marker CSV has no rows.")

    vectors = build_world_vectors(
        odom=odom,
        marker=marker,
        max_time_diff_ns=int(args.max_time_diff_ms * 1e6),
        marker_length_override=args.marker_length,
    )

    if len(vectors["origin"]) == 0:
        raise RuntimeError(
            "No marker samples matched odometry within the time threshold. "
            "Increase --max-time-diff-ms."
        )

    dt_ms = vectors["dt_ns"].astype(np.float64) / 1e6
    print(f"Loaded odom samples: {len(odom['t'])}")
    print(f"Loaded marker samples: {len(marker['t'])}")
    print(f"Matched samples: {len(vectors['origin'])}")
    print(
        "Matching dt [ms] min/mean/max: "
        f"{dt_ms.min():.3f}/{dt_ms.mean():.3f}/{dt_ms.max():.3f}"
    )

    plot_pose_and_bearing(odom=odom, vectors=vectors, sample_step=args.sample_step)


if __name__ == "__main__":
    main()
