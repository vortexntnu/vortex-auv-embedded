using UnderwaterAcoustics
using Plots
using SignalAnalysis


# ==============================
# Simulation parameters
# ==============================

env = UnderwaterEnvironment(
  bathymetry = 6, 
  temperature = 27.0, 
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
pinger_pos = (15.0, 10.0, -5.5)

pinger = AcousticSource(pinger_pos, pinger_frequency,spl=pinger_power)

print("Pinger configured at position:\n")
print("  ", pinger_pos, "\n")

# Hydrophone configuration
hydrophone_sample_frequency = 1_000_000  # Sampling frequency of hydrophones [Hz]

hydrophones_pos = [
    (0.5, 0.5, 0.5),
    (1.0, 0.0, 0.0),
    (0.0, 1.0, 0.0),
    (1.0, 1.0, 0.0),
    (0.0, 0.0, 0.0)
]

hydrophone_offset = (0.0, 0.0, -3.0)  # Offset to be applied to all hydrophones

hydrophones = Array{AcousticReceiver}(undef, length(hydrophones_pos))

for i ∈ eachindex(hydrophones_pos)
    hydrophones_pos[i] = (
        hydrophones_pos[i][1] + hydrophone_offset[1],
        hydrophones_pos[i][2] + hydrophone_offset[2],
        hydrophones_pos[i][3] + hydrophone_offset[3]
    )
    hydrophones[i] = AcousticReceiver(hydrophones_pos[i])
end

print("Hydrphones configured at positions: \n")
for i ∈ eachindex(hydrophones_pos)
    print("  ", hydrophones_pos[i], "\n")
end

print("Pinger and Hydrophones configured.\n")

# ==============================
# Simulating model
# ==============================

reference_hydrophone_rays = arrivals(pm, pinger, hydrophones[1])

#= p = plot(env; xlims=(-5,15))
plot!(pinger)
plot!(hydrophones[1])
plot!(reference_hydrophone_rays)

display(p)
gui() =#

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



#plot_reference_hydrophone_rays(reference_hydrophone_rays)

transmission_losses = transmission_loss(pm, pinger, hydrophones)

channels = Array{UnderwaterAcoustics.SampledPassbandChannel}(undef, length(hydrophones))

for i ∈ eachindex(hydrophones)
    channels[i] = channel(pm, pinger, hydrophones[i], hydrophone_sample_frequency; noise=RedGaussianNoise(1e6))
end

x = cw(pinger_frequency, pinger_duration, hydrophone_sample_frequency; window=(tukey, 0.05)) |> real

hydrophones_data = Array{SignalAnalysis.SampledSignal}(undef, length(hydrophones))
for i ∈ eachindex(channels)
    hydrophones_data[i] = transmit(channels[i], x; abstime = true)
end

print("Signal transmission through channels completed.\n")

# ==============================
# Visualization
# ==============================

function plot_reference_hydrophone_rays(rays)
    plot(env; xlims=(-5,15))
    plot!(pinger)
    for hydrophone ∈ hydrophones
        plot!(hydrophone)
    end
    plot!(rays)
    gui()
end

function plot_impulse_response()
    plot(impulse_response(pm, pinger, hydrophones[1], 62500.0); title="Impulse Response at Reference Hydrophone")
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

plot_signals()
#psd(hydrophones_data[1]; title="PSD at Reference Hydrophone")
#print(hydrophones_data[1])

# ==============================
# CSV Output Setup
# ==============================

# Save to files (optional)
function save_to_csv(hydrophones_data)
    for i ∈ eachindex(hydrophones_data)
        t = range(0, step=1/hydrophone_sample_frequency, length=length(hydrophones_data[i]))
        sig = hydrophones_data[i]
        open("hydrophone_$(i)_data.csv", "w") do f
            println(f, "time,signal")
            for j ∈ eachindex(sig)
                println(f, "$(t[j]),$(sig[j])")
            end
        end
    end
end

save_to_csv(hydrophones_data)