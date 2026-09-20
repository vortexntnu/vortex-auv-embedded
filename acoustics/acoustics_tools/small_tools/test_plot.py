import time
import random
from collections import deque

import numpy as np
import matplotlib.pyplot as plt
from matplotlib.animation import FuncAnimation
from matplotlib import colors as mcolors
from mpl_toolkits.mplot3d import Axes3D  # noqa: F401


# =========================
# Configuration
# =========================
MAX_SAMPLES = 200
FADE_SECONDS = 4.0
PLOT_INTERVAL_MS = 100   # was 50; a little slower refresh often feels smoother overall

SNR_MIN = 0.0
SNR_MAX = 50.0

STEP_STD = 0.08
SNR_STEP_STD = 1.5

hydrophones = np.array([
    [0.0, 0.0, 0.0],
    [0.0, 0.0, 0.0],
    [0.45, 0.0, 0.0],
    [0.0, 0.45, 0.0],
    [0.0, 0.0, -0.45],
], dtype=float)


# =========================
# Data storage
# =========================
samples = deque(maxlen=MAX_SAMPLES)

state = {
    "x": 1.0,
    "y": -0.5,
    "z": 0.8,
    "snr": 20.0,
}


# Precompute unit sphere once
u = np.linspace(0, 2 * np.pi, 30)
v = np.linspace(0, np.pi, 20)
SPHERE_X = np.outer(np.cos(u), np.sin(v))
SPHERE_Y = np.outer(np.sin(u), np.sin(v))
SPHERE_Z = np.outer(np.ones_like(u), np.cos(v))

NORM = mcolors.Normalize(vmin=SNR_MIN, vmax=SNR_MAX)
CMAP = plt.cm.viridis


def simulate_next_point():
    x = state["x"]
    y = state["y"]
    z = state["z"]
    snr = state["snr"]

    x += random.gauss(0, STEP_STD)
    y += random.gauss(0, STEP_STD)
    z += random.gauss(0, STEP_STD)

    norm = (x * x + y * y + z * z) ** 0.5
    if norm > 1e-6:
        x /= norm
        y /= norm
        z /= norm

    snr += random.gauss(0, SNR_STEP_STD)
    snr = max(SNR_MIN, min(SNR_MAX, snr))

    state["x"], state["y"], state["z"], state["snr"] = x, y, z, snr
    return x, y, z, snr


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

    return xs, ys, zs, snrs, colors


def setup_3d_axes(ax):
    ax.set_xlabel("X")
    ax.set_ylabel("Y")
    ax.set_zlabel("Z")
    ax.set_xlim(-1.5, 1.5)
    ax.set_ylim(-1.5, 1.5)
    ax.set_zlim(-1.5, 1.5)
    ax.set_box_aspect([1, 1, 1])

    # Reference axes
    # ax.plot([0, 1.2], [0, 0], [0, 0], linewidth=2)
    # ax.plot([0, 0], [0, 1.2], [0, 0], linewidth=2)
    # ax.plot([0, 0], [0, 0], [0, 1.2], linewidth=2)
    #
    # ax.text(1.25, 0, 0, "+X", fontsize=10)
    # ax.text(0, 1.25, 0, "+Y", fontsize=10)
    # ax.text(0, 0, 1.25, "+Z", fontsize=10)
    #
    # Unit sphere
    ax.plot_wireframe(SPHERE_X, SPHERE_Y, SPHERE_Z, alpha=0.08)

    # Reference hydrophone
    ref = hydrophones[0]
    ax.scatter([ref[0]], [ref[1]], [ref[2]], c="black", s=80, marker="o", depthshade=False)
    # ax.text(ref[0], ref[1], ref[2], " H0 (ref)", fontsize=9)

    # Hydrophones + lines from H0
    for i, (hx, hy, hz) in enumerate(hydrophones):
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


def main():
    fig = plt.figure(figsize=(15, 7))

    ax = fig.add_axes([0.05, 0.10, 0.42, 0.80], projection="3d")
    ax2 = fig.add_axes([0.55, 0.10, 0.32, 0.80])
    cax = fig.add_axes([0.90, 0.15, 0.02, 0.70])

    setup_3d_axes(ax)
    setup_az_el_axes(ax2)

    mappable = plt.cm.ScalarMappable(norm=NORM, cmap=CMAP)
    mappable.set_array([])
    cbar = fig.colorbar(mappable, cax=cax)
    cbar.set_label("SNR")

    # Dynamic artists created once
    scatter3d = ax.scatter([], [], [], s=30, depthshade=False)
    scatter_ground = ax.scatter([], [], [], s=10, depthshade=False)
    latest3d = ax.scatter([], [], [], s=100, marker="x", linewidths=2, depthshade=False)

    scatter2d = ax2.scatter([], [], s=25)
    latest2d = ax2.scatter([], [], s=100, marker="x", linewidths=2)

    latest_text_3d = ax.text2D(0.02, 0.95, "", transform=ax.transAxes, fontsize=9)
    latest_text_2d = ax2.text(0.02, 0.98, "", transform=ax2.transAxes, va="top", fontsize=9)

    # Seed first point
    x, y, z, snr = simulate_next_point()
    samples.append((time.time(), x, y, z, snr))

    def update(frame):
        x, y, z, snr = simulate_next_point()
        samples.append((time.time(), x, y, z, snr))

        visible = get_visible_arrays(time.time())
        if visible is None:
            return scatter3d, scatter_ground, latest3d, scatter2d, latest2d, latest_text_3d, latest_text_2d

        xs, ys, zs, snrs, colors = visible

        # 3D cloud
        scatter3d._offsets3d = (xs, ys, zs)
        scatter3d.set_facecolors(colors)
        scatter3d.set_edgecolors(colors)

        # Ground projection
        scatter_ground._offsets3d = (xs, ys, np.full_like(zs, -1.5))
        scatter_ground.set_facecolors(colors)
        scatter_ground.set_edgecolors(colors)

        # Latest point
        x_last, y_last, z_last, snr_last = xs[-1], ys[-1], zs[-1], snrs[-1]
        latest3d._offsets3d = ([x_last], [y_last], [z_last])
        latest3d.set_facecolors([[1.0, 0.0, 0.0, 1.0]])
        latest3d.set_edgecolors([[1.0, 0.0, 0.0, 1.0]])

        # Az/el
        az = np.degrees(np.arctan2(ys, xs))
        el = np.degrees(np.arcsin(np.clip(zs, -1.0, 1.0)))
        offsets2d = np.column_stack((az, el))
        scatter2d.set_offsets(offsets2d)
        scatter2d.set_facecolors(colors)
        scatter2d.set_edgecolors(colors)

        az_last = az[-1]
        el_last = el[-1]
        latest2d.set_offsets([[az_last, el_last]])
        latest2d.set_facecolors([[1.0, 0.0, 0.0, 1.0]])
        latest2d.set_edgecolors([[1.0, 0.0, 0.0, 1.0]])

        # Text / title updates only
        ax.set_title(
            f"Simulated Hydrophone Relative Position | "
            f"Visible samples: {len(xs)} | "
            f"Latest SNR: {snr_last:.1f}"
        )
        latest_text_3d.set_text(f"latest: ({x_last:.2f}, {y_last:.2f}, {z_last:.2f})")
        latest_text_2d.set_text(f"latest: ({az_last:.1f}°, {el_last:.1f}°)")

        return scatter3d, scatter_ground, latest3d, scatter2d, latest2d, latest_text_3d, latest_text_2d

    anim = FuncAnimation(
        fig,
        update,
        interval=PLOT_INTERVAL_MS,
        cache_frame_data=False,
        blit=False,  # 3D matplotlib generally does not play nicely with blit
    )

    plt.show()
    return anim


if __name__ == "__main__":
    main()
