import time
import struct
import csv
from collections import deque

import can
import numpy as np
import matplotlib.pyplot as plt
from matplotlib.animation import FuncAnimation
from matplotlib import colors as mcolors
from mpl_toolkits.mplot3d import Axes3D  # noqa: F401


# =========================
# CAN Configuration
# =========================
CAN_ID = 0x200
INTERFACE = "slcan"
SERIAL_PORT = "COM9"
SERIAL_BAUDRATE = 115200
BITRATE = 500000
DATA_BITRATE = 2000000
FD = True


# =========================
# Plot Configuration
# =========================
MAX_SAMPLES = 200
MAX_SAMPLES_LOG = 1024  # For logging all received samples, not just visible ones
FADE_SECONDS = 30.0
PLOT_INTERVAL_MS = 100

SNR_MIN = 0.0
SNR_MAX = 30.0

DENSITY_AZ_BINS = 180
DENSITY_EL_BINS = 90
DENSITY_SIGMA_BINS = 2.7
DENSITY_ALPHA_MAX = 0.6
NEW_POINT_MARKER_SECONDS = 1.0

hydrophones_local = np.array([
    [0.0, 0.0, 0.0],
    [0.0, 0.0, 0.0],
    [32.3, -4.3, 3.66],
    [-6.6, 34.7, 1.66],
    [-4.6, 0.0, -34.34]
], dtype=float)

longest_hydrophone_length = np.linalg.norm(hydrophones_local, axis=1).max()
hydrophones_local /= longest_hydrophone_length * 0.5  # Scale down for better visualization

hydrophones_pitch = 90 - 19.323971
hydrophones_yaw = 45.0

# =========================
# Data storage
# =========================
LOG_DIR = "small_tools/can_log_samples/"
LOG_NAME = f"can_samples_log_"
samples = deque(maxlen=MAX_SAMPLES)
samples_log = deque(maxlen=MAX_SAMPLES_LOG)  # For logging all received samples, not just visible ones


# Precompute unit sphere once
u = np.linspace(0, 2 * np.pi, 30)
v = np.linspace(0, np.pi, 20)
SPHERE_X = np.outer(np.cos(u), np.sin(v))
SPHERE_Y = np.outer(np.sin(u), np.sin(v))
SPHERE_Z = np.outer(np.ones_like(u), np.cos(v))

NORM = mcolors.Normalize(vmin=SNR_MIN, vmax=SNR_MAX)
CMAP = plt.cm.viridis


def rotate_vector_x(vector, degrees):
    v = np.asarray(vector, dtype=float)
    if v.shape != (3,):
        raise ValueError("vector must be a 3-element vector")

    theta = np.deg2rad(degrees)
    c = np.cos(theta)
    s = np.sin(theta)
    rotation = np.array([
        [1.0, 0.0, 0.0],
        [0.0, c, -s],
        [0.0, s, c],
    ])
    return rotation @ v


def rotate_vector_y(vector, degrees):
    v = np.asarray(vector, dtype=float)
    if v.shape != (3,):
        raise ValueError("vector must be a 3-element vector")

    theta = np.deg2rad(degrees)
    c = np.cos(theta)
    s = np.sin(theta)
    rotation = np.array([
        [c, 0.0, s],
        [0.0, 1.0, 0.0],
        [-s, 0.0, c],
    ])
    return rotation @ v


def rotate_vector_z(vector, degrees):
    v = np.asarray(vector, dtype=float)
    if v.shape != (3,):
        raise ValueError("vector must be a 3-element vector")

    theta = np.deg2rad(degrees)
    c = np.cos(theta)
    s = np.sin(theta)
    rotation = np.array([
        [c, -s, 0.0],
        [s, c, 0.0],
        [0.0, 0.0, 1.0],
    ])
    return rotation @ v

def hydrophone_local_to_global(local):
    rotated = rotate_vector_z(local, -hydrophones_yaw)
    rotated = rotate_vector_y(rotated, hydrophones_pitch)
    return rotated

hydrophones_global = hydrophones_local.copy()  # Store original positions for global reference
for i in range(len(hydrophones_local)):
    hydrophones_global[i] = hydrophone_local_to_global(hydrophones_local[i])


def create_bus():
    return can.Bus(
        interface=INTERFACE,
        channel=f"{SERIAL_PORT}@{SERIAL_BAUDRATE}",
        bitrate=BITRATE,
        data_bitrate=DATA_BITRATE,
        fd=FD,
    )


def parse_message(data: bytes):
    if len(data) != 16:
        print(f"Received message with invalid data length {len(data)}, expected 16 bytes.")
        return None

    try:
        x, y, z, snr = struct.unpack("<4f", data)
    except struct.error:
        print("Received message with invalid data format, unable to unpack.")
        return None

    norm = (x * x + y * y + z * z) ** 0.5
    if norm <= 1e-9:
        print("Received message with near-zero vector, ignoring.")
        return None

    x /= norm
    y /= norm
    z /= norm

    try:
        snr = 10.*np.log10(snr)
    except (struct.error, np.linalg.LinAlgError):
        print("SNR value is invalid, unable to convert to dB.")
        return None
    
    vec = np.array([x, y, z])
    vec = hydrophone_local_to_global(vec)
    x, y, z = vec.tolist()

    return x, y, z, snr


def poll_can_messages(bus):
    """
    Read all currently available CAN messages without blocking.
    """
    count = 0
    while True:
        msg = bus.recv(timeout=0.0)
        if msg is None:
            break

        if msg.arbitration_id != CAN_ID:
            print(f"Received message with unexpected ID 0x{msg.arbitration_id:X}, ignoring.")
            continue

        parsed = parse_message(bytes(msg.data))
        if parsed is None:
            print(f"Received message with invalid data, ignoring.")
            continue

        x, y, z, snr = parsed
        print(f"Received vector=({x:.3f}, {y:.3f}, {z:.3f}) weight={snr:.2f}")
        samples.append((time.time(), x, y, z, snr))
        samples_log.append((time.time(), x, y, z, snr))
        if(len(samples_log) >= MAX_SAMPLES_LOG):
            write_samples_log_to_csv(LOG_DIR + LOG_NAME + time.strftime("%Y%m%d_%H%M%S") + ".csv")
            samples_log.clear()
        count += 1

    return count


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
        writer.writerow(["timestamp", "x", "y", "z", "snr_db"])
        writer.writerows(samples_log)

    print(f"Saved {len(samples_log)} samples to {path}")


def setup_3d_axes(ax):
    ax.set_xlabel("X")
    ax.set_ylabel("Y")
    ax.set_zlabel("Z")
    ax.set_xlim(-1.5, 1.5)
    ax.set_ylim(-1.5, 1.5)
    ax.set_zlim(-1.5, 1.5)
    ax.set_box_aspect([1, 1, 1])

    ax.plot_wireframe(SPHERE_X, SPHERE_Y, SPHERE_Z, alpha=0.08)

    # Reference hydrophone
    ref = hydrophones_global[0]
    ax.scatter([ref[0]], [ref[1]], [ref[2]], c="black", s=80, marker="o", depthshade=False)

    # Hydrophones + lines from H0
    for i, (hx, hy, hz) in enumerate(hydrophones_global):
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


def main():
    print(
        f"Listening for CAN FD ID 0x{CAN_ID:X} on {INTERFACE}, "
        f"port={SERIAL_PORT}, tty_baudrate={SERIAL_BAUDRATE}..."
    )

    bus = create_bus()

    fig = plt.figure(figsize=(15, 7))

    ax = fig.add_axes([0.05, 0.10, 0.42, 0.80], projection="3d")
    ax2 = fig.add_axes([0.55, 0.10, 0.32, 0.80])
    cax = fig.add_axes([0.90, 0.15, 0.02, 0.70])

    setup_3d_axes(ax)
    density_img = setup_az_el_axes(ax2)

    mappable = plt.cm.ScalarMappable(norm=NORM, cmap=CMAP)
    mappable.set_array([])
    cbar = fig.colorbar(mappable, cax=cax)
    cbar.set_label("SNR")

    # Dynamic artists created once
    scatter3d = ax.scatter([], [], [], s=30, depthshade=False)
    scatter_ground = ax.scatter([], [], [], s=10, depthshade=False)
    recent3d = ax.scatter([], [], [], s=90, marker="x", linewidths=2, depthshade=False)

    scatter2d = ax2.scatter([], [], s=25)
    recent2d = ax2.scatter([], [], s=90, marker="x", linewidths=2)

    latest_text_3d = ax.text2D(0.02, 0.95, "", transform=ax.transAxes, fontsize=9)
    latest_text_2d = ax2.text(0.02, 0.98, "", transform=ax2.transAxes, va="top", fontsize=9)

    def update(frame):
        poll_can_messages(bus)

        visible = get_visible_arrays(time.time())
        if visible is None:
            ax.set_title("Hydrophone Relative Position | waiting for CAN data...")
            latest_text_3d.set_text("")
            latest_text_2d.set_text("")
            density_img.set_data(np.zeros((DENSITY_EL_BINS, DENSITY_AZ_BINS), dtype=float))
            density_img.set_alpha(0.0)
            scatter3d._offsets3d = ([], [], [])
            scatter_ground._offsets3d = ([], [], [])
            recent3d._offsets3d = ([], [], [])
            scatter2d.set_offsets(np.empty((0, 2)))
            recent2d.set_offsets(np.empty((0, 2)))
            return density_img, scatter3d, scatter_ground, recent3d, scatter2d, recent2d, latest_text_3d, latest_text_2d

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

        ax.set_title(
            f"Hydrophone Relative Position | "
            f"Visible samples: {len(xs)} | "
            f"Latest SNR: {snr_last:.1f}"
        )
        latest_text_3d.set_text(f"latest: ({x_last:.2f}, {y_last:.2f}, {z_last:.2f})")
        latest_text_2d.set_text(f"latest: ({az_last:.1f}°, {el_last:.1f}°)")

        return density_img, scatter3d, scatter_ground, recent3d, scatter2d, recent2d, latest_text_3d, latest_text_2d

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
        bus.shutdown()

    return anim


if __name__ == "__main__":
    main()