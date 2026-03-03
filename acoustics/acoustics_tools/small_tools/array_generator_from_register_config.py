INPUT_FILE = "AD7606_Register_Data.txt"
OUTPUT_FILE = "regs_output.c"
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
    f.write(f"const uint8_t {ARRAY_NAME}[] =\n{{\n")

    for addr, value in pairs:
        f.write(f"    {addr}, {value},\n")

    f.write("};\n\n")
    f.write(f"int conf_len = {len(pairs)};\n")

print(f"Converted {len(pairs)} register pairs.")
print(f"Output written to {OUTPUT_FILE}")