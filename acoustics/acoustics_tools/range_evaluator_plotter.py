import matplotlib.pyplot as plt
import numpy as np
import matplotlib as mpl  # <-- add
from matplotlib.ticker import PercentFormatter

from range_evaluator import COUNT_PER_RANGE, OUT_DIR

no_noise_dir = OUT_DIR.parent / "test_data_no_noise"

load_file = no_noise_dir / "fullstack_error_vs_range.npz"#OUT_DIR / "fullstack_error_vs_range.npz"

data = np.load(load_file, allow_pickle=True)
ranges_list = data["ranges_m"]
all_direction_errors = [np.array(arr) for arr in data["all_direction_errors_deg"]]
all_position_errors_deg = [np.array(arr) for arr in data["all_position_errors_deg"]]
all_position_errors_abs = [np.array(arr) for arr in data["all_position_errors_m"]]
fail_direction_counts = data["fail_direction_counts"]
fail_position_counts = data["fail_position_counts"]

all_direction_errors = [arr[~np.isnan(arr)] for arr in all_direction_errors]
all_position_errors_deg = [arr[~np.isnan(arr)] for arr in all_position_errors_deg]
all_position_errors_abs = [arr[~np.isnan(arr)] for arr in all_position_errors_abs]

#print(fail_direction_counts)

median_style = dict(color="#ff0000", linewidth=1.5)
width = 0.8 * (ranges_list[1] - ranges_list[0])
show_means = False

plt.style.use('dark_background')

fig, axs = plt.subplots(3,1,figsize=(12, 6), constrained_layout=True)

bp0 = axs[0].boxplot(all_direction_errors, positions=ranges_list, widths=width, showmeans=show_means, patch_artist=True, medianprops=median_style)
axs[0].set_ylim(0, 45/6)
axs[0].set_xlabel("Range (m)")
axs[0].set_ylabel("Direction Error (deg)")

bp1 = axs[1].boxplot(all_position_errors_deg, positions=ranges_list, widths=width, showmeans=show_means, patch_artist=True, medianprops=median_style)
axs[1].set_ylim(0, 45/6)
axs[1].set_xlabel("Range (m)")
axs[1].set_ylabel("Position Error (deg)")

bp2 = axs[2].boxplot(all_position_errors_abs, positions=ranges_list, widths=width, showmeans=show_means, patch_artist=True, medianprops=median_style)
axs[2].set_yscale("log")
axs[2].set_xlabel("Range (m)")
axs[2].set_ylabel("Position Error (m)")
axs[2].grid(True)

fail_rates_dir = fail_direction_counts.astype(float) / float(COUNT_PER_RANGE)  # 0..1
fail_rates_pos = fail_position_counts.astype(float) / float(COUNT_PER_RANGE)

# White (0% fail) -> Red (100% fail)
""" cmap = mpl.colors.LinearSegmentedColormap.from_list(
    "white_to_red",
    ["#ffffff", "#ff0000"],
)
norm = mpl.colors.Normalize(vmin=0.0, vmax=1.0) """

cmap = plt.cm.viridis
norm = mpl.colors.Normalize(vmin=0.0, vmax=1.0)

def color_boxes(bp, rates):
    for patch, r in zip(bp["boxes"], rates):
        r = float(np.clip(r, 0.0, 1.0))
        patch.set_facecolor(cmap(norm(r)))
        patch.set_edgecolor("black")
        patch.set_alpha(1.0)  # keep white truly white

color_boxes(bp0, fail_rates_dir)
color_boxes(bp1, fail_rates_pos)
color_boxes(bp2, fail_rates_pos)

# Optional: add a colorbar legend for fail-rate
sm = mpl.cm.ScalarMappable(norm=norm, cmap=cmap)
sm.set_array([])
cbar = fig.colorbar(sm, ax=axs, fraction=0.02, pad=0.05)
cbar.ax.set_title("Failure rate", pad=6)
cbar.ax.yaxis.set_major_formatter(PercentFormatter(xmax=1.0))

plt.suptitle("Fullstack Capture Error vs. Range")
fig.savefig("error_no_noise_new.png", dpi=300, transparent=True)
plt.show()