using UnderwaterAcoustics
using DSP
using LinearAlgebra
using Random
using CSV
using DataFrames

# ==============================
# Simulation parameters
# ==============================

fs = 100_000.0           # Sampling frequency [Hz]
T  = 0.02                # Signal duration [s]
c  = 1500.0              # Speed of sound [m/s]
t  = collect(0:1/fs:T)   # Explicit vector for CSV writing

# ==============================
# Pinger signal
# ==============================

f0 = 30_000.0            # Pinger frequency [Hz]
pinger_signal = sin.(2π * f0 .* t)

# ==============================
# Geometry (meters)
# ==============================

pinger_pos = [10.0, 5.0, -2.0]

hydrophones = [
    [0.0, 0.0, 0.0],
    [1.0, 0.0, 0.0],
    [0.0, 1.0, 0.0],
    [1.0, 1.0, 0.0],
    [0.5, 0.5, -0.5]
]

N_h = length(hydrophones)

# ==============================
# Simulate received signals
# ==============================

received = zeros(N_h, length(t))
true_delays = zeros(N_h)

for i ∈ 1:N_h
    r = norm(hydrophones[i] .- pinger_pos)   # Distance
    τ = r / c                                # Time delay
    A = 1 / r                                # Spreading loss

    true_delays[i] = τ

    delay_samples = τ * fs
    delayed_signal = DSP.delay(pinger_signal, delay_samples)

    received[i, :] .= A .* delayed_signal
end

# ==============================
# Add noise
# ==============================

SNR_dB = 10.0
signal_power = mean(pinger_signal.^2)
noise_power = signal_power / 10^(SNR_dB / 10)

Random.seed!(0)
received .+= sqrt(noise_power) .* randn(size(received))

# ==============================
# Write CSV files
# ==============================

# --- Per-hydrophone CSV files ---
for i ∈ 1:N_h
    df = DataFrame(
        time_s = t,
        amplitude = received[i, :]
    )

    filename = "hydrophone_$(i)_data.csv"
    CSV.write(filename, df)
end

# --- Combined CSV (all hydrophones) ---
df_all = DataFrame(time_s = t)

for i ∈ 1:N_h
    df_all[!, "h$(i)"] = received[i, :]
end

CSV.write("hydrophones_all.csv", df_all)

# --- Optional: write metadata (geometry + delays) ---
meta = DataFrame(
    hydrophone = 1:N_h,
    x = [h[1] for h in hydrophones],
    y = [h[2] for h in hydrophones],
    z = [h[3] for h in hydrophones],
    true_delay_s = true_delays
)

CSV.write("hydrophone_metadata.csv", meta)