

# **PWM Generator MCU Documentation**

## **Before you read**
The pwm_generator was originally designed to use the SAMC21E17A (same MCU as the gripper). However, we have since realized that the SAMC21E17A does not have enough distinct PWM channels.
If we in the future decide to use a similar setup a different MCU has to be chosen. For instance a MCU from the SAME5x series will have enough distinct channels. 
This is an import lesson for future embedded teams. Coordinate with electrical to avoid these errors in the future

## **Overview**
The **PWM Generator MCU** is responsible for generating precise PWM signals to control **thrusters**. The MCU receives **duty cycle commands** (in microseconds) from the **Jetson Orin** over **CAN** and converts them into appropriate PWM signals.

This document provides details on **setup, peripherals, communication protocols, CAN IDs, I²C messages, and timer calculations**.

---

## **How to Use**

### **1. Download and Install MPLAB X IDE**
- Ensure you have the latest version of **MPLAB X IDE** installed.
- Download it from the [Microchip website](https://www.microchip.com/mplab/mplab-x-ide).

### **2. Import the `pwm_generator.x` Project**
- Open **MPLAB X IDE**.
- Go to **File > Open Project** and select `pwm_generator.x`.

### **3. Connect the SAMC21E17A to Your Computer**
- Use a **compatible programmer/debugger**:
  - **PICkit 4**
  - **Atmel-ICE**
  - **MPLAB SNAP**
- Ensure the **SWD interface** is properly connected.

### **4. Build and Program the Device**
- Click **Make and Program Device** (`F5`) to compile and flash the firmware.
- Verify the output in the **MPLAB X console** to ensure successful programming.

---

## **Peripheral Overview**
The **PWM Generator MCU** utilizes the following peripherals:

### **Timers (TCC - Timer/Counter for Control)**
- `TCC0` → **Primary PWM Generator**
- `TCC1` → **Backup/Additional PWM Channels**

### **I²C Interfaces (SERCOM - Serial Communication)**
- `SERCOM0` → **I²C 3 (USART for debugging)**
- `SERCOM1` → **I²C 2**
- `SERCOM3` → **I²C 1**

### **Backup I²C Communication**
- `SERCOM3` (Fallback communication channel)
- Uses **0x21** as i2c address

### **Communication Interfaces**
- **CAN FD (Controller Area Network)**
  - Supports real-time data exchange.

### **Other Peripherals**
- **Power Manager (PM)**
- **Watchdog Timer (WDT)** for system reliability.

---


## Pinout

### LED Pins
| Pin  | Signal       | Direction | Notes               |
|------|--------------|-----------|---------------------|
| PA2  | Led_enable   | Output    | Enables LED drive   |
| PA17 | LED_Red      | Output    | Red LED control     |
| PA18 | LED_Yellow   | Output    | Yellow LED control  |
| PA19 | LED_Green    | Output    | Green LED control   |
---

### PWM Pins

#### TCC0 Channels
| Pin  | Timer/Counter | Channel (WO) | Purpose             |
|------|---------------|--------------|---------------------|
| PA4  | TCC0          | WO[0] (C0)   | PWM output (thruster 1) |
| PA5  | TCC0          | WO[1] (C1)   | PWM output (thruster 2) |
| PA10 | TCC0          | WO[2] (C2)   | PWM output (thruster 3) |
| PA11 | TCC0          | WO[3] (C3)   | PWM output (thruster 4) |


#### TCC1 Channels
| Pin  | Timer/Counter | Channel (WO) | Purpose             |
|------|---------------|--------------|---------------------|
| PA6  | TCC1          | WO[0] (C0)   | PWM output (thruster 5) |
| PA7  | TCC1          | WO[1] (C1)   | PWM output (thruster 6) |
| PA8  | TCC1          | WO[2] (C2)   | PWM output (thruster 7) |
| PA9  | TCC1          | WO[3] (C3)   | PWM output (thruster 8) |

> **Note:**  
> PWM channels 2 and 3 are not independent.  
> - Channel 2 mirrors Channel 0’s waveform.  
> - Channel 3 mirrors Channel 1’s waveform.  
>   
> As a result, this configuration does not provide enough independent PWM outputs for controlling 8 thrusters.


#### TCC2 Channels
| Pin   | Timer/Counter | Channel (WO) | Purpose                 |
|-------|---------------|--------------|-------------------------|
| PA16  | TCC2          | WO[0] (C0)   | PWM output (aux 1)      |
| PA17  | TCC2          | WO[1] (C1)   | PWM output (aux 2)      |

> **Note:**  
> TCC2 provides 16-bit resolution, compared to the 24-bit resolution of TCC0 and TCC1.  
> This should not be a significant limitation, since 16-bit resolution is sufficient for our purposes.



#### TC4 Channels (LED PWM)
| Pin   | Timer/Counter | Channel (WO) | Purpose            |
|-------|---------------|--------------|--------------------|
| PA14  | TC4           | WO[0] (C0)   | PWM for indicator LED 1 |
| PA15  | TC4           | WO[1] (C1)   | PWM for indicator LED 2 |

> **Note:**  
> TC4 can also generate waveform output, but it is not designed specifically for control applications.  
> For control systems, the TCCx peripherals are preferred.  
> However, for tasks such as LED modulation, the simpler TCx peripherals are sufficient.



---

### I²C / USART (SERCOM3)
| Pin   | Function       | Peripheral | Notes                              |
|-------|----------------|------------|------------------------------------|
| PA22  | SERCOM3 PAD0   | I²C SDA    | I²C slave SDA (or USART RX)        |
| PA23  | SERCOM3 PAD1   | I²C SCL    | I²C slave SCL (or USART TX)        |


---

### CAN0
| Pin   | Function  | Peripheral |
|-------|-----------|------------|
| PA24  | CAN0_TX   | CAN0       |
| PA25  | CAN0_RX   | CAN0       |


---

## **MCU Functionality**
- The **MCU generates PWM signals** to control **eight thrusters**.
- It **receives duty cycle data** (in microseconds) from the **Jetson Orin** via **I²C**.
- The received duty cycle is **converted into an actual PWM duty cycle** and applied to the thrusters.

---

## **Communication Protocols**

### **CAN ID Table**
The **CAN FD** interface is configured with **FIFO0** to accept **IDs from `0x469` to `0x479`**.

| **CAN ID** | **Message Description** |
|------------|-------------------------|
| `0x369` | **STOP_GENERATOR** |
| `0x36A` | **START_GENERATOR** |
| `0x36B` | **SET_PWM** |
| `0x36C` | **LED** |
| `0x36D` | **RESET_MCU** |

---

### **I²C Communication**

#### **I²C Device Address**
- The MCU has an I²C **address of `0x21`** (configurable).

#### **I²C Start Byte Table**
To **distinguish different message types**, a **start byte** is used.

| **START BYTE** | **Message Description** |
|--------------|----------------------|
| `0x0` | **SEND_PWM** (Set PWM duty cycle) |
| `0x1` | **STOP_GENERATOR** |
| `0x2` | **START_GENERATOR** |
| `0x3` | **LED** |
| `0x4` | **RESET_MCU** |

---

## **Future Improvements**
🔹 **Direct Memory Access (DMA)** → Reducing CPU workload during data transfers.  
🔹 **Low Power Mode Implementation** → Optimizing power consumption for extended operation.  
🔹 **Enhanced Error Handling** → Improving CAN FD message filtering & I²C communication reliability.  

---

## **Additional Notes**
- The MCU ensures **accurate PWM generation** to maintain **precise thruster control**.
- **DMA and interrupt-based processing** will be added in future firmware updates to **enhance real-time performance**.

The reason the SetPWMThruster function does not use a for loop is to avoid if-statements or multiple for-loops, but 
mostly to accomodate electrical and auto in the order which the pwm values are set

---

## **References**
- **Microchip MPLAB X IDE**: [Microchip MPLAB Website](https://www.microchip.com/mplab/mplab-x-ide)
- **SAMC21 Datasheet**: [SAMC21 Microchip Documentation](https://ww1.microchip.com/downloads/en/DeviceDoc/SAMC20_C21_Family_Data_Sheet_DS60001479D.pdf)
- **CAN FD Protocol Specification**: [CAN FD ISO 11898-1](https://www.iso.org/standard/66047.html)

---



