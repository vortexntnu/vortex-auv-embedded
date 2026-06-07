import argparse
import csv
from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np
from matplotlib import colors as mcolors


# Match live CAN reader visualization settings
SNR_MIN = 0.0
SNR_MAX = 50.0
# Set to None to load all rows, or an integer to load only the latest N rows.
MAX_LOADED_SAMPLES = 30000

hydrophones_local = np.array([
	[0.0, 0.0, 0.0],
	[0.0, 0.0, 0.0],
	[32.3, -4.3, 3.66],
	[-6.6, 34.7, 1.66],
	[-4.6, 0.0, -34.34],
], dtype=float)

longest_hydrophone_length = np.linalg.norm(hydrophones_local, axis=1).max()
hydrophones_local /= longest_hydrophone_length * 0.5

hydrophones_pitch = 90 - 19.323971
hydrophones_yaw = 45.0

u = np.linspace(0, 2 * np.pi, 30)
v = np.linspace(0, np.pi, 20)
SPHERE_X = np.outer(np.cos(u), np.sin(v))
SPHERE_Y = np.outer(np.sin(u), np.sin(v))
SPHERE_Z = np.outer(np.ones_like(u), np.cos(v))

NORM = mcolors.Normalize(vmin=SNR_MIN, vmax=SNR_MAX)
CMAP = plt.cm.viridis


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

def hydrophone_global_to_local(global_pos):
	rotated = rotate_vector_y(global_pos, -hydrophones_pitch)
	rotated = rotate_vector_z(rotated, hydrophones_yaw)
	return rotated


hydrophones_global = hydrophones_local.copy()
for i in range(len(hydrophones_local)):
	hydrophones_global[i] = hydrophone_local_to_global(hydrophones_local[i])


def load_csv_rows(csv_path: Path):
	rows = []
	with csv_path.open("r", newline="", encoding="utf-8") as f:
		reader = csv.DictReader(f)
		required = {"timestamp", "x", "y", "z", "snr_db"}
		if reader.fieldnames is None or not required.issubset(set(reader.fieldnames)):
			raise ValueError(
				f"{csv_path} missing required columns. "
				f"Expected: {sorted(required)} | Found: {reader.fieldnames}"
			)

		for row in reader:
			rows.append(
				(
					float(row["timestamp"]),
					float(row["x"]),
					float(row["y"]),
					float(row["z"]),
					float(row["snr_db"]),
				)
			)
	return rows


def collect_files(pattern: str, file_arg: str):
	files = []
	if file_arg:
		files.append(Path(file_arg))
	else:
		files = sorted(Path().glob(pattern))

	existing = [f for f in files if f.exists() and f.is_file()]
	if not existing:
		raise FileNotFoundError(
			f"No CSV files found. file={file_arg!r}, pattern={pattern!r}"
		)
	return existing


def load_data(files, max_loaded_samples=None):
	all_rows = []
	for f in files:
		all_rows.extend(load_csv_rows(f))

	if not all_rows:
		raise ValueError("CSV files were found but contain no data rows.")

	arr = np.array(all_rows, dtype=float)
	order = np.argsort(arr[:, 0])
	arr = arr[order]

	if max_loaded_samples is not None:
		max_loaded_samples = int(max_loaded_samples)
		if max_loaded_samples <= 0:
			raise ValueError("max_loaded_samples must be > 0 or None")
		if len(arr) > max_loaded_samples:
			arr = arr[-max_loaded_samples:]

	return arr


def setup_3d_axes(ax):
	ax.set_xlabel("X")
	ax.set_ylabel("Y")
	ax.set_zlabel("Z")
	ax.set_xlim(-1.5, 1.5)
	ax.set_ylim(-1.5, 1.5)
	ax.set_zlim(-1.5, 1.5)
	ax.set_box_aspect([1, 1, 1])

	ax.plot_wireframe(SPHERE_X, SPHERE_Y, SPHERE_Z, alpha=0.08)

	ref = hydrophones_global[0]
	ax.scatter([ref[0]], [ref[1]], [ref[2]], c="black", s=80, marker="o", depthshade=False)

	for i, (hx, hy, hz) in enumerate(hydrophones_global):
		tax = ax
		tax.scatter([hx], [hy], [hz], c="red", s=60, marker="^", depthshade=False)
		tax.text(hx, hy, hz, f" H{i}", fontsize=8)

		if i != 0:
			tax.plot(
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


def apply_filter_to_unfiltered(arr, filtered_mask, filter_func, filter_name="filter"):
	"""
	Apply one filter only on currently unfiltered rows.

	filter_func must take sub-array rows and return a boolean mask (True = filter out).
	"""
	if filtered_mask.shape != (len(arr),):
		raise ValueError("filtered_mask must be a 1D boolean array with len(arr) elements")

	remaining_idx = np.where(~filtered_mask)[0]
	if remaining_idx.size == 0:
		return filtered_mask

	remaining_arr = arr[remaining_idx]
	local_mask = np.asarray(filter_func(remaining_arr), dtype=bool)
	if local_mask.shape != (len(remaining_arr),):
		raise ValueError(f"{filter_name} returned invalid mask shape {local_mask.shape}")

	filtered_mask[remaining_idx[local_mask]] = True
	return filtered_mask


def filter_by_snr_threshold(arr, snr_min=5.0):
	"""
	Template filter: filter out rows with snr_db below snr_min.
	"""
	return arr[:, 4] < float(snr_min)


def filter_by_knn_distance_sum(arr, n_neighbors=100, threshold=2.5, mode="greater"):
	"""
	Template filter: filter rows using k-NN distance-sum threshold in xyz space.
	"""
	vectors = arr[:, 1:4]
	filtered_mask, _scores = cluster_filter_by_neighbor_distance_sum(
		vectors,
		n_neighbors=n_neighbors,
		threshold=threshold,
		mode=mode,
	)
	return filtered_mask


def filter_non_finite_values(arr):
	"""
	Template filter: remove rows with NaN/Inf in x,y,z,snr.
	"""
	return ~np.isfinite(arr[:, 1:5]).all(axis=1)


def filter_template_custom(arr):
	"""
	Template placeholder for custom logic.

	Return True for rows you want filtered out.
	"""
	# TODO: replace with your own rule, for example based on timestamp or geometry.
	return np.zeros(len(arr), dtype=bool)


def mark_filtered_points(arr):
	"""
	Return a boolean mask where True means "filtered out".

	Rows in arr are [timestamp, x, y, z, snr_db].
	Filters are applied sequentially and each filter only runs on still-unfiltered rows.
	"""
	filtered_mask = np.zeros(len(arr), dtype=bool)

	# Configure your filter chain here.
	filtered_mask = apply_filter_to_unfiltered(
		arr,
		filtered_mask,
		lambda sub_arr: filter_non_finite_values(sub_arr),
		filter_name="filter_non_finite_values",
	)
	filtered_mask = apply_filter_to_unfiltered(
		arr,
		filtered_mask,
		lambda sub_arr: filter_by_snr_threshold(sub_arr, snr_min=5.0),
		filter_name="filter_by_snr_threshold",
	)
	filtered_mask = apply_filter_to_unfiltered(
		arr,
		filtered_mask,
		lambda sub_arr: filter_by_knn_distance_sum(
			sub_arr,
			n_neighbors=100,
			threshold=2.5,
			mode="greater",
		),
		filter_name="filter_by_knn_distance_sum",
	)

	# Uncomment to use your own custom filter template.
	# filtered_mask = apply_filter_to_unfiltered(
	# 	arr,
	# 	filtered_mask,
	# 	lambda sub_arr: filter_template_custom(sub_arr),
	# 	filter_name="filter_template_custom",
	# )

	return filtered_mask


def sum_distances_to_k_nearest_neighbors(vectors, n_neighbors=100):
	"""
	Compute, for each vector, the sum of Euclidean distances to its k nearest neighbors.

	vectors: array-like of shape (N, D)
	returns: np.ndarray of shape (N,), where each entry is the k-NN distance sum.
	"""
	vectors = np.asarray(vectors, dtype=float)
	if vectors.ndim != 2:
		raise ValueError("vectors must be a 2D array with shape (N, D)")

	n_points = vectors.shape[0]
	if n_points == 0:
		return np.array([], dtype=float)
	if n_points == 1:
		return np.zeros(1, dtype=float)

	k = int(n_neighbors)
	if k <= 0:
		raise ValueError("n_neighbors must be >= 1")
	k = min(k, n_points - 1)

	scores = np.empty(n_points, dtype=float)
	for i in range(n_points):
		diff = vectors - vectors[i]
		dists = np.sqrt(np.einsum("ij,ij->i", diff, diff))
		dists[i] = np.inf  # Exclude the point itself.

		neighbor_dists = np.partition(dists, k)[:k]
		scores[i] = float(np.sum(neighbor_dists))

	return scores


def cluster_filter_by_neighbor_distance_sum(vectors, n_neighbors=100, threshold=1.0, mode="greater"):
	"""
	Threshold k-NN distance sums to produce a cluster/outlier mask.

	mode:
	- "greater": filtered when score > threshold (common for outlier removal)
	- "less": filtered when score < threshold

	Returns: (filtered_mask, scores)
	"""
	scores = sum_distances_to_k_nearest_neighbors(vectors, n_neighbors=n_neighbors)

	if mode == "greater":
		filtered_mask = scores > float(threshold)
	elif mode == "less":
		filtered_mask = scores < float(threshold)
	else:
		raise ValueError("mode must be 'greater' or 'less'")

	return filtered_mask, scores


def plot_data(arr, filtered_mask):
	x = arr[:, 1]
	y = arr[:, 2]
	z = arr[:, 3]
	snr = arr[:, 4]

	if filtered_mask.shape != (len(arr),):
		raise ValueError("filtered_mask must be a 1D boolean array with len(arr) elements")

	kept_mask = ~filtered_mask
	if not np.any(kept_mask):
		raise ValueError("All points are filtered. Keep at least one point for plotting.")

	fig = plt.figure(figsize=(15, 7))

	ax = fig.add_axes([0.05, 0.10, 0.42, 0.80], projection="3d")
	ax2 = fig.add_axes([0.55, 0.10, 0.32, 0.80])
	cax = fig.add_axes([0.90, 0.15, 0.02, 0.70])

	setup_3d_axes(ax)
	setup_az_el_axes(ax2)

	colors = CMAP(NORM(snr[kept_mask]))

	# Same artist types as live plot: 3D cloud, ground projection, and latest marker
	scatter3d = ax.scatter(x[kept_mask], y[kept_mask], z[kept_mask], s=30, depthshade=False)
	scatter3d.set_facecolors(colors)
	scatter3d.set_edgecolors(colors)

	scatter_ground = ax.scatter(
		x[kept_mask],
		y[kept_mask],
		np.full(np.count_nonzero(kept_mask), -1.5),
		s=10,
		depthshade=False,
	)
	scatter_ground.set_facecolors(colors)
	scatter_ground.set_edgecolors(colors)

	if np.any(filtered_mask):
		ax.scatter(
			x[filtered_mask],
			y[filtered_mask],
			z[filtered_mask],
			s=22,
			facecolors="none",
			edgecolors="gray",
			linewidths=1.0,
			alpha=0.8,
			depthshade=False,
		)
		ax.scatter(
			x[filtered_mask],
			y[filtered_mask],
			np.full(np.count_nonzero(filtered_mask), -1.5),
			s=8,
			facecolors="none",
			edgecolors="gray",
			linewidths=0.8,
			alpha=0.5,
			depthshade=False,
		)

	last_kept_index = np.where(kept_mask)[0][-1]
	x_last, y_last, z_last, snr_last = x[last_kept_index], y[last_kept_index], z[last_kept_index], snr[last_kept_index]
	latest3d = ax.scatter([x_last], [y_last], [z_last], s=100, marker="x", linewidths=2, depthshade=False)
	latest3d.set_facecolors([[1.0, 0.0, 0.0, 1.0]])
	latest3d.set_edgecolors([[1.0, 0.0, 0.0, 1.0]])

	az = -np.degrees(np.arctan2(y, x))
	el = np.degrees(np.arcsin(np.clip(z, -1.0, 1.0)))

	scatter2d = ax2.scatter(az[kept_mask], el[kept_mask], s=25)
	scatter2d.set_facecolors(colors)
	scatter2d.set_edgecolors(colors)

	if np.any(filtered_mask):
		ax2.scatter(
			az[filtered_mask],
			el[filtered_mask],
			s=20,
			facecolors="none",
			edgecolors="gray",
			linewidths=1.0,
			alpha=0.8,
			label="filtered",
		)

	az_last = az[last_kept_index]
	el_last = el[last_kept_index]
	latest2d = ax2.scatter([az_last], [el_last], s=100, marker="x", linewidths=2)
	latest2d.set_facecolors([[1.0, 0.0, 0.0, 1.0]])
	latest2d.set_edgecolors([[1.0, 0.0, 0.0, 1.0]])

	latest_text_3d = ax.text2D(0.02, 0.95, "", transform=ax.transAxes, fontsize=9)
	latest_text_2d = ax2.text(0.02, 0.98, "", transform=ax2.transAxes, va="top", fontsize=9)

	ax.set_title(
		f"Hydrophone Relative Position | "
		f"Visible samples: {np.count_nonzero(kept_mask)} | "
		f"Filtered: {np.count_nonzero(filtered_mask)} | "
		f"Latest SNR: {snr_last:.1f}"
	)
	latest_text_3d.set_text(f"latest: ({x_last:.2f}, {y_last:.2f}, {z_last:.2f})")
	latest_text_2d.set_text(f"latest: ({az_last:.1f}°, {el_last:.1f}°)")

	mappable = plt.cm.ScalarMappable(norm=NORM, cmap=CMAP)
	mappable.set_array([])
	cbar = fig.colorbar(mappable, cax=cax)
	cbar.set_label("SNR")

	plt.show()


def parse_args():
	parser = argparse.ArgumentParser(
		description="Plot all logged CAN CSV samples offline (no realtime bus needed)."
	)
	parser.add_argument(
		"--file",
		type=str,
		default="",
		help="Single CSV file to plot (overrides --pattern).",
	)
	parser.add_argument(
		"--pattern",
		type=str,
		default="small_tools/can_log_samples/can_samples_log_*.csv",
		help="Glob pattern used when --file is not provided.",
	)
	return parser.parse_args()


def main():
	args = parse_args()
	files = collect_files(args.pattern, args.file)
	arr = load_data(files, max_loaded_samples=MAX_LOADED_SAMPLES)
	filtered_mask = mark_filtered_points(arr)

	print(f"Loaded {len(arr):,} samples from {len(files)} file(s).")
	if MAX_LOADED_SAMPLES is not None:
		print(f"Load cap enabled: latest {MAX_LOADED_SAMPLES:,} samples max.")
	print(f"Filtered {np.count_nonzero(filtered_mask):,} / {len(arr):,} samples.")
	if len(files) <= 5:
		for f in files:
			print(f" - {f}")
	else:
		print(f" - First: {files[0]}")
		print(f" - Last : {files[-1]}")

	plot_data(arr, filtered_mask)


if __name__ == "__main__":
	main()
