# Motor Control Unit

The motor control unit is responsible for receiving CAN FD commands, generating PWM signals for thrusters and camera lights, monitoring thruster current draw, and reporting hardware fault events back over CAN.

## Hardware Overview

| Item               | Detail                                               |
| ------------------ | ---------------------------------------------------- |
| MCU                | Microchip SAMC21J18A                                 |
| Framework          | MPLAB Harmony 3                                      |
| CAN peripheral     | CAN1 (CAN FD with bit-rate switching)                |
| PWM outputs        | 8× thrusters (TCC0/1/2), 1× camera light (TC3)       |
| Current monitoring | ADC0 thruster channels via DMA sleepwalking          |
| Fault inputs       | 8× FLT pins (EIC EXTINT), 8× PGOOD pins (EIC EXTINT) |
| Killswitch         | NMI via EIC                                          |

## Functional Overview

### PWM Generation

Eight thrusters are driven by TCC0, TCC1, and TCC2 in Normal PWM mode. The camera light is driven by TC3 in Match PWM mode (MPWM). All outputs run at 50 Hz (20 ms frame period).

| Output | Timer | Channel | Pulse Range (µs) | Neutral (µs) |
| ------ | ----- | ------- | ---------------- | ------------ |
| TH1    | TCC2  | CC0     | 1000–2000        | 1500         |
| TH2    | TCC2  | CC1     | 1000–2000        | 1500         |
| TH3    | TCC1  | CC0     | 1000–2000        | 1500         |
| TH4    | TCC1  | CC1     | 1000–2000        | 1500         |
| TH5    | TCC0  | CC1     | 1000–2000        | 1500         |
| TH6    | TCC0  | CC0     | 1000–2000        | 1500         |
| TH7    | TCC0  | CC3     | 1000–2000        | 1500         |
| TH8    | TCC0  | CC2     | 1000–2000        | 1500         |
| Light  | TC3   | CC1     | 1100–1900        | 1100         |

### CAN FD Interface

The firmware uses CAN1 in FD mode with bit-rate switching (BRS). All received frames are read from Rx FIFO 0.

#### Received Command Frames

| CAN ID  | Command              | Payload                                                   |
| ------- | -------------------- | --------------------------------------------------------- |
| `0x369` | `TURN_THRUSTERS_OFF` | None — sets all 8 thrusters to neutral                    |
| `0x36A` | `TURN_LIGHTS_OFF`    | None — sets camera light to neutral                       |
| `0x36B` | `RESET`              | None — triggers `NVIC_SystemReset()`                      |
| `0x36C` | `SET_THRUSTER_PWM`   | 8× `uint16_t` pulse widths in µs (little-endian, TH1–TH8) |
| `0x36D` | `SET_LIGHT_PWM`      | 1× `uint16_t` pulse width in µs (little-endian)           |

#### Transmitted Event Frames

All outgoing frames are transmitted on CAN ID `0x45A` in CAN FD format.

| `data[0]` | `data[1]`     | Event                                                                                   |
| --------- | ------------- | --------------------------------------------------------------------------------------- |
| `0x00`    | —             | Current measurements (bytes 1–32: 8× `float`, one per thruster, in amps, little-endian) |
| `0x01`    | Channel index | FLT fault event for the given channel                                                   |
| `0x02`    | Channel index | PGOOD event for the given channel                                                       |
| `0x03`    | —             | Killswitch triggered                                                                    |

#### Current Measurement Frame Encoding

When `data[0] == 0x00`, bytes 1–32 contain 8 thruster current values packed sequentially. Each value is a 32-bit single-precision IEEE 754 float encoded as its raw binary representation using `memcpy` — **not** as an integer or scaled integer. The byte order is **little-endian**.

```
data[0]        = 0x00  (message type: current measurements)
data[1..4]     = TH1 current (IEEE 754 float, little-endian)
data[5..8]     = TH2 current (IEEE 754 float, little-endian)
data[9..12]    = TH3 current (IEEE 754 float, little-endian)
data[13..16]   = TH4 current (IEEE 754 float, little-endian)
data[17..20]   = TH5 current (IEEE 754 float, little-endian)
data[21..24]   = TH6 current (IEEE 754 float, little-endian)
data[25..28]   = TH7 current (IEEE 754 float, little-endian)
data[29..32]   = TH8 current (IEEE 754 float, little-endian)
```

When decoding, each value needs to be reconstructed using `memcpy` (or equivalent) into a `float` variable — do not cast or interpret the bytes as an integer type. Units are **amps**.

Example decode in C:

```c
float current[8];
for (int i = 0; i < 8; i++) {
    memcpy(&current[i], &data[1 + i * sizeof(float)], sizeof(float));
}
```

### Current Monitoring

Thruster current is measured continuously via ADC0 with DMA sleepwalking. On each DMA transfer complete, `log_current()` converts the raw ADC samples to amps using the efuse IMON transfer function and transmits the results over CAN.

**Conversion formula:**

```
I_out = V_Imon / (G_Imon × R_Imon)

V_Imon = (raw_adc / 4095) × 5.0 V
G_Imon = 18.31 µA/A
R_Imon = 2.697 kΩ
```

#### ADC Channel Mapping

| ADC Channel | Thruster |
| ----------- | -------- |
| AIN0        | TH3      |
| AIN1        | TH4      |
| AIN2        | TH1      |
| AIN4        | TH2      |
| AIN5        | TH5      |
| AIN6        | TH6      |
| AIN7        | TH7      |
| AIN9        | TH8      |

### Fault & Event Handling

FLT and PGOOD signals are monitored via EIC external interrupts. Callbacks set bits in a pending event mask, which is drained in the main task loop and reported over CAN. The killswitch is handled via the NMI.

---
