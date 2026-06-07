import time
import csv
import threading
import importlib
from collections import deque

import numpy as np
import matplotlib.pyplot as plt
from matplotlib.animation import FuncAnimation
from matplotlib import colors as mcolors
from mpl_toolkits.mplot3d import Axes3D  # noqa: F401
from mpl_toolkits.mplot3d.art3d import Line3DCollection


# =========================
# ROS2 Configuration
# =========================
TOPIC_NAME = "/nautilus/acoustics/bearing_measurement"
ODOM_TOPIC_NAME = "/nautilus/odom"
# Example custom type; change package name if yours differs.
MSG_TYPE = "vortex_msgs.msg.BearingMeasurement"
QOS_DEPTH = 100
ROS_NODE_NAME = "bearing_live_viewer"
TARGET_ID_FILTER = None  # Set to an int to only visualize one target_id

import rclpy
from rclpy.node import Node
from rclpy.qos import qos_profile_sensor_data
from nav_msgs.msg import Odometry
from geometry_msgs.msg import Vector3Stamped   # adjust to your actual msg type

from triangulator_node import PingerTriangulator
from scipy.spatial.transform import Rotation


# =========================
# Plot Configuration
# =========================
MAX_SAMPLES = 1000
MAX_SAMPLES_LOG = 1024  # For logging all received samples, not just visible ones
FADE_SECONDS = 240.0
PLOT_INTERVAL_MS = 100
TRIANGULATION_MERGE_DISTANCE = 1
TRIANGULATION_MERGE_TIME = 3.0
ESTIMATE_HISTORY_MIN_DISTANCE = 0.05
ESTIMATE_HISTORY_MIN_SECONDS = 0.75

FILTER_DROPOFF_WEIGHT = 10
FILTER_MERGE_DISTANCE = 0.25

WEIGHT_MIN = 0.0
WEIGHT_MAX = 1.0

DENSITY_AZ_BINS = 180
DENSITY_EL_BINS = 90
DENSITY_SIGMA_BINS = 2.7
DENSITY_ALPHA_MAX = 0.6
NEW_POINT_MARKER_SECONDS = 1.0

hydrophones_local = np.array([
    [ 0.13, 0.235,-0.215],
    [-0.13,-0.235, 0.215],
    [-0.13, 0.235,-0.215],
    [ 0.13,-0.235,-0.215],
    [ 0.13, 0.235, 0.215]
], dtype=float)

""" hydrophones_local = np.array([
     [0.0, 0.0, 0.0],
     [0.0, 0.0, 0.0],
     [32.3, -4.3, 3.66],
     [-6.6, 34.7, 1.66],
     [-4.6, 0.0, -34.34]
], dtype=float) """

longest_hydrophone_length = np.linalg.norm(hydrophones_local, axis=1).max()
hydrophones_local /= longest_hydrophone_length * 0.5  # Scale down for better visualization

hydrophones_pitch = 0 #90 - 19.323971
hydrophones_yaw = 0 #45.0

# =========================
# Data storage
# =========================
LOG_DIR = "src/quick_test/csv_log"
LOG_NAME = f"ros2_samples_log_"
samples = deque(maxlen=MAX_SAMPLES)
samples_log = deque(maxlen=MAX_SAMPLES_LOG)  # For logging all received samples, not just visible ones
odom_samples = deque(maxlen=MAX_SAMPLES_LOG)


# Precompute unit sphere once
u = np.linspace(0, 2 * np.pi, 30)
v = np.linspace(0, np.pi, 20)
SPHERE_X = np.outer(np.cos(u), np.sin(v))
SPHERE_Y = np.outer(np.sin(u), np.sin(v))
SPHERE_Z = np.outer(np.ones_like(u), np.cos(v))

NORM = mcolors.Normalize(vmin=WEIGHT_MIN, vmax=WEIGHT_MAX)
CMAP = plt.cm.viridis

latest_target_id = None
latest_odom_pose = None
latest_bearing_direction = None
latest_bearing_weight = None
odom_reference_position = None

# Triangulation state
triangulator = None
stored_measurements = deque(maxlen=1024)  # (ts, bearing_dir, odom_pos, odom_quat, base_weight)
stored_measurements_global = deque(maxlen=1024)  # (ts, bearing_dir_global, odom_pos, odom_quat, base_weight)
filtered_measurements = []
filtered_measurement = None
estimated_pinger_position = None
estimated_pinger_history = deque(maxlen=256)  # (ts, position)
triangulation_measurements = []  # For visualization: (bearing_dir_global, odom_pos, weight)

# Drone body convention: +x forward, +y right, +z down.
DRONE_BODY_AXIS_SIGNS = np.array([1.0, 1.0, 1.0], dtype=float)


def import_message_type(type_path: str):
    """
    Resolve a ROS2 message class from a string like:
      - "std_msgs.msg.Float32MultiArray"
      - "vortex_msgs.msg.BearingMeasurement"
    """
    module_path, _, class_name = type_path.rpartition(".")
    if not module_path or not class_name:
        raise ValueError(f"Invalid MSG_TYPE '{type_path}'. Use '<pkg>.msg.<MessageClass>'.")

    module = importlib.import_module(module_path)
    return getattr(module, class_name)


def _get_nested_attr(obj, path):
    cur = obj
    for key in path:
        if not hasattr(cur, key):
            return None
        cur = getattr(cur, key)
    return cur


def parse_bearing_message(msg):
    """
    Parse message shape:
      geometry_msgs/Vector3Stamped bearing
      float64 weight
      int32 target_id

    Returns: (x, y, z, weight, target_id)
    """
    bx = _get_nested_attr(msg, ("bearing", "vector", "x"))
    by = _get_nested_attr(msg, ("bearing", "vector", "y"))
    bz = _get_nested_attr(msg, ("bearing", "vector", "z"))
    if bx is None or by is None or bz is None:
        print("Unable to parse msg.bearing.vector.{x,y,z}. Update parse_bearing_message().")
        return None

    weight = float(getattr(msg, "weight", 1.0))
    target_id = int(getattr(msg, "target_id", 0))

    if TARGET_ID_FILTER is not None and target_id != TARGET_ID_FILTER:
        return None

    vec = np.array([float(bx), float(by), float(bz)], dtype=float)
    norm = np.linalg.norm(vec)
    if norm <= 1e-9:
        print("Received near-zero bearing vector, ignoring.")
        return None
    vec /= norm

    x, y, z = vec.tolist()
    return x, y, z, weight, target_id


def parse_odometry_message(msg):
    pose = getattr(msg, "pose", None)
    if pose is None or not hasattr(pose, "pose"):
        print("Unable to parse msg.pose.pose. Update parse_odometry_message().")
        return None

    position = pose.pose.position
    orientation = pose.pose.orientation

    pos = np.array([float(position.x), float(position.y), float(position.z)], dtype=float)
    quat = np.array(
        [
            float(orientation.x),
            float(orientation.y),
            float(orientation.z),
            float(orientation.w),
        ],
        dtype=float,
    )

    quat_norm = np.linalg.norm(quat)
    if quat_norm <= 1e-9:
        print("Received near-zero odometry quaternion, ignoring.")
        return None

    quat /= quat_norm
    return pos, quat


def get_visible_arrays(now):
    visible = [(ts, x, y, z, snr) for ts, x, y, z, snr in samples if now - ts <= FADE_SECONDS]
    if not visible:
        return None

    arr = np.array([(x, y, z, snr) for _, x, y, z, snr in visible], dtype=float)
    ages = np.array([now - ts for ts, *_ in visible], dtype=float)
    alphas = np.maximum(0.05, 1.0 - ages / FADE_SECONDS)

    xs = arr[:, 0]
    ys = arr[:, 1]
    zs = arr[:, 2]
    snrs = arr[:, 3]

    colors = CMAP(NORM(snrs))
    colors[:, 3] = alphas

    return xs, ys, zs, snrs, colors, alphas, ages


def get_visible_odom_arrays(now):
    visible = [(ts, pos, quat) for ts, pos, quat in odom_samples if now - ts <= FADE_SECONDS]
    if not visible:
        return None

    positions = np.array([pos for _, pos, _ in visible], dtype=float)
    quats = np.array([quat for _, _, quat in visible], dtype=float)
    ages = np.array([now - ts for ts, *_ in visible], dtype=float)
    alphas = np.maximum(0.05, 1.0 - ages / FADE_SECONDS)
    return positions, quats, ages, alphas


def gaussian_kernel_1d(sigma_bins):
    if sigma_bins <= 0:
        return np.array([1.0], dtype=float)

    radius = int(np.ceil(3.0 * sigma_bins))
    x = np.arange(-radius, radius + 1, dtype=float)
    kernel = np.exp(-(x * x) / (2.0 * sigma_bins * sigma_bins))
    kernel /= np.sum(kernel)
    return kernel


def smooth_2d_density(grid, sigma_bins):
    kernel = gaussian_kernel_1d(sigma_bins)

    smoothed = np.apply_along_axis(
        lambda row: np.convolve(row, kernel, mode="same"),
        axis=1,
        arr=grid,
    )
    smoothed = np.apply_along_axis(
        lambda col: np.convolve(col, kernel, mode="same"),
        axis=0,
        arr=smoothed,
    )
    return smoothed


def build_az_el_density(azimuth_deg, elevation_deg, weights):
    hist, _, _ = np.histogram2d(
        elevation_deg,
        azimuth_deg,
        bins=[DENSITY_EL_BINS, DENSITY_AZ_BINS],
        range=[[-90.0, 90.0], [-180.0, 180.0]],
        weights=weights,
    )

    smoothed = smooth_2d_density(hist, DENSITY_SIGMA_BINS)
    scaled = np.log1p(smoothed)
    peak = float(np.max(scaled))
    if peak > 1e-12:
        scaled /= peak

    return scaled

def write_samples_log_to_csv(path="samples_log.csv"):
    with open(path, "w", newline="", encoding="utf-8") as f:
        writer = csv.writer(f)
        writer.writerow(["timestamp", "x", "y", "z", "weight"])
        writer.writerows(samples_log)

    print(f"Saved {len(samples_log)} samples to {path}")


def setup_3d_axes(ax):
    ax.set_xlabel("X")
    ax.set_ylabel("Y")
    ax.set_zlabel("Z")
    ax.set_xlim(-1.5, 1.5)
    ax.set_ylim(-1.5, 1.5)
    ax.set_zlim(-1.5, 1.5)
    ax.invert_yaxis()
    ax.invert_zaxis()
    ax.set_box_aspect([1, 1, 1])

    ax.plot_wireframe(SPHERE_X, SPHERE_Y, SPHERE_Z, alpha=0.08)

    # Reference hydrophone
    ref = hydrophones_local[0]
    ax.scatter([ref[0]], [ref[1]], [ref[2]], c="black", s=80, marker="o", depthshade=False)

    # Hydrophones + lines from H0
    for i, (hx, hy, hz) in enumerate(hydrophones_local):
        ax.scatter([hx], [hy], [hz], c="red", s=60, marker="^", depthshade=False)
        ax.text(hx, hy, hz, f" H{i}", fontsize=8)

        if i != 0:
            ax.plot(
                [ref[0], hx],
                [ref[1], hy],
                [ref[2], hz],
                linestyle="--",
                linewidth=1.5,
                alpha=0.6,
            )


def setup_az_el_axes(ax2):
    ax2.set_xlabel("Azimuth (deg)")
    ax2.set_ylabel("Elevation (deg)")
    ax2.set_title("Direction (Azimuth vs Elevation)")
    ax2.set_xlim(-180, 180)
    ax2.set_ylim(-90, 90)
    ax2.grid(True, alpha=0.3)

    density_img = ax2.imshow(
        np.zeros((DENSITY_EL_BINS, DENSITY_AZ_BINS), dtype=float),
        extent=[-180, 180, -90, 90],
        origin="lower",
        cmap="inferno",
        interpolation="bilinear",
        vmin=0.0,
        vmax=1.0,
        alpha=0.0,
        aspect="auto",
        zorder=0,
    )

    return density_img


def setup_global_position_axes(ax3):
    ax3.set_title("Drone Global Pose")
    ax3.set_xlabel("X")
    ax3.set_ylabel("Y")
    ax3.set_zlabel("Z")
    ax3.invert_yaxis()
    ax3.invert_zaxis()
    ax3.grid(True, alpha=0.18)
    ax3.set_box_aspect([1, 1, 1])

    origin = np.zeros(3, dtype=float)
    axis_scale = 1.0
    ax3.plot([origin[0], origin[0] + axis_scale], [origin[1], origin[1]], [origin[2], origin[2]], color="red", alpha=0.18)
    ax3.plot([origin[0], origin[0]], [origin[1], origin[1] + axis_scale], [origin[2], origin[2]], color="green", alpha=0.18)
    ax3.plot([origin[0], origin[0]], [origin[1], origin[1]], [origin[2], origin[2] + axis_scale], color="blue", alpha=0.18)
    ax3.scatter([0.0], [0.0], [0.0], c="black", s=30, depthshade=False)

    trail_line, = ax3.plot([], [], [], color="#1f77b4", linewidth=2.0, alpha=0.8)
    current_point = ax3.scatter([], [], [], c="#ff7f0e", s=70, depthshade=False)
    direction_line, = ax3.plot([], [], [], color="#ff2d55", linewidth=2.4, alpha=0.95)
    position_text = ax3.text2D(0.02, 0.95, "", transform=ax3.transAxes, fontsize=9)
    orientation_origin_point = ax3.scatter([], [], [], c="black", s=45, depthshade=False)
    orientation_x_axis, = ax3.plot([], [], [], color="red", linewidth=2.5, alpha=0.9)
    orientation_y_axis, = ax3.plot([], [], [], color="green", linewidth=2.5, alpha=0.9)
    orientation_z_axis, = ax3.plot([], [], [], color="blue", linewidth=2.5, alpha=0.9)
    orientation_text = ax3.text2D(0.02, 0.86, "", transform=ax3.transAxes, fontsize=9)
    
    # Triangulation visualization: estimated pinger location
    estimated_pinger_point = ax3.scatter([], [], [], c="#9d4edd", s=150, marker="*", depthshade=False, label="Est. Pinger")
    estimated_pinger_history_line, = ax3.plot([], [], [], color="#9d4edd", linewidth=2.0, alpha=0.35, label="Pinger History")
    
    # All triangulation measurements (opacity follows weight)
    triangulation_positions = ax3.scatter([], [], [], c="#00d9ff", s=50, marker="o", depthshade=False, label="Triangulation Meas.")
    triangulation_direction_lines = Line3DCollection(
        [np.array([[0.0, 0.0, 0.0], [0.0, 0.0, 0.0]], dtype=float)],
        linewidths=1.5,
    )
    triangulation_direction_lines.set_color([(0.0, 0.85, 1.0, 0.0)])
    ax3.add_collection3d(triangulation_direction_lines)

    return (
        trail_line,
        current_point,
        direction_line,
        position_text,
        orientation_origin_point,
        orientation_x_axis,
        orientation_y_axis,
        orientation_z_axis,
        orientation_text,
        estimated_pinger_point,
        estimated_pinger_history_line,
        triangulation_positions,
        triangulation_direction_lines,
    )


def set_3d_limits_from_points(ax, points, default_span=1.5):
    if points.size == 0:
        ax.set_xlim(-default_span, default_span)
        ax.set_ylim(-default_span, default_span)
        ax.set_zlim(-default_span, default_span)
        return

    mins = points.min(axis=0)
    maxs = points.max(axis=0)
    center = (mins + maxs) * 0.5
    max_span = float(np.max(maxs - mins))
    span = max(default_span, max_span * 0.5 + 0.35)

    ax.set_xlim(center[0] - span, center[0] + span)
    ax.set_ylim(center[1] - span, center[1] + span)
    ax.set_zlim(center[2] - span, center[2] + span)
    ax.invert_yaxis()
    ax.invert_zaxis()


def should_store_estimate_history(now, estimate_position):
    if estimate_position is None:
        return False

    estimate_position = np.asarray(estimate_position, dtype=float)
    if estimated_pinger_history:
        last_ts, last_position = estimated_pinger_history[-1]
        displacement = float(np.linalg.norm(estimate_position - last_position))
        if displacement <= 1e-6:
            return False
        if displacement < ESTIMATE_HISTORY_MIN_DISTANCE and (now - last_ts) < ESTIMATE_HISTORY_MIN_SECONDS:
            return False

    estimated_pinger_history.append((now, estimate_position))
    return True

def normalize_weights(triangulation_measurements):
    if not triangulation_measurements:
        return triangulation_measurements

    max_w = max(w for _, _, w in triangulation_measurements)
    if max_w <= 1e-9:
        return triangulation_measurements

    normalized = [(dir, pos, w / max_w) for dir, pos, w in triangulation_measurements]
    return normalized

def multiply_weights(triangulation_measurements, factor):
    multiplied = [(dir, pos, w * factor) for dir, pos, w in triangulation_measurements]
    return multiplied


def main():
    print(
        f"Listening for ROS2 bearing messages on topic '{TOPIC_NAME}' "
        f"with type '{MSG_TYPE}'..."
    )

    msg_cls = import_message_type(MSG_TYPE)
    rclpy = importlib.import_module("rclpy")
    executors = importlib.import_module("rclpy.executors")

    rclpy.init(args=None)
    node = rclpy.create_node(ROS_NODE_NAME)

    global triangulator
    triangulator = PingerTriangulator(min_measurements=5, max_measurements=50)

    def on_bearing(msg):
        global latest_target_id, latest_bearing_direction, latest_bearing_weight, triangulator
        global estimated_pinger_position, triangulation_measurements, filtered_measurement
        parsed = parse_bearing_message(msg)
        if parsed is None:
            return

        x, y, z, weight, target_id = parsed
        ts = time.time()
        latest_target_id = target_id
        latest_bearing_direction = np.array([x, y, z], dtype=float)
        latest_bearing_weight = weight
        #print(f"Received vector=({x:.3f}, {y:.3f}, {z:.3f}) weight={weight:.3f} target_id={target_id}")
        samples.append((ts, x, y, z, weight))
        samples_log.append((ts, x, y, z, weight))

        # Pair with recent odom for triangulation
        if odom_samples and triangulator is not None:
            _, odom_pos, odom_quat = odom_samples[-1]
            bearing_dir = np.array([x, y, z], dtype=float) # flip for use with acoustics stand?
            stored_measurements.append((ts, bearing_dir, odom_pos, odom_quat, weight))

            rotation_at_meas = Rotation.from_quat(odom_quat)
            bearing_dir_global = rotation_at_meas.apply(bearing_dir * DRONE_BODY_AXIS_SIGNS)

            stored_measurements_global.append((ts, bearing_dir_global, odom_pos, weight))

            #this is where the REAL filtering code is
            if stored_measurements_global and triangulator is not None:
                _, bearing_dir_global, odom_pos, weight = stored_measurements_global[-1]
                if filtered_measurement:
                    prev_dir, prev_pos, prev_weight = filtered_measurement
                    
                    #take weighted average of filtered measurement (assumed to be the current best measurement) and new measurement
                    combined_weight = prev_weight + weight
                    if combined_weight <= 1e-12:
                        combined_weight = 1e-12

                    blended_pos = ((prev_pos * prev_weight) + (odom_pos * weight)) / combined_weight
                    blended_dir = (prev_dir * prev_weight) + (bearing_dir_global * weight)
                    blended_norm = np.linalg.norm(blended_dir)
                    if blended_norm > 1e-9:
                        blended_dir = blended_dir / blended_norm
                    else:
                        prev_norm = np.linalg.norm(prev_dir)
                        cur_norm = np.linalg.norm(bearing_dir_global)
                        if cur_norm > prev_norm and cur_norm > 1e-9:
                            blended_dir = bearing_dir_global / cur_norm
                        elif prev_norm > 1e-9:
                            blended_dir = prev_dir / prev_norm
                        else:
                            blended_dir = bearing_dir_global
                    
                    #update filtered measurement
                    #note the new weight is the sum of the previous weights
                    filtered_measurement = (blended_dir, blended_pos, combined_weight)

                    distance = np.linalg.norm(odom_pos - blended_pos)
                    #check if filtered measurement is heavy enough and far enough away from previous measurement
                    if (combined_weight > FILTER_DROPOFF_WEIGHT) and (distance > FILTER_MERGE_DISTANCE):

                        #Reset filtered measurement weight
                        filtered_measurement = (blended_dir, blended_pos, 0)
                        #Use filtered measurement in triangulation
                        triangulation_measurements.append((blended_dir, blended_pos, combined_weight))

                        dir, pos, weight = triangulation_measurements[-1]

                        best_estimate = None
                        print(f"Best estimate: {estimated_pinger_position} | Adding measurement with weight {weight:.3f} at odom position ({odom_pos[0]:.2f}, {odom_pos[1]:.2f}, {odom_pos[2]:.2f}) and bearing direction ({bearing_dir_global[0]:.3f}, {bearing_dir_global[1]:.3f}, {bearing_dir_global[2]:.3f})")
                        best_estimate = triangulator.update(pos, dir, weight=weight)
                        if best_estimate is not None:
                            print(f"Best estimate after triangulation: {best_estimate}")
                            estimated_pinger_position = best_estimate
                            should_store_estimate_history(ts, best_estimate)

                else:
                    #if we don't have a current best measurement we define one
                    filtered_measurement = (bearing_dir_global, odom_pos, weight)

        if len(samples_log) >= MAX_SAMPLES_LOG:
            write_samples_log_to_csv(LOG_DIR + LOG_NAME + time.strftime("%Y%m%d_%H%M%S") + ".csv")
            samples_log.clear()

    def on_odom(msg):
        global latest_odom_pose, odom_reference_position
        parsed = parse_odometry_message(msg)
        if parsed is None:
            return

        pos, quat = parsed
        ts = time.time()
        if odom_reference_position is None:
            odom_reference_position = pos.copy()
        latest_odom_pose = (ts, pos, quat)
        odom_samples.append((ts, pos, quat))

    # Keep a reference to avoid subscription garbage collection.
    node._bearing_subscription = node.create_subscription(msg_cls, TOPIC_NAME, on_bearing, QOS_DEPTH)
    node._odom_subscription = node.create_subscription(Odometry, ODOM_TOPIC_NAME, on_odom, qos_profile_sensor_data)

    executor = executors.SingleThreadedExecutor()
    executor.add_node(node)

    spin_thread = threading.Thread(target=executor.spin, daemon=True)
    spin_thread.start()

    fig = plt.figure(figsize=(18, 10))

    ax3 = fig.add_axes([0.03, 0.08, 0.53, 0.84], projection="3d")
    ax2 = fig.add_axes([0.60, 0.56, 0.34, 0.36])
    ax = fig.add_axes([0.60, 0.08, 0.34, 0.36], projection="3d")
    cax = fig.add_axes([0.95, 0.18, 0.015, 0.64])

    setup_3d_axes(ax)
    density_img = setup_az_el_axes(ax2)
    (
        position_trail_line,
        position_current_point,
        direction_line,
        position_text,
        orientation_origin_point,
        orientation_x_axis,
        orientation_y_axis,
        orientation_z_axis,
        orientation_text,
        estimated_pinger_point,
        estimated_pinger_history_line,
        triangulation_positions,
        triangulation_direction_lines,
    ) = setup_global_position_axes(ax3)

    mappable = plt.cm.ScalarMappable(norm=NORM, cmap=CMAP)
    mappable.set_array([])
    cbar = fig.colorbar(mappable, cax=cax)
    cbar.set_label("Weight")

    # Dynamic artists created once
    scatter3d = ax.scatter([], [], [], s=30, depthshade=False)
    scatter_ground = ax.scatter([], [], [], s=10, depthshade=False)
    recent3d = ax.scatter([], [], [], s=90, marker="x", linewidths=2, depthshade=False)

    scatter2d = ax2.scatter([], [], s=25)
    recent2d = ax2.scatter([], [], s=90, marker="x", linewidths=2)

    latest_text_3d = ax.text2D(0.02, 0.95, "", transform=ax.transAxes, fontsize=9)
    latest_text_2d = ax2.text(0.02, 0.98, "", transform=ax2.transAxes, va="top", fontsize=9)

    def update(frame):
        global odom_reference_position
        now = time.time()
        visible = get_visible_arrays(now)
        visible_odom = get_visible_odom_arrays(now)
        bearing_visible = visible is not None
        odom_visible = visible_odom is not None

        if not bearing_visible:
            ax.set_title("Hydrophone Relative Position | waiting for ROS2 data...")
            latest_text_3d.set_text("")
            latest_text_2d.set_text("")
            density_img.set_data(np.zeros((DENSITY_EL_BINS, DENSITY_AZ_BINS), dtype=float))
            density_img.set_alpha(0.0)
            scatter3d._offsets3d = ([], [], [])
            scatter_ground._offsets3d = ([], [], [])
            recent3d._offsets3d = ([], [], [])
            scatter2d.set_offsets(np.empty((0, 2)))
            recent2d.set_offsets(np.empty((0, 2)))

        if not odom_visible:
            ax3.set_title("Drone Global Pose | waiting for ROS2 odom...")
            position_trail_line.set_data([], [])
            position_trail_line.set_3d_properties([])
            position_current_point._offsets3d = ([], [], [])
            direction_line.set_data([], [])
            direction_line.set_3d_properties([])
            position_text.set_text("")
            orientation_origin_point._offsets3d = ([], [], [])
            orientation_x_axis.set_data([], [])
            orientation_x_axis.set_3d_properties([])
            orientation_y_axis.set_data([], [])
            orientation_y_axis.set_3d_properties([])
            orientation_z_axis.set_data([], [])
            orientation_z_axis.set_3d_properties([])
            orientation_text.set_text("")
            estimated_pinger_point._offsets3d = ([], [], [])
            estimated_pinger_history_line.set_data([], [])
            estimated_pinger_history_line.set_3d_properties([])
            triangulation_positions._offsets3d = ([], [], [])
            triangulation_direction_lines.set_segments([])
            triangulation_direction_lines.set_color([])
        if bearing_visible:
            xs, ys, zs, snrs, colors, alphas, ages = visible

            # 3D cloud
            scatter3d._offsets3d = (xs, ys, zs)
            scatter3d.set_facecolors(colors)
            scatter3d.set_edgecolors(colors)

            # Ground projection
            scatter_ground._offsets3d = (xs, ys, np.full_like(zs, -1.5))
            scatter_ground.set_facecolors(colors)
            scatter_ground.set_edgecolors(colors)

            # Latest point info in text
            x_last, y_last, z_last, snr_last = xs[-1], ys[-1], zs[-1], snrs[-1]

            # Mark all points newer than NEW_POINT_MARKER_SECONDS with fading X markers
            recent_mask = ages <= NEW_POINT_MARKER_SECONDS
            if np.any(recent_mask):
                rx = xs[recent_mask]
                ry = ys[recent_mask]
                rz = zs[recent_mask]
                recent_ages = ages[recent_mask]
                recent_marker_alpha = np.clip(1.0 - recent_ages / NEW_POINT_MARKER_SECONDS, 0.0, 1.0)
                recent_colors = np.zeros((len(rx), 4), dtype=float)
                recent_colors[:, 0] = 1.0
                recent_colors[:, 3] = recent_marker_alpha

                recent3d._offsets3d = (rx, ry, rz)
                recent3d.set_facecolors(recent_colors)
                recent3d.set_edgecolors(recent_colors)
            else:
                recent3d._offsets3d = ([], [], [])

            # Azimuth / elevation
            az = -np.degrees(np.arctan2(ys, xs))
            el = np.degrees(np.arcsin(np.clip(zs, -1.0, 1.0)))

            density = build_az_el_density(az, el, alphas)
            density_img.set_data(density)
            density_img.set_alpha(DENSITY_ALPHA_MAX)

            scatter2d.set_offsets(np.column_stack((az, el)))
            scatter2d.set_facecolors(colors)
            scatter2d.set_edgecolors(colors)

            az_last = az[-1]
            el_last = el[-1]

            if np.any(recent_mask):
                raz = az[recent_mask]
                rel = el[recent_mask]
                recent_ages = ages[recent_mask]
                recent_marker_alpha = np.clip(1.0 - recent_ages / NEW_POINT_MARKER_SECONDS, 0.0, 1.0)
                recent_colors = np.zeros((len(raz), 4), dtype=float)
                recent_colors[:, 0] = 1.0
                recent_colors[:, 3] = recent_marker_alpha

                recent2d.set_offsets(np.column_stack((raz, rel)))
                recent2d.set_facecolors(recent_colors)
                recent2d.set_edgecolors(recent_colors)
            else:
                recent2d.set_offsets(np.empty((0, 2)))

            target_text = f"target_id: {latest_target_id}" if latest_target_id is not None else "target_id: n/a"
            ax.set_title(
                f"Hydrophone Relative Position | "
                f"Visible samples: {len(xs)} | "
                f"Latest weight: {snr_last:.3f} | {target_text}"
            )
            latest_text_3d.set_text(f"latest: ({x_last:.2f}, {y_last:.2f}, {z_last:.2f})")
            latest_text_2d.set_text(f"latest: ({az_last:.1f}°, {el_last:.1f}°)")
        else:
            latest_text_3d.set_text("")
            latest_text_2d.set_text("")
            density_img.set_data(np.zeros((DENSITY_EL_BINS, DENSITY_AZ_BINS), dtype=float))
            density_img.set_alpha(0.0)
            scatter3d._offsets3d = ([], [], [])
            scatter_ground._offsets3d = ([], [], [])
            recent3d._offsets3d = ([], [], [])
            scatter2d.set_offsets(np.empty((0, 2)))
            recent2d.set_offsets(np.empty((0, 2)))

        if visible_odom is not None:
            odom_positions, odom_quats, _, _ = visible_odom
            if odom_reference_position is None:
                odom_reference_position = odom_positions[0].copy()

            odom_positions_rel = odom_positions # - odom_reference_position
            latest_position = odom_positions_rel[-1]
            latest_quat = odom_quats[-1]
            latest_rotation = Rotation.from_quat(latest_quat)

            position_trail_line.set_data(odom_positions_rel[:, 0], odom_positions_rel[:, 1])
            position_trail_line.set_3d_properties(odom_positions_rel[:, 2])
            position_current_point._offsets3d = ([latest_position[0]], [latest_position[1]], [latest_position[2]])

            set_3d_limits_from_points(ax3, odom_positions_rel, default_span=3.0)
            position_text.set_text(
                f"latest: ({odom_positions[-1][0]:.2f}, {odom_positions[-1][1]:.2f}, {odom_positions[-1][2]:.2f})"
            )

            pinger_endpoint = None
            if latest_bearing_direction is not None:
                direction_scale = 1.25
                bearing_direction_global = latest_rotation.apply(latest_bearing_direction * DRONE_BODY_AXIS_SIGNS)
                pinger_endpoint = latest_position + bearing_direction_global * direction_scale
                direction_line.set_data(
                    [latest_position[0], pinger_endpoint[0]],
                    [latest_position[1], pinger_endpoint[1]],
                )
                direction_line.set_3d_properties([latest_position[2], pinger_endpoint[2]])
            else:
                direction_line.set_data([], [])
                direction_line.set_3d_properties([])

            body_axes = latest_rotation.apply(np.array([
                [1.0, 0.0, 0.0],
                [0.0, 1.0, 0.0],
                [0.0, 0.0, 1.0],
            ], dtype=float))
            axis_scale = 0.9
            axis_endpoints = latest_position + body_axes * axis_scale

            orientation_origin_point._offsets3d = ([latest_position[0]], [latest_position[1]], [latest_position[2]])
            for line, endpoint in zip(
                (orientation_x_axis, orientation_y_axis, orientation_z_axis),
                axis_endpoints,
            ):
                line.set_data([latest_position[0], endpoint[0]], [latest_position[1], endpoint[1]])
                line.set_3d_properties([latest_position[2], endpoint[2]])

            if pinger_endpoint is not None:
                limit_points = np.vstack([odom_positions_rel, axis_endpoints, pinger_endpoint])
            else:
                limit_points = np.vstack([odom_positions_rel, axis_endpoints])
            set_3d_limits_from_points(ax3, limit_points, default_span=3.0)

            roll, pitch, yaw = latest_rotation.as_euler("xyz", degrees=True)
            orientation_text.set_text(f"rpy: ({roll:.1f}°, {pitch:.1f}°, {yaw:.1f}°)")

            # ===== TRIANGULATION & TOP 10 VISUALIZATION =====
            global estimated_pinger_position, triangulation_measurements, triangulator
            

            
            # Visualization: estimated pinger location
            if estimated_pinger_position is not None:
                est_pos_rel = estimated_pinger_position  # Already in absolute coords; could also use relative
                estimated_pinger_point._offsets3d = (
                    [est_pos_rel[0]],
                    [est_pos_rel[1]],
                    [est_pos_rel[2]],
                )
            else:
                estimated_pinger_point._offsets3d = ([], [], [])

            if estimated_pinger_history:
                history_positions = np.array([pos for _, pos in estimated_pinger_history], dtype=float)
                estimated_pinger_history_line.set_data(history_positions[:, 0], history_positions[:, 1])
                estimated_pinger_history_line.set_3d_properties(history_positions[:, 2])
            else:
                estimated_pinger_history_line.set_data([], [])
                estimated_pinger_history_line.set_3d_properties([])
            
            # Visualization: all triangulation measurements and directions
            if triangulation_measurements:
                meas_positions = np.array([op for  _, op, _ in triangulation_measurements], dtype=float)
                meas_weights = np.array([w for  _, _, w in triangulation_measurements], dtype=float)
                max_weight = float(np.max(meas_weights)) if meas_weights.size else 0.0
                if max_weight <= 1e-12:
                    max_weight = 1.0

                position_alphas = np.clip(meas_weights / max_weight, 0.08, 1.0)
                position_colors = np.tile(np.array([[0.0, 0.85, 1.0, 1.0]], dtype=float), (len(meas_positions), 1))
                position_colors[:, 3] = position_alphas
                triangulation_positions._offsets3d = (
                    meas_positions[:, 0],
                    meas_positions[:, 1],
                    meas_positions[:, 2],
                )
                triangulation_positions.set_facecolors(position_colors)
                triangulation_positions.set_edgecolors(position_colors)

                direction_segments = []
                direction_colors = []
                direction_endpoints = []
                direction_scale = 1.0
                for bearing_dir_global, odom_pos, weight in triangulation_measurements:
                    bearing_norm = np.linalg.norm(bearing_dir_global)
                    if bearing_norm <= 1e-9:
                        continue
                    endpoint = odom_pos + (bearing_dir_global / bearing_norm) * direction_scale
                    direction_segments.append(np.array([odom_pos, endpoint], dtype=float))
                    direction_endpoints.append(endpoint)
                    alpha = float(np.clip(weight / max_weight, 0.08, 1.0))
                    direction_colors.append((0.0, 0.85, 1.0, alpha))

                triangulation_direction_lines.set_segments(direction_segments)
                triangulation_direction_lines.set_color(direction_colors)
            else:
                triangulation_positions._offsets3d = ([], [], [])
                triangulation_direction_lines.set_segments([])
                triangulation_direction_lines.set_color([])
            
            # Include estimated pinger and triangulation measurement positions in limit calculation
            limit_points_list = [odom_positions_rel, axis_endpoints]
            if estimated_pinger_position is not None:
                limit_points_list.append(np.array([estimated_pinger_position]))
            if estimated_pinger_history:
                limit_points_list.append(np.array([pos for _, pos in estimated_pinger_history], dtype=float))
            if triangulation_measurements:
                limit_points_list.append(meas_positions)
                if direction_endpoints:
                    limit_points_list.append(np.array(direction_endpoints, dtype=float))
            limit_points = np.vstack(limit_points_list)
            set_3d_limits_from_points(ax3, limit_points, default_span=3.0)

        return (
            density_img,
            scatter3d,
            scatter_ground,
            recent3d,
            scatter2d,
            recent2d,
            position_trail_line,
            position_current_point,
            direction_line,
            orientation_origin_point,
            orientation_x_axis,
            orientation_y_axis,
            orientation_z_axis,
            latest_text_3d,
            latest_text_2d,
            position_text,
            orientation_text,
            estimated_pinger_point,
            estimated_pinger_history_line,
            triangulation_positions,
            triangulation_direction_lines,
        )

    anim = FuncAnimation(
        fig,
        update,
        interval=PLOT_INTERVAL_MS,
        cache_frame_data=False,
        blit=False,
    )

    try:
        plt.show()
    finally:
        executor.shutdown()
        node.destroy_node()
        rclpy.shutdown()

    return anim


if __name__ == "__main__":
    main()