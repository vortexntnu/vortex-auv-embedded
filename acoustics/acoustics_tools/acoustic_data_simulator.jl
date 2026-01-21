using UnderwaterAcoustics
using Plots
using SignalAnalysis
using JSON3


struct simulation_config
    hydrophones_pos::Vector{Tuple{Float64,Float64,Float64}}
    drone_pos::Tuple{Float64,Float64,Float64}
    pinger_pos::Tuple{Float64,Float64,Float64}
    sea_depth::Float64
    noise_level::Float64
    noise_type::String
    function simulation_config(; hydrophones_pos, drone_pos, pinger_pos, sea_depth=6.0, noise_level=0.0, noise_type="white")
        return new(hydrophones_pos, drone_pos, pinger_pos, sea_depth, noise_level, noise_type)
    end
end

function config_from_json(path::AbstractString)
    cfg = JSON3.read(read(path, String))

    hydrophones_pos = [Tuple(Float64.(pos)) for pos in cfg["hydrophones_pos"]]
    drone_pos = Tuple(Float64.(cfg["drone_pos"]))
    pinger_pos = Tuple(Float64.(cfg["pinger_pos"]))

    sea_depth = haskey(cfg, "sea_depth") ? Float64(cfg["sea_depth"]) : 6.0
    noise_level = haskey(cfg, "noise_level") ? Float64(cfg["noise_level"]) : 0.0
    noise_type = haskey(cfg, "noise_type") ? String(cfg["noise_type"]) : "white"

    return simulation_config(
        hydrophones_pos = hydrophones_pos,
        drone_pos = drone_pos,
        pinger_pos = pinger_pos,
        sea_depth = sea_depth,
        noise_level = noise_level,
        noise_type = noise_type,
    )
end

function simulate_hydrophone_data(config::simulation_config)

# Unpack configuration
hydrophones_pos = config.hydrophones_pos
drone_pos = config.drone_pos
pinger_pos = config.pinger_pos
sea_depth = config.sea_depth
noise_level = config.noise_level
noise_type = config.noise_type
print("Simulation configuration loaded.\n")

# ==============================
# Simulation parameters
# ==============================

env = UnderwaterEnvironment(
  bathymetry = sea_depth, 
  temperature = 15.0, 
  salinity = 35.0, 
  pH = 8.1, 
  soundspeed = 1538.9235842, 
  density = 1022.7198310217424, 
  seabed = VeryCoarseSand,
  surface = PressureReleaseBoundary, 
)
pm = PekerisRayTracer(env) # Pekeris based propagation model

print("Simulatioon Environment configured.\n")
# ==============================
# Hardware configuration
# ==============================

# Pinger configuration
pinger_frequency = 30_000.0  # Pinger frequency [Hz]
pinger_duration = 4e-3      # Pinger duration [ms]
pinger_power = 150.0         # Pinger source level [dB re 1μPa at 1m]

pinger = AcousticSource(pinger_pos, pinger_frequency,spl=pinger_power)

print("Pinger configured at position:\n")
print("  ", pinger_pos, "\n")

# Hydrophone configuration
hydrophone_sample_frequency = 1_000_000  # Sampling frequency of hydrophones [Hz]

hydrophones_pos = map(pos -> pos .+ drone_pos, hydrophones_pos)  # Adjust hydrophone positions relative to drone position

hydrophones = [AcousticReceiver(pos) for pos ∈ hydrophones_pos]

print("Hydrphones configured at positions: \n")
for i ∈ eachindex(hydrophones_pos)
    print("  ", hydrophones_pos[i], "\n")
end

print("Pinger and Hydrophones configured.\n")

# ==============================
# Simulating model
# ==============================

reference_hydrophone_rays = arrivals(pm, pinger, hydrophones[1])

print("Ray tracing simulation completed.\n")
print("Rays to reference hydrophone: \n")
for ray ∈ reference_hydrophone_rays
    print("  ",ray, "\n")
end

print("Hydropone ping rays: \n")
for i ∈ eachindex(hydrophones)
    rays = arrivals(pm, pinger, hydrophones[i])
    print("Hydrophone ", i, " first ray: ", rays[1], "\n")
end

if noise_type == "white"
    noise = WhiteGaussianNoise(noise_level)
elseif noise_type == "red"
    noise = RedGaussianNoise(noise_level)
else
    error("Unsupported noise type: $noise_type")
end

channels = channel(pm, pinger, hydrophones, hydrophone_sample_frequency; noise=noise)

x = cw(pinger_frequency, pinger_duration, hydrophone_sample_frequency; window=(tukey, 0.0000005)) |> real

hydrophones_data = transmit(channels, x; abstime = true)
hydrophones_data = [collect(row) for row in eachcol(hydrophones_data)]

print("Signal transmission through channels completed.\n")

# ===============================
# Electrical Hardware Simulation
# ===============================
hydrophone_sensitivity = -180.0  # dB re 1V/μPa
amplifier_gain = 8.57            # dB at 31kHz
analog_filter_lower_cutoff = 18_870  # Hz
analog_filter_upper_cutoff = 52_870  # Hz

Michael_filter = analogfilter(Bandpass(2*π*analog_filter_lower_cutoff, 2*π*analog_filter_upper_cutoff),Butterworth(4))

f_0 = sqrt(analog_filter_lower_cutoff * analog_filter_upper_cutoff)
BW = analog_filter_upper_cutoff - analog_filter_lower_cutoff
Q = f_0 / BW
K = 10.0^(amplifier_gain / 20.0)
ω_0 = 2 * π * f_0
ω_0_squared = ω_0^2

Michael_filter = bilinear(Michael_filter, hydrophone_sample_frequency)

for i ∈ eachindex(hydrophones_data)
    # Apply hydrophone sensitivity
    hydrophones_data[i] .*= 10.0^( hydrophone_sensitivity / 20.0 )

    # Apply amplifier gain
    hydrophones_data[i] .*= 10.0^( amplifier_gain / 20.0 )

    # Apply analog filter
    hydrophones_data[i] = filt(Michael_filter, hydrophones_data[i])
end

print("Electrical hardware simulation completed.\n")

# ==============================
# Visualization
# ==============================

function plot_reference_hydrophone_rays(rays)
    p = plot(env; xlims=(-5,15))
    plot!(pinger)
    for hydrophone ∈ hydrophones
        plot!(hydrophone)
    end
    plot!(rays)
    display(p)
    gui()
end

#plot_reference_hydrophone_rays(reference_hydrophone_rays)

function plot_impulse_response()
    p = plot(impulse_response(pm, pinger, hydrophones[1], 62500.0); title="Impulse Response at Reference Hydrophone")
    display(p)
    gui()
end

#plot_impulse_response()

function plot_signals()
    n = length(hydrophones_data)
    data_length = 20 # milliseconds

    # Create hydrophone plots
    hydro_plots = [plot(hydrophones_data[i]; xlims=(0,data_length), title="Hydrophone $i") for i in 1:n]

    # Combine transmitted + hydrophone plots
    p = plot(
        plot(x; xlims=(0,data_length), title="Transmitted Signal"),
        hydro_plots...;
        layout = (n+1, 1),
        heights = [0.3; fill(0.7/n, n)],  # first plot 30%, rest split evenly
        size = (800, 200 + 150*n)
    )
    display(p)
    gui()
end

# ==============================
# CSV Output Setup
# ==============================

function save_to_csv(data,timeseries,name)
    open("$(name)_data.csv", "w") do f
        println(f, "time,signal")
        for j ∈ eachindex(data)
            println(f, "$(timeseries[j]),$(data[j])")
        end
    end
end

# Save to files (optional)
function save_all_to_csv(hydrophones_data)
    for i ∈ eachindex(hydrophones_data)
        t = range(0, step=1/hydrophone_sample_frequency, length=length(hydrophones_data[i]))
        sig = hydrophones_data[i]
        save_to_csv(sig,t,"hydrophone_$(i)")
    end
end

save_all_to_csv(hydrophones_data)
print("Simulation data saved to CSV files.\n")
end

function default_simulation_config()
    hydro_pos = [
        (0.5, 0.5, 0.5),
        (1.0, 0.0, 0.0),
        (0.0, 1.0, 0.0),
        (1.0, 1.0, 0.0),
        (0.0, 0.0, 0.0)
    ]

    return simulation_config(
        hydrophones_pos = hydro_pos,
        drone_pos = (0.0, 0.0, -1.0),
        pinger_pos = (15.0, 10.0, -5.5),
        sea_depth = 6.0,
        noise_level = 5e5,
        noise_type = "white",
    )
end

function main()
    print("Starting Acoustic Data Simulator...\n")
    cfg = config_from_json("simulation_config.json")
    simulate_hydrophone_data(cfg)
end

main()