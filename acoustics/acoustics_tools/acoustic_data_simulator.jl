using UnderwaterAcoustics
using Plots
using SignalAnalysis
using JSON3
using FFTW


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

# ===============================
# Electrical model configuration
# ===============================
# For best fidelity: export the LTspice AC analysis of V(out) as a text/CSV file with columns:
#   freq_hz, mag_db, phase_deg
# Then set `use_measured_transfer = true` and point to that file.
use_measured_transfer = true
measured_transfer_path = "LTSpice_analog_filter_sim/analog_filter_sim_results.txt"  # user-provided export

# Hydrophone sensitivity: -180 dB re 1 V/µPa.
# IMPORTANT: ensure the acoustic simulator output is in µPa. If it is in Pa, add +120 dB.
hydrophone_sensitivity_db_v_per_uPa = -180.0

# Simple parametric fallback model (used when no measured transfer is supplied)
fallback_amp_gain_db_at_31k = 8.57
fallback_bp_low_hz = 18_870.0
fallback_bp_high_hz = 52_870.0
fallback_bp_order = 2

function load_measured_transfer(path::AbstractString)
    # Accept LTspice exports in either:
    #  - polar:   freq <tab> (-148dB,-91°)
    #  - cart:    freq <tab> (re,im)
    #  - numeric: freq, mag_db, phase_deg
    # Lines starting with '#' or ';' are ignored. Header lines are skipped.
    rows = Tuple{Float64,Float64,Float64}[]  # (freq_hz, mag_db, phase_deg)

    function _parse_first_float(s::AbstractString)
        m = match(r"[-+]?((\d+(\.\d*)?)|(\.\d+))([eE][-+]?\d+)?", s)
        m === nothing && error("No float in: $s")
        return parse(Float64, m.match)
    end

    function _strip_wrappers(s::AbstractString)
        t = strip(s)
        if startswith(t, "(") && endswith(t, ")")
            t = t[2:end-1]
        end
        return strip(t)
    end

    function _parse_ltspice_pair(field::AbstractString)
        # Returns either (mag_db, phase_deg, :polar) or (mag_db, phase_deg, :cart)
        t = _strip_wrappers(field)
        parts = split(t, ",")
        length(parts) < 2 && error("Not a complex pair: $field")
        a = strip(parts[1])
        b = strip(parts[2])

        # Detect polar by 'dB' or degree symbol
        if occursin("dB", a) || occursin("°", b) || occursin("deg", lowercase(b)) || occursin("dB", b)
            mdb = _parse_first_float(a)
            ph = _parse_first_float(b)
            return mdb, ph, :polar
        else
            re = _parse_first_float(a)
            im = _parse_first_float(b)
            mag = sqrt(re^2 + im^2)
            mdb = 20.0 * log10(mag + 1e-300)
            ph = atan(im, re) * 180.0 / π
            return mdb, ph, :cart
        end
    end

    for line in eachline(path)
        s = strip(line)
        isempty(s) && continue
        startswith(s, "#") && continue
        startswith(s, ";") && continue

        # Normalize separators; keep parentheses payload intact by not splitting on commas first.
        parts = split(s)  # whitespace split (tabs/spaces)
        if length(parts) >= 2
            # LTspice 2-column export: freq + complex field
            try
                f = _parse_first_float(parts[1])
                field = join(parts[2:end], "")
                mdb, ph, _ = _parse_ltspice_pair(field)
                push!(rows, (f, mdb, ph))
                continue
            catch
                # fall through to numeric parsing
            end
        end

        # Numeric parsing: accept CSV or whitespace separated freq, mag_db, phase_deg
        parts_num = split(replace(s, "," => " "))
        if length(parts_num) >= 3
            try
                f = _parse_first_float(parts_num[1])
                mdb = _parse_first_float(parts_num[2])
                ph = _parse_first_float(parts_num[3])
                push!(rows, (f, mdb, ph))
            catch
                # ignore unparsable lines (e.g., headers)
            end
        end
    end
    if isempty(rows)
        error("No usable rows found in measured transfer file: $path")
    end
    sort!(rows, by = r -> r[1])
    freqs = [r[1] for r in rows]
    mag_db = [r[2] for r in rows]
    phase_deg = [r[3] for r in rows]
    return freqs, mag_db, phase_deg
end

function interp1_linear(x::Vector{Float64}, y::Vector{Float64}, xq::Float64)
    # Linear interpolation with end clamping
    xq <= x[1] && return y[1]
    xq >= x[end] && return y[end]
    i = searchsortedlast(x, xq)
    i = clamp(i, 1, length(x) - 1)
    x1, x2 = x[i], x[i+1]
    y1, y2 = y[i], y[i+1]
    t = (xq - x1) / (x2 - x1)
    return y1 + t * (y2 - y1)
end

function apply_measured_transfer_fft(x::Vector{Float64}, fs::Int64, freqs::Vector{Float64}, mag_db::Vector{Float64}, phase_deg::Vector{Float64})
    # Applies a one-sided transfer function to a real signal using rFFT.
    # Frequency response is interpolated linearly in frequency.
    n = length(x)
    X = rfft(x)
    # rfft bins: k=0..n/2
    for k in eachindex(X)
        f = (k - 1) * fs / n
        mdb = interp1_linear(freqs, mag_db, f)
        ph = interp1_linear(freqs, phase_deg, f)
        H = 10.0^(mdb / 20.0) * cis(ph * π / 180.0)
        X[k] *= H
    end
    return irfft(X, n)
end

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
sens_v_per_uPa = 10.0^(hydrophone_sensitivity_db_v_per_uPa / 20.0)

measured_freqs = Float64[]
measured_mag_db = Float64[]
measured_phase_deg = Float64[]
if use_measured_transfer
    if isfile(measured_transfer_path)
        measured_freqs, measured_mag_db, measured_phase_deg = load_measured_transfer(measured_transfer_path)
        print("Loaded measured transfer response from $(measured_transfer_path).\n")
    else
        error("use_measured_transfer=true but file not found: $(measured_transfer_path)")
    end
end

fallback_filter = analogfilter(
    Bandpass(2*π*fallback_bp_low_hz, 2*π*fallback_bp_high_hz),
    Butterworth(fallback_bp_order),
)
fallback_filter = bilinear(fallback_filter, hydrophone_sample_frequency)
fallback_gain = 10.0^(fallback_amp_gain_db_at_31k / 20.0)

for i ∈ eachindex(hydrophones_data)
    # Apply hydrophone sensitivity
    hydrophones_data[i] .*= sens_v_per_uPa

    if use_measured_transfer
        hydrophones_data[i] = apply_measured_transfer_fft(
            hydrophones_data[i],
            hydrophone_sample_frequency,
            measured_freqs,
            measured_mag_db,
            measured_phase_deg,
        )
    else
        hydrophones_data[i] .*= fallback_gain
        hydrophones_data[i] = filt(fallback_filter, hydrophones_data[i])
    end
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