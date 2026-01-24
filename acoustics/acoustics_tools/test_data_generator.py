import subprocess

n = 100

for i in range(n):
    print("Starting acoustic_data_simulator.jl...\n")
    subprocess.run(
        ["julia", "acoustic_data_simulator.jl", "simulation_config_for_testing.json", f"tests/test_data/data_{i}.csv", "quiet"],  # add script arguments if needed
        capture_output=True,
        text=True
    )
    print("Done running acoustic_data_simulator.jl\n")
    
print("All simulations completed.\n")