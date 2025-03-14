
# **PWM Generator MCU Documentation**


## **How to Use**

1. **Download and Install MPLAB X IDE**  
   - Ensure you have the latest version of **MPLAB X IDE** installed.  
   - If you haven’t installed it yet, download it from the [Microchip website](https://www.microchip.com/mplab/mplab-x-ide).

2. **Import the `pwm_generator.x` Project**  
   - Open **MPLAB X IDE**.
   - Go to **File > Open Project** and select the `pwm_generator.x` project.

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
- **Backup I2C Communication**
  - `SERCOM3`
- **Power Manager (PM)**
- **Communication Interfaces**
  - **CAN FD** (Controller Area Network) for real-time data exchange
- **Watchdog Timer (WDT)**

---
## **MCU explanation**

This MCU is used to generate PWM signals for the thrusters. There is a total of eight truster.
The MCU recieves duty cycle in microseconds from the Jetson Orin and converts it into the actual duty cycle.


---


## **CAN ID Table**
CAN FD in FIFO0 is set to only accept ID in the range 0x469 to 0x479.
Each ID has its own purpose. Below is a table showing what each ID used for. 

| ID | Message |
| -------------- | --------------- |
| 0x369 | STOP_GRIPPER |
| 0x36A | START_GRIPPER |
| 0x36B | SET_PWM |
| 0x36C | RESET_MCU |

## **I2C STARTBYTE Table**
To distinguish different types of I2C messages a start byte is used.
Below is a table showing what each start byte means.

The MCU will have an address of 0x21 (can be configured)


| STARTBYTE   | Message   |
|--------------- | --------------- |
| 0x0   | SEND_PWM   |
| 0x1   | STOP_GRIPPER   |
| 0x2   | START_GRIPPER   |
| 0x3   | RESET_MCU   |

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

### **Example Calculation**
For a **PWM period of 20 ms (50 Hz)**:

$$
\text{TCC PERIOD} = \frac{(48,000,000 / 4) \times 0.020}{2} - 1
$$
$$
= \frac{12,000,000 \times 0.020}{2} - 1
$$
$$
= {120,000} - 1
$$
$$
= 119,999
$$

Thus, for **50 Hz PWM**, the **TCC period should be set to `119999`**.

---

## **Future Improvements**
- 🔹 **Direct Memory Access (DMA)** for efficient data handling.
- 🔹 **Sleep Mode Implementation** for power efficiency.

---

## **Additional Notes**
- The **duty cycle computation** ensures that the received PWM signal is **converted accurately** for motor control.
- Implementing **DMA & interrupt-based** processing will further **reduce MCU workload and improve real-time performance**.

