Battery Management System (BMS) — TI BQ76942
Overview

> [!NOTE]
> Pages about Direct commands and subcommands is on page (13-14) and Data Memory settings on page (125) in BQ-TI datasheet:
> https://www.ti.com/lit/ug/sluuby1b/sluuby1b.pdf?ts=1761234036230&ref_url=https%253A%252F%252Fcopilot.microsoft.com%252F

The goal of this work is to implement and understand the Texas Instruments BQ76942 battery management IC for use in Vortex NTNU’s AUV power system.
The IC manages various lithium cells, providing voltage, current, and temperature protection as well as cell balancing and fault monitoring.

> [!NOTE]
> All firmware is written in bare-metal C for the Microchip SAMC21 microcontroller. Communication between the MCU and the BQ76942 uses SPI.

What Has Been Done
1. IC Initialization & Configuration

Implemented routines to enter CONFIG_UPDATE mode for safe parameter editing.

Wrote register configuration for:

Overvoltage (COV) and Undervoltage (CUV) thresholds

Discharge and Charge FET control

Battery Status

Read cell voltages

Protections and alert handling

> [!TIP]
> Using the TI register map, thresholds are calculated based on cell voltage divided by 50.6 mV per bit, giving accurate programmable limits.

2. Power Mode Handling

Need to verify the IC’s transition between:

NORMAL → SLEEP → DEEPSLEEP → SHUTDOWN

Implemented RST_SHUT pin behavior for controlled shutdown and wake-up.

Discovered that configuration commands must be resent after SHUTDOWN, since register memory is cleared.

3. Protection & Fault Behavior

Explored COV/CUV protection activation logic and timing.

Confirmed ALERT pin triggers correctly when thresholds are crossed.

Observed autonomous recovery works after voltage returns to safe range.

> [!IMPORTANT]
> The BQ76942 protection system runs independently from the host MCU, meaning faults are handled even if communication is lost.

4. Communication & Command Structure

Verified SPI protocol operation using the Direct Command and Subcommand interface.

Implemented functions for:

Sending command-only subcommands (e.g., RESET, SEAL, UNSEAL)

Reading measurement registers (cell voltages, pack current, temperature)

🔍 Key Discoveries

RST_SHUT pin: Can both reset and shut down the IC depending on logic level and pulse duration.

Dual ADC design allows simultaneous current and voltage sampling for precise protection timing.

REG1/REG2 LDOs can power external logic (3.3 V or 5 V), reducing component count.

OTP memory stores permanent configuration — useful for final production calibration.

> [!NOTE]
> The system now successfully reads live cell voltages, controls protection FETs, and reports alerts over SPI.
Next focus: integrate fault reporting over CAN and test full pack balancing.
