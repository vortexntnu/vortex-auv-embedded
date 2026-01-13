# Signal Test Data Generator for Hydrophones and Pinger
# This script generates simulated acoustic data from 5 hydrophones detecting a pinger signal.

using LinearAlgebra  # For vector operations
using Random  # For noise generation

# Configuration structure
struct HydrophoneConfig
    pinger_position::Vector{Float64}  # [x, y, z] in meters
    hydrophone_positions::Vector{Vector{Float64}}  # Array of [x, y, z] for 5 hydrophones
    pinger_frequency::Float64  # Hz
    pinger_duration::Float64  # seconds
    sample_frequencies::Vector{Float64}  # Hz for each hydrophone
    output_sizes::Vector{Int}  # Number of samples for each hydrophone
    speed_of_sound::Float64  # m/s
    absorption_coefficient::Float64  # dB/m (attenuation per meter)
    multipath_enabled::Bool
    multipath_delays::Vector{Float64}  # Additional delays in seconds for multipath
    multipath_attenuations::Vector{Float64}  # Additional attenuations in dB for multipath
    noise_level::Float64  # Standard deviation of additive noise
end

# Default configuration
function default_config()
    pinger_pos = [20.0, 15.0, 30.0]
    hydro_pos = [
        [0.5, 0.5, 0.5],
        [1.0, 0.0, 0.0],
        [0.0, 1.0, 0.0],
        [0.0, 0.0, 0.0],
        [1.0, 1.0, 0.0]
    ]
    return HydrophoneConfig(
        pinger_pos,
        hydro_pos,
        30000.0,  # 30 kHz
        0.004,  # 4 ms
        fill(1000000.0, 5),  # 1 MHz for all
        fill(100000, 5),  # 100000 samples each
        1500.0,  # Speed of sound in water
        0.1,  # Absorption
        false,  # No multipath
        [0.01, 0.02],  # Example delays
        [10.0, 20.0],  # Example attenuations
        0.01  # Noise level
    )
end

# Function to generate pinger signal
function generate_pinger_signal(config::HydrophoneConfig)
    fs_max = maximum(config.sample_frequencies)
    t_total = config.pinger_duration
    t = collect(0:1/fs_max:t_total)
    signal = sin.(2π * config.pinger_frequency * t)
    return t, signal
end

# Function to simulate hydrophone recording
function simulate_hydrophone(config::HydrophoneConfig, hydro_idx::Int, t_pinger::Vector{Float64}, signal_pinger::Vector{Float64})
    pos_pinger = config.pinger_position
    pos_hydro = config.hydrophone_positions[hydro_idx]
    distance = norm(pos_hydro - pos_pinger)
    delay = distance / config.speed_of_sound
    attenuation_db = config.absorption_coefficient * distance
    attenuation = 10^(-attenuation_db / 20)

    fs_hydro = config.sample_frequencies[hydro_idx]
    n_samples = config.output_sizes[hydro_idx]
    t_hydro = (0:n_samples-1) / fs_hydro

    # Interpolate pinger signal at hydrophone times, shifted by delay
    signal_hydro = zeros(Float64, n_samples)
    for i in 1:n_samples
        t_sample = t_hydro[i] - delay
        if t_sample >= 0 && t_sample <= config.pinger_duration
            # Simple interpolation (could use better method)
            idx = searchsortedfirst(t_pinger, t_sample)
            if idx > 1 && idx <= length(t_pinger)
                frac = (t_sample - t_pinger[idx-1]) / (t_pinger[idx] - t_pinger[idx-1])
                signal_hydro[i] = attenuation * (signal_pinger[idx-1] * (1 - frac) + signal_pinger[idx] * frac)
            end
        end
    end

    # Add multipath if enabled
    if config.multipath_enabled
        for (delay_mp, att_db_mp) in zip(config.multipath_delays, config.multipath_attenuations)
            att_mp = 10^(-att_db_mp / 20)
            for i in 1:n_samples
                t_sample = t_hydro[i] - delay - delay_mp
                if t_sample >= 0 && t_sample <= config.pinger_duration
                    idx = searchsortedfirst(t_pinger, t_sample)
                    if idx > 1 && idx <= length(t_pinger)
                        frac = (t_sample - t_pinger[idx-1]) / (t_pinger[idx] - t_pinger[idx-1])
                        signal_hydro[i] += attenuation * att_mp * (signal_pinger[idx-1] * (1 - frac) + signal_pinger[idx] * frac)
                    end
                end
            end
        end
    end

    # Add noise
    signal_hydro .+= config.noise_level * randn(n_samples)

    return t_hydro, signal_hydro
end

# Main generation function
function generate_hydrophone_data(config::HydrophoneConfig)
    t_pinger, signal_pinger = generate_pinger_signal(config)
    hydrophone_data = Vector{Tuple{Vector{Float64}, Vector{Float64}}}(undef, 5)
    for i in 1:5
        hydrophone_data[i] = simulate_hydrophone(config, i, t_pinger, signal_pinger)
    end
    return hydrophone_data
end

# Example usage
config = default_config()
data = generate_hydrophone_data(config)

# Save to files (optional)
for i in 1:5
    t, sig = data[i]
    open("hydrophone_$(i)_data.csv", "w") do f
        println(f, "time,signal")
        for j in 1:length(t)
            println(f, "$(t[j]),$(sig[j])")
        end
    end
end

println("Data generation complete. Files saved as hydrophone_1_data.csv to hydrophone_5_data.csv")