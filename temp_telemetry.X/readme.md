


# Temp Telemetry Project

## Overview

The Temp Telemetry Project implements a minimal temperature‐sensing telemetry node using a Microchip SAME51J20A Curiosity Nano development board. It is designed to act as an I²C slave “telemetry MCU” that reads two differential channels from an external ADS1115 (configured here as the “PSM” device) and serves these readings over I²C. While the immediate motivation was to work around I²C communication issues between an NVIDIA Orin host and the ADS1115 (“PSM”), this codebase is intended as a reusable template for integrating an auxiliary telemetry MCU into larger systems (e.g., for backup or redundancy of analog sensor reads).

Key features:
- Configures an ADS1115 over I²C to perform two differential ADC conversions (channels 0–1 and 2–3) at 128 SPS.
- Implements a SAM E51 (SAME51J20A) as an I²C‐slave device (SERCOM3) that triggers new conversions on-demand when the I²C master writes a single‐byte command (0x00), then returns four bytes of raw ADC data (two 16-bit readings) on read.
- Provides a straightforward, interrupt‐driven callback (`sercom3_i2c_calback`) to handle address matches, data reception, and data transmission.
- Uses SysTick as a timebase (currently no periodic tasks, but ready for future scheduling).
- Initializes clocks, pin multiplexing, NVIC, and I²C peripherals via MPLAB Harmony–style `system_init` calls.

## Pinout and Wiring
- **SERCOM0 (I²C Master)**:
  - `PA08` → SDA (ADS1115)
  - `PA09` → SCL (ADS1115)
- **SERCOM3 (I²C Slave)**:
  - `PA22` → SDA (External Host)
  - `PA23` → SCL (External Host)

## Hardware Requirements

- **Microchip SAME51J20A Curiosity Nano Board** (Dev Board “CURIOSITY_NANO_A1_SAME51J20A”).
- **ADS1115 ADC Module** (I²C address 0x48; often labeled as “PSM” in this code). Two analog inputs wired as differential pairs:
  - AIN0 ↔ AIN1 (channel “0–1” differential)
  - AIN2 ↔ AIN3 (channel “2–3” differential)
- **Pull‐up resistors** (4.7 kΩ – 10 kΩ) on each I²C line (if not already on ADS1115 breakout board).
- **Level wiring**:
  - SAME51J20A SERCOM0 SDA → ADS1115 SDA
  - SAME51J20A SERCOM0 SCL → ADS1115 SCL
  - SAME51J20A SERCOM3 SDA → I²C Master Host SDA
  - SAME51J20A SERCOM3 SCL → I²C Master Host SCL
  - Common GND between all boards.
  - 3.3 V (or 5 V, depending on ADS1115 board) power supply to ADS1115.

> **Note:** In this design, the SAME51 acts as an I²C repeater/translator: it is an I²C master toward the ADS1115 (SERCOM0) and an I²C slave toward the external host (SERCOM3). Be sure that both I²C buses share a common reference (ground). Level shifting may be required if host I²C voltage differs from 3.3 V.

## Software Requirements

- **MPLAB X IDE** (version 5.50 or later).
- **XC32 Compiler** (version 4.0 or later) or ARM GCC toolchain that targets Cortex-M4.
- **Atmel START** (optional) or Harmony Configurator to generate `system_init.h`, peripheral drivers, and clock setup for SAME51J20A.
- **CMSIS** and **ASF4** (if using Atmel START or Harmony).

