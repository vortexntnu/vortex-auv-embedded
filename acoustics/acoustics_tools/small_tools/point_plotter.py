dump_raw = """[
dump = [
	{[0.544883,0.340551,0.766242],22.106126},
	{[0.463603,0.760143,-0.455250],33.606098},
	{[0.682957,0.548105,-0.482855],37.210407},
	{[1.000000,0.000000,0.000000],54.648853},
	{[-0.948946,0.253943,0.187116],52.957401},
	{[0.003936,-0.999992,0.000000],91.273597},
	{[-0.935674,0.249513,-0.249513],84.750244},
	{[0.358150,-0.153493,-0.920960],89.586685},
	{[-0.605877,-0.795503,0.009250],20.354919},
	{[0.780867,0.373458,-0.500773],64.164604},
	{[0.845036,0.501740,0.184851],18.000301},
	{[0.885258,0.431833,-0.172733],49.725246},
	{[-0.584806,-0.803370,-0.112235],50.989021},
	{[0.508899,0.174934,-0.842864],48.001514},
	{[0.575837,0.578105,-0.578105],76.984588},
	{[0.721893,0.630946,-0.284210],56.120716},
	{[0.845036,0.501740,0.184851],12.122183},
	{[0.885258,0.431833,-0.172733],57.941040},
	{[0.396770,0.036069,0.917209],87.309722},
	{[0.341972,-0.146559,-0.928210],48.420082},
	{[-0.019407,-0.706973,0.706973],79.116455},
	{[0.837478,0.523423,0.157027],12.117522},
	{[0.826551,0.525987,-0.200376],76.100555},
	{[0.620174,-0.248068,0.744208],62.490177},
	{[0.458172,-0.844273,-0.277992],77.185737},
	{[0.833774,0.521108,0.182388],10.950998},
	{[0.876917,0.449152,-0.171105],89.458564},
	{[0.306896,-0.131526,-0.942610],77.644859},
	{[0.664545,-0.572430,-0.480315],72.425201},
	{[0.837478,0.523424,0.157027],16.149099},
	{[0.883667,0.441833,-0.154641],62.973369},
	{[0.686963,0.661983,-0.299765],52.878082},
	{[0.837478,0.523424,0.157027],17.480882},
	{[0.885258,0.431833,-0.172733],57.689666},
	{[0.649065,0.635107,-0.418751],46.020523},
	{[-0.411408,-0.698148,-0.585945],82.631347},
	{[0.845036,0.501740,0.184851],15.855894},
	{[0.885258,0.431833,-0.172733],78.365043},
	{[0.655948,0.041869,-0.753643],50.702030},
	{[0.327088,-0.140180,-0.934538],46.865375},
	{[0.000000,-0.707106,0.707106],87.967269},
	{[0.833774,0.521108,0.182388],11.173570},
	{[0.885258,0.431833,-0.172733],66.260871},
	{[0.546855,0.555675,-0.626237],57.838798},
	{[0.292006,-0.157233,-0.943403],74.014305},
	{[-0.447213,0.000000,0.894427],59.441246},
	{[0.609697,-0.670163,-0.423261],43.076652},
	{[-0.836452,0.235620,0.494802],53.663936},
	{[0.553371,0.622543,-0.553371],110.940238},
	{[-0.504801,-0.826829,0.248048],58.145027},
	{[-0.920415,0.262975,0.289273],26.212589},
	{[0.374705,0.783167,-0.496230],19.614301},
	{[0.408248,-0.163298,-0.898146],63.368473},
	{[0.772008,0.466118,-0.432130],56.678241},
	{[0.815521,0.442316,-0.373204],68.015968},
	{[0.740193,0.439489,0.508882],37.852767},
	{[0.724286,0.633750,-0.271607],113.191909},
	{[-0.945658,0.266382,0.186467],46.565498},
	{[0.258079,0.683152,-0.683152],36.202342},
	{[0.601445,-0.240578,0.761830],39.409633},

"""

import matplotlib.pyplot as plt
import numpy as np
import re

hydrophone_positions_temp= [
			[0.0,0.0,0.0],
			[0.0,0.0,0.0],
			[0.45,0.0,0.0],
			[0.0,0.45,0.0],
			[0.0,0.0,-0.45]
]

# Create 3D plot
fig = plt.figure(figsize=(12, 9))
ax = fig.add_subplot(111, projection='3d')

# Parse weighted dump points: each entry is {[x,y,z],weight}
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

# Plot hydrophone positions
ax.scatter(hydrophone_array[:, 0], hydrophone_array[:, 1], hydrophone_array[:, 2],
           c='red', marker='o', s=100, label='Hydrophone Positions', alpha=0.8, edgecolors='darkred', linewidth=2)

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

