# **Gripper MCU Documentation**

This document describes how to build, program, and operate the **Gripper MCU firmware**. It includes setup instructions, peripheral usage, communication interfaces, and system functionality.

---

## Getting Started

### 1. Install Development Tools

* Download and install the latest **[MPLAB X IDE](https://www.microchip.com/mplab/mplab-x-ide)**.
* Or you could use the **MPLAB VScode** extension
* Ensure you also have **XC32 compiler** installed (bundled with MPLAB X IDE).

### 2. Open the Project

* Launch **MPLAB X IDE**.
* Navigate to **File → Open Project**, then select the `gripper_17.x` project.

### 3. Connect the Hardware

* Target MCU: **ATSAMC21E17A**
* Programmer/debugger: **PICkit 4, Atmel-ICE, or MPLAB SNAP**
* Use the **SWD interface** to connect the MCU to your computer.

### 4. Build & Flash

* Press **F5** or select **Make and Program Device**.
* Monitor the **MPLAB X console output** to confirm successful programming.

---

## Peripheral Overview

The **Gripper MCU firmware** uses the following peripherals:

* **Timers (TCC – Timer/Counter for Control)**

  * `TCC0`, `TCC1` for PWM generation.

* **I²C Interfaces (SERCOM)**

  * `SERCOM0` → I²C 3 (USART for debugging)
  * `SERCOM1` → I²C 2
  * `SERCOM3` → I²C 1 + Backup I²C (address = `0x31`)

* **Analog-to-Digital Converter (ADC0)**

* **Direct Memory Access Controller (DMAC)**

* **Real-Time Counter (RTC)**

* **CAN FD** (for real-time communication)

* **Watchdog Timer (WDT)**

* **Event System (EVSYS)**

* **Power Manager (PM)**

---

##  System Behavior

After initialization, the MCU enters **idle mode**. It wakes up only when one of these interrupts occurs:

* **CAN\_RX interrupt** → triggered by incoming CAN frame.
* **I2C\_RX interrupt** → triggered by incoming I²C message.
* **DMA interrupt** → triggered when **1024 ADC samples** are collected.

### Message Handling

When awakened, the MCU runs `message_handler`, which decides actions based on:

* The **CAN RX ID**, or
* The **first byte** of an incoming I²C message.

---

## Command Handling

### **SET\_PWM (via CAN)**

When a **PWM command** is received:

1. MCU reads the PWM value (**µs**).
2. Converts it into a **duty cycle**.
3. Applies the duty cycle to the configured **TCC channel**.
4. Calls `read_encoders` to capture encoder values.
5. If an encoder does not respond, the **Watchdog Timer** resets the MCU safely.

**Notes:**

* Valid PWM input: **700–2300 µs**
* `PWM_PERIOD`: **2000 µs**
* `TCC_PERIOD`: **119,999**

---

## Communication Protocols

### CAN FD (FIFO0, IDs `0x469–0x479`)

| CAN ID  | Message        |
| ------- | -------------- |
| `0x469` | STOP\_GRIPPER  |
| `0x46A` | START\_GRIPPER |
| `0x46B` | SET\_PWM       |
| `0x46C` | RESET\_MCU     |

---

### I²C Start Byte Table

| START BYTE | Message        |
| ---------- | -------------- |
| `0x0`      | SEND\_PWM      |
| `0x1`      | STOP\_GRIPPER  |
| `0x2`      | START\_GRIPPER |
| `0x3`      | RESET\_MCU     |

---

##  Current Sensing & Protection

The MCU continuously monitors **servo motor current** using **ADC + RTC + DMA**.

### How It Works

1. **Sampling:**

   * ADC measures servo voltage.
   * DMA transfers **16 averaged samples** (each an average of 1024 readings).

2. **Overcurrent Detection:**

   * Compare ADC average against `VOLTAGE_THRESHOLD`.
   * If exceeded → **disable corresponding servo pin**.

3. **Servo Cycling:**

   * Rotates monitoring across `SERVO_1`, `SERVO_2`, `SERVO_3`.
   * Stops any servo exceeding current limits.

4. **DMA Restart:**

   * DMA is restarted automatically for continuous monitoring.

### Key Features

- **Efficient sampling with DMA + RTC**

- **Noise reduced by 16× averaging**

- **Automatic servo shutdown on overcurrent**

- **Continuous cyclic monitoring**

---

##  TCC Period Calculation

To configure **TCC** for PWM:

**Given Parameters**

* Clock: `48 MHz`
* Prescaler: `4`
* PWM Period: `PWM_PERIOD_MICROSECONDS`

**Formula**

$$
TCC_{PERIOD} = \frac{(Clock / Prescaler) \times PWM_{period}(µs)}{2} - 1
$$

---

##  Future Improvements

* Improve encoder angles sampling
* Use **DMA for all data handling** to further reduce CPU load.
* Implement **deep sleep mode** for better power efficiency.
* Improve **encoder handling** with timeout-based error recovery.

---

##  References

* [Microchip MPLAB X IDE](https://www.microchip.com/mplab/mplab-x-ide)
* [SAMC21 Datasheet](https://ww1.microchip.com/downloads/en/DeviceDoc/SAMC20_C21_Family_Data_Sheet_DS60001479D.pdf)
* [CAN FD ISO 11898-1](https://www.iso.org/standard/66047.html)

