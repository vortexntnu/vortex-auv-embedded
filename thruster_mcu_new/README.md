# Motor Control Unit

The motor control unit is responsible for receiving commands, generating PWM signals
for thrusters and camera lights, monitoring thruster current draw, and reporting
hardware fault events.

> **Note — Easter testing hardware change:** Shortly before Easter testing the original
> SAMC21J18A short-circuited and was replaced with a **revision B** device. Revision B
> has a known OSC48M errata that prevents running faster than 4 MHz from internal
> oscillators, down from the original 48 MHz. The degraded clock made PWM accuracy
> insufficient for reliable thruster control over CAN FD, so the communication
> interface was switched to **UART** for Easter testing.

## Hardware Overview

| Item               | Detail                                               |
| ------------------ | ---------------------------------------------------- |
| MCU                | Microchip SAMC21J18A (revision B)                    |
| Framework          | MPLAB Harmony 3                                      |
| Communication      | UART over SERCOM2                                    |
| PWM outputs        | 8× thrusters (TCC0/1/2), 1× camera light (TC3)       |
| Current monitoring | ADC0 thruster channels via DMA sleepwalking          |
| Fault inputs       | 8× FLT pins (EIC EXTINT), 8× PGOOD pins (EIC EXTINT) |
| Killswitch         | NMI via EIC                                          |

## Functional Overview

### PWM Generation

Eight thrusters are driven by TCC0, TCC1, and TCC2 in Normal PWM mode. The camera
light is driven by TC3 in Match PWM mode (MPWM). All outputs run at 50 Hz (20 ms
frame period).

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

### UART Interface
The firmware communicates over SERCOM2 USART in asynchronous mode (UART). Frames
follow the format below.

[ 0xAA | MSG_ID | LENGTH | PAYLOAD (LENGTH bytes) | CHECKSUM ]

Checksum is XOR of MSG_ID, LENGTH, and all payload bytes.

#### Received Command Frames

| MSG_ID | Command              | Payload                                                    |
| ------ | -------------------- | ---------------------------------------------------------- |
| `0x01` | `TURN_THRUSTERS_OFF` | None — sets all 8 thrusters to neutral                     |
| `0x02` | `TURN_LIGHTS_OFF`    | None — sets camera light to neutral                        |
| `0x03` | `RESET`              | None — triggers `NVIC_SystemReset()`                       |
| `0x04` | `SET_THRUSTER_PWM`   | 8× `uint16_t` pulse widths in µs (native-endian, TH1–TH8) |
| `0x05` | `SET_LIGHT_PWM`      | 1× `uint16_t` pulse width in µs (native-endian)            |

#### Transmitted Event Frames

| MSG_ID | Event                  | Payload                                              |
| ------ | ---------------------- | ---------------------------------------------------- |
| `0x10` | FLT fault event        | `[channel, 0x01]`                                    |
| `0x11` | PGOOD event            | `[channel, 0x02]`                                    |
| `0x12` | Killswitch triggered   | None                                                 |
| `0x13` | Current measurements   | 8× `float` (IEEE 754, little-endian) — see below     |

#### Current Measurement Frame Encoding

Bytes 0–31 contain 8 thruster current values packed sequentially. Each value is a
32-bit single-precision IEEE 754 float encoded as its raw binary representation using
`memcpy` — **not** as an integer or scaled integer. Units are **amps**.

bytes 0..3   = TH1 current (IEEE 754 float, little-endian)

bytes 4..7   = TH2 current (IEEE 754 float, little-endian)

bytes 8..11  = TH3 current (IEEE 754 float, little-endian)

bytes 12..15 = TH4 current (IEEE 754 float, little-endian)

bytes 16..19 = TH5 current (IEEE 754 float, little-endian)

bytes 20..23 = TH6 current (IEEE 754 float, little-endian)

bytes 24..27 = TH7 current (IEEE 754 float, little-endian)

bytes 28..31 = TH8 current (IEEE 754 float, little-endian)

Example decode in C:

```c
float current[8];
for (int i = 0; i < 8; i++) {
    memcpy(&current[i], &data[i * sizeof(float)], sizeof(float));
}
```

### Current Monitoring

Thruster current is measured continuously via ADC0 with DMA sleepwalking. On each DMA
transfer complete, `log_current()` converts the raw ADC samples to amps using the
efuse IMON transfer function and transmits the results over UART.

**Conversion formula:**

I_out = V_Imon / (G_Imon × R_Imon)

V_Imon = (raw_adc / 4095) × 5.0 V

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

FLT and PGOOD signals are monitored via EIC external interrupts. Callbacks set bits in
a pending event mask, which is drained in the main task loop and reported over UART.
The killswitch is handled via the NMI.
