import Pkg
Pkg.activate(".")          # uses ./Project.toml (creates if missing)
Pkg.add(["UnderwaterAcoustics", "Plots", "SignalAnalysis", "TOML"])
Pkg.instantiate()