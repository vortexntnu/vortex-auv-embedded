
# bms_e18a

## Structure

| Path                        | Purpose                                                                                                                             |
|-----------------------------|-------------------------------------------------------------------------------------------------------------------------------------|
| _build                      | The [CMake build tree](https://cmake.org/cmake/help/latest/manual/cmake.1.html#introduction-to-cmake-buildsystems), can be deleted. |
| cmake                       | Generated [CMake](https://cmake.org/) files. May be deleted if user.cmake has not been added                                        |
| .vscode                     | See [VSCode](https://code.visualstudio.com/docs/getstarted/settings)                                                                |
| .vscode/settings.json       | Workspace specific settings                                                                                                         |
| .vscode/bms_e18a.mplab.json | The MPLAB project file, should not be deleted                                                                                       |
| out                         | Final build artifacts                                                                                                               |

## SPI Test Brief

Current firmware runs a basic BQ76942 SPI voltage test from `main.c`.

1. `voltage_test_init()` runs once at boot:
- Initializes BQ76942 over SPI.
- Prints `BMS voltage test started` on UART.

2. `voltage_test_step()` runs continuously:
- Reads Cell1..Cell6 voltage registers over SPI.
- Sends one UART CSV line: `c1,c2,c3,c4,c5,c6` (mV).
- On failure, sends `read fail`.

UART:
- Interface: `SERCOM3` on PA22/PA23 (DEBUGTX/DEBUGRX).
- Typical terminal setup: `115200 8N1`.

LED meaning:
- `LED_G ON` + `LED_R OFF`: cell read success.
- `LED_R ON` + `LED_G OFF`: read failed.
- `LED_Y` toggles as heartbeat.
