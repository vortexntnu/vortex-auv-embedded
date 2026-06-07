import matplotlib.pyplot as plt
import numpy as np
import re

dump_file = "small_tools/direction_dump"

hydrophone_positions_temp= [
			[0.0,0.0,0.0],
			[0.0,0.0,0.0],
			[0.45,0.0,0.0],
			[0.0,0.45,0.0],
			[0.0,0.0,-0.45]
]

# Edit these colors to choose a unique color for each hydrophone.
hydrophone_colors = ["orange", "red", "green", "brown", "blue"]

# Create 3D plot
fig = plt.figure(figsize=(12, 9))
ax = fig.add_subplot(111, projection='3d')

# Read weighted dump points from file: each entry is {[x,y,z],weight}
with open(dump_file, "r", encoding="utf-8") as file_handle:
	dump_raw = file_handle.read()

matches = re.findall(r"\{\[([^\]]+)\],([-+0-9.eE]+)\}", dump_raw)
dump = [([float(v) for v in vec.split(',')], float(weight)) for vec, weight in matches]

# Convert to numpy arrays
hydrophone_array = np.array(hydrophone_positions_temp)
dump_vectors = np.array([point[0] for point in dump], dtype=float)
dump_weights = np.array([point[1] for point in dump], dtype=float)

# Map weights to marker sizes for visibility
weight_min = dump_weights.min()
weight_max = dump_weights.max()
if weight_max > weight_min:
	weight_norm = (dump_weights - weight_min) / (weight_max - weight_min)
else:
	weight_norm = np.ones_like(dump_weights)
marker_sizes = 20 + 220 * weight_norm

# Plot hydrophone positions with unique colors
for index, (position, color) in enumerate(zip(hydrophone_array, hydrophone_colors), start=1):
	ax.scatter(
		position[0],
		position[1],
		position[2],
		c=color,
		marker='o',
		s=100,
		label=f'Hydrophone {index}',
		alpha=0.85,
		edgecolors='black',
		linewidth=1.5
	)

# Plot weighted direction vectors
weighted_scatter = ax.scatter(
	dump_vectors[:, 0],
	dump_vectors[:, 1],
	dump_vectors[:, 2],
	c=dump_weights,
	cmap='viridis',
	marker='^',
	s=marker_sizes,
	alpha=0.75,
	label='Direction Vectors (weighted)'
)

# Show weight scale
colorbar = fig.colorbar(weighted_scatter, ax=ax, shrink=0.7, pad=0.08)
colorbar.set_label('Weight')

# Force equal data scale on all 3 axes so geometry is not visually distorted.
all_points = np.vstack((dump_vectors, hydrophone_array))
mins = all_points.min(axis=0)
maxs = all_points.max(axis=0)
center = (mins + maxs) / 2.0
radius = np.max(maxs - mins) / 2.0

ax.set_xlim(center[0] - radius, center[0] + radius)
ax.set_ylim(center[1] - radius, center[1] + radius)
ax.set_zlim(center[2] - radius, center[2] + radius)
ax.set_box_aspect((1, 1, 1))

# Set labels
ax.set_xlabel('X')
ax.set_ylabel('Y')
ax.set_zlabel('Z')
ax.set_title('3D Point Visualization')

# Add legend
ax.legend()

# Add grid
ax.grid(True, alpha=0.3)

plt.tight_layout()
plt.show()

