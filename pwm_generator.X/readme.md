

# **PWM Generator MCU Documentation**

## **Overview**
The **PWM Generator MCU** is responsible for generating precise PWM signals to control **thrusters**. The MCU receives **duty cycle commands** (in microseconds) from the **Jetson Orin** over **I²C** and converts them into appropriate PWM signals.

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

### **Communication Interfaces**
- **CAN FD (Controller Area Network)**
  - Supports real-time data exchange.

### **Other Peripherals**
- **Power Manager (PM)**
- **Watchdog Timer (WDT)** for system reliability.

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
| `0x369` | **STOP_GRIPPER** |
| `0x36A` | **START_GRIPPER** |
| `0x36B` | **SET_PWM** |
| `0x36C` | **RESET_MCU** |

---

### **I²C Communication**

#### **I²C Device Address**
- The MCU has an I²C **address of `0x21`** (configurable).

#### **I²C Start Byte Table**
To **distinguish different message types**, a **start byte** is used.

| **START BYTE** | **Message Description** |
|--------------|----------------------|
| `0x0` | **SEND_PWM** (Set PWM duty cycle) |
| `0x1` | **STOP_GRIPPER** |
| `0x2` | **START_GRIPPER** |
| `0x3` | **RESET_MCU** |

---

## **Future Improvements**
🔹 **Direct Memory Access (DMA)** → Reducing CPU workload during data transfers.  
🔹 **Interrupt-Based I²C Handling** → Enhancing real-time communication efficiency.  
🔹 **Low Power Mode Implementation** → Optimizing power consumption for extended operation.  
🔹 **Enhanced Error Handling** → Improving CAN FD message filtering & I²C communication reliability.  

---

## **Additional Notes**
- The MCU ensures **accurate PWM generation** to maintain **precise thruster control**.
- **DMA and interrupt-based processing** will be added in future firmware updates to **enhance real-time performance**.

---

## **Changelog**
| **Version** | **Changes** |
|------------|------------|
| `1.0` | Initial documentation release |
| `1.1` | Added CAN & I²C protocol tables |
| `1.2` | Expanded TCC period calculation |

---

## **References**
- **Microchip MPLAB X IDE**: [Microchip MPLAB Website](https://www.microchip.com/mplab/mplab-x-ide)
- **SAMC21 Datasheet**: [SAMC21 Microchip Documentation](https://www.microchip.com/wwwproducts/en/SAMC21)
- **CAN FD Protocol Specification**: [CAN FD ISO 11898-1](https://www.iso.org/standard/66047.html)

---



