INPUT_FILE = "Default_Register_Data.txt"
OUTPUT_FILE = "default_regs_output.c"
ARRAY_NAME = "ad7606_reg_table"

pairs = []

with open(INPUT_FILE, "r") as f:
    for line in f:
        line = line.strip()

        # Skip empty lines
        if not line:
            continue

        parts = line.split()

        # We only care about lines with two items like: 0x03 0x44
        if len(parts) == 2 and parts[0].startswith("0x") and parts[1].startswith("0x"):
            addr = parts[0]
            value = parts[1]
            pairs.append((addr, value))

with open(OUTPUT_FILE, "w") as f:
    f.write(f"struct ad7606_registers {ARRAY_NAME} =\n{{\n")

    pairs.sort(key=lambda x: int(x[0], 16))  # Sort by address

    f.write(f"    {{ 0x01, 0x00, true }},\n")
    addr, value = pairs[0]
    f.write(f"    {{ {addr}, {value}, false }},\n")

    f.write("{\n")
    for i in range(1,1+3):
        addr, value = pairs[i]
        f.write(f"    {{ {addr}, {value}, false }},\n")
    addr, value = pairs[4]
    f.write(f"    {{ {addr}, {value}, false }}\n")
    f.write("},\n")

    for i in range(5,7):
        addr, value = pairs[i]
        f.write(f"    {{ {addr}, {value}, false }},\n")

    for j in range(0, 3):
        f.write("{\n")
        for i in range(7+8*j,7+8*(j+1)-1):
            addr, value = pairs[i]
            f.write(f"    {{ {addr}, {value}, false }},\n")
        addr, value = pairs[7+8*(j+1)-1]
        f.write(f"    {{ {addr}, {value}, false }}\n")
        f.write("},\n")

    for i in range(31,35):
        addr, value = pairs[i]
        f.write(f"    {{ {addr}, {value}, false }},\n")

    f.write("{\n")
    for i in range(35,38):
        addr, value = pairs[i]
        f.write(f"    {{ {addr}, {value}, false }},\n")
    addr, value = pairs[38]
    f.write(f"    {{ {addr}, {value}, false }}\n")
    f.write("},\n")
    addr, value = pairs[39]
    f.write(f"    {{ {addr}, {value}, false  }},\n")

    f.write(f"    {{ 0x2D, 0x00, true }},\n")
    f.write(f"    {{ 0x2E, 0x00, true }},\n")
    f.write(f"    {{ 0x2F, 0x31, true }},\n")

    f.write("};")

print(f"Converted {len(pairs)} register pairs.")
print(f"Output written to {OUTPUT_FILE}")