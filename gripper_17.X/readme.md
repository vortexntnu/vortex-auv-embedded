
# **Gripper MCU Documentation**


## **How to Use**

1. **Download and Install MPLAB X IDE**  
   - Ensure you have the latest version of **MPLAB X IDE** installed.  
   - If you haven’t installed it yet, download it from the [Microchip website](https://www.microchip.com/mplab/mplab-x-ide).

2. **Import the `gripper_17.x` Project**  
   - Open **MPLAB X IDE**.
   - Go to **File > Open Project** and select the `gripper_17.x` project.

3. **Connect the SAMC21E17A to Your Computer**  
   - Use a **compatible programmer/debugger** (e.g., **PICkit 4, Atmel-ICE, or MPLAB SNAP**).  
   - Ensure the **SWD interface** is properly connected.  

4. **Build and Program the Device**  
   - Click **Make and Program Device** (`F5`) to compile and flash the firmware.  
   - Verify the output in the MPLAB X console to ensure successful programming.
---


## **Peripheral Overview**
The **Gripper MCU** utilizes the following peripherals:

- **Timers (TCC - Timer/Counter for Control)**
  - `TCC0`
  - `TCC1`
- **I²C Interfaces (SERCOM - Serial Communication)**
  - `SERCOM0` → I²C 3 (USART for debugging)
  - `SERCOM1` → I²C 2
  - `SERCOM3` → I²C 1
- **Analog Digital Converter**
  - `ADC0` 
- **Direct Memory Access Controller (DMAC)**
- **Real Time Counter (RTC)**
- **Backup I2C Communication**
  - `SERCOM3` Adress = 0x31
- **Power Manager (PM)**
- **Communication Interfaces**
  - **CAN FD** (Controller Area Network) for real-time data exchange
- **Watchdog Timer (WDT)**
- **Event Systems (EVSYS)**

---

## Functionality

After initialization, the microcontroller immediately enters **idle mode**.  
It will only wake up when one of the following interrupts occurs:

- **CAN_RX interrupt** – triggered on reception of a CAN frame  
- **I2C_RX interrupt** – triggered on reception of an I²C message  
- **DMA interrupt** – triggered after **1024 ADC samples** are collected  

---

### Message Handling

Once the processor wakes up, it calls the `message_handler` function.  
This function decides what action to take based on:

- The **CAN RX identifier** (`rx_id`), or  
- The **first byte** of the incoming I²C message  

See the reference tables below for how IDs and command bytes map to actions.

---

### **2️⃣SET_PWM M**

When the MCU successfully receives a **PWM command** via CAN:

1. It reads the received **PWM value** (in **microseconds**).  
2. It converts this value into a **duty cycle** using the formula:  
   \text{Duty Cycle} =
   \frac{\text{DATA\_MICROSECONDS} \times (\text{TCC\_PERIOD} + 1)}
   {\text{PWM\_PERIOD\_MICROSECONDS}}
   $$

3. The resulting duty cycle is applied to the configured TCC channel.  
4. Calls the `Encoder_Read` function to read all connected encoders.  
5. Ensures that encoder reads complete in a timely manner.  
6. If the MCU becomes stuck in a loop (e.g., encoder not responding), a **Watchdog Timer Reset** will be triggered to recover safely.  
   $$

**Notes:**
- Valid PWM signal range: **700 µs → 2300 µs**  
- `PWM_PERIOD`: **2000 µs**  
- `TCC_PERIOD`: **119999**  

---


## **CAN ID Table**
CAN FD in FIFO0 is set to only accept ID in the range 0x469 to 0x479.
Each ID has its own purpose. Below is a table showing what each ID used for. 

| ID | Message |
| -------------- | --------------- |
| 0x469 | STOP_GRIPPER |
| 0x46A | START_GRIPPER |
| 0x46B | SET_PWM |
| 0x46C | RESET_MCU |

## **I2C STARTBYTE Table**
To distinguish different types of I2C messages a start byte is used.
Below is a table showing what each start byte means.


| STARTBYTE   | Message   |
|--------------- | --------------- |
| 0x0   | SEND_PWM   |
| 0x1   | STOP_GRIPPER   |
| 0x2   | START_GRIPPER   |
| 0x3   | RESET_MCU   |

###  Current Sensing and Overcurrent Protection

This system monitors the **current consumption** of **servo motors** using an **ADC (Analog-to-Digital Converter)**, **RTC (Real-Time Counter)**, and **DMA (Direct Memory Access)** to ensure efficient sampling and protection against overcurrent conditions.

###  How It Works
1. **ADC Sampling (Triggered by RTC):**  
   - The **ADC measures the voltage** across the servo power lines.  
   - **DMA transfers 16 ADC samples** (each an average of 1024 readings) to reduce noise.  

2. **Overcurrent Detection:**  
   - The **averaged ADC value** is compared to a **threshold (`VOLTAGE_THRESHOLD`)**.  
   - If **any sample exceeds the limit**, the **corresponding servo enable pin is disabled**.

3. **Servo Cycling:**  
   - The system monitors **three servos (`SERVO_1`, `SERVO_2`, `SERVO_3`)** in a loop.  
   - If an overcurrent condition is detected, the **servo is turned off**.  
   - The **ADC input is switched to the next servo** for continuous monitoring.  

4. **DMA Restart:**  
   - After processing, the **DMA transfer restarts**, ensuring **continuous current monitoring** with minimal CPU usage.

###  Key Features
✅ **RTC + DMA ensures efficient ADC sampling** without CPU blocking.  
✅ **16-sample averaging (each 1024 ADC readings)** minimizes noise.  
✅ **Automatic servo disable on overcurrent** prevents damage.  
✅ **Cyclic servo monitoring** ensures all servos are checked.


## **TCC Period Calculation**
To correctly configure **TCC (Timer/Counter for Control)** for PWM generation, we calculate the **TCC period** using the given clock and prescaler.

### **Given Parameters**
- **Clock Frequency**: `48 MHz`
- **Prescaler**: `4`
- **Desired PWM Period** (in microseconds): `PWM_PERIOD_MICROSECONDS`

### **Formula for TCC Period**

$$
\text{TCC PERIOD} = \frac{(\text{Clock Frequency} / \text{Prescaler}) \times \text{PWM Period (μs)}}{2} - 1
$$

## **Future Improvements**
- 🔹 **Direct Memory Access (DMA)** for efficient data handling.
- 🔹 **Sleep Mode Implementation** for power efficiency.

---

## **Additional Notes**
- The **duty cycle computation** ensures that the received PWM signal is **converted accurately** for motor control.
- **Encoder data handling** is optimized to ensure **precise angular position measurements** before transmission.
- Implementing **DMA & interrupt-based** processing will further **reduce MCU workload and improve real-time performance**.

---

## **References**
- **Microchip MPLAB X IDE**: [Microchip MPLAB Website](https://www.microchip.com/mplab/mplab-x-ide)
- **SAMC21 Datasheet**: [SAMC21 Microchip Documentation](https://ww1.microchip.com/downloads/en/DeviceDoc/SAMC20_C21_Family_Data_Sheet_DS60001479D.pdf)
- **CAN FD Protocol Specification**: [CAN FD ISO 11898-1](https://www.iso.org/standard/66047.html)

---
