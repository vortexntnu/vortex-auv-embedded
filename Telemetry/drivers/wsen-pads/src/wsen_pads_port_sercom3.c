#include "plib_sercom3_i2c_master.h"

#define WSEN_PADS_ADDR 0x5D  // SAO = 1 (0x5C if SAO = 0)

// Register addresses
#define REG_DEVICE_ID 0x0F
#define REG_CTRL_1 0x10
#define REG_CTRL_3 0x12
#define REG_STATUS 0x27
#define REG_DATA_P_XL 0x28
#define REG_DATA_T_L 0x2B

#define EXPECTED_DEVICE_ID 0xB3

int wsen_init(void) {
    uint8_t buf[2];

    // Set sampling rate
    uint8_t ctrl1 = 0x72;  // ODR 200Hz and BDU high
    buf[0] = REG_CTRL_1;
    buf[1] = ctrl1;
    if (!SERCOM3_I2C_Write(WSEN_PADS_ADDR, buf, 2)) {
        return -1;
    };
    // Enable data ready interrupts
    uint8_t ctrl3 = 0x04;  // DRDY = 1, INT_S = 00
    buf[0] = REG_CTRL_3;
    buf[1] = ctrl3;
    if (!SERCOM3_I2C_Write(WSEN_PADS_ADDR, buf, 2)) {
        return -1;
    };
    return 0;
}
/**
 * @brief Can be used to check successful connection with wsen-pads
 */
int wsen_check_device_id(void) {
    uint8_t reg = REG_DEVICE_ID;
    uint8_t device_id = 0;
    if (!SERCOM3_I2C_WriteRead(WSEN_PADS_ADDR, &reg, 1, &device_id, 1)) {
        return -1;
    };

    if (device_id == EXPECTED_DEVICE_ID) {
        return 0;
    } else {
        return -1;
    }
}

int read_pressure(float* pressure) {
    uint8_t status = 0;
    uint8_t rawData[3];
    int32_t rawPressure;

    uint8_t reg = REG_STATUS;
    // Wait until new pressure data available (P_DA bit = 1)
    do {
        SERCOM3_I2C_WriteRead(WSEN_PADS_ADDR, &reg, 1, &status, 1);
    } while (!(status & 0x01));

    // Read the 3 pressure registers (XL, L, H)
    reg = REG_DATA_P_XL;
    if (!SERCOM3_I2C_WriteRead(WSEN_PADS_ADDR, &reg, 1, &rawData, 3)) {
        return -1;
    };

    // Combine into signed 24-bit value
    rawPressure =
        (int32_t)((rawData[2] << 16) | (rawData[1] << 8) | rawData[0]);
    if (rawPressure & 0x00800000) {  // Sign extend negative numbers
        rawPressure |= 0xFF000000;
    }

    // Convert to kPa using sensitivity 1/40960 kPa/digit
    *pressure = (float)rawPressure / 40960.0f;

    return 0;
}

int read_temp(float* temp) {
    uint8_t status = 0;
    uint8_t rawData[2];
    int16_t rawTemp;

    // Wait until new temperature data available (T_DA bit = 1)
    uint8_t reg = REG_STATUS;
    do {
        SERCOM3_I2C_WriteRead(WSEN_PADS_ADDR, &reg, 1, &status, 1);
    } while (!(status & 0x02));

    reg = REG_DATA_T_L;
    // Read 2 temperature bytes: L, H
    if (!SERCOM3_I2C_WriteRead(WSEN_PADS_ADDR, &reg, 1, &rawData, 2)) {
        return -1;
    };

    // Combine into signed 16-bit value
    rawTemp = (int16_t)((rawData[1] << 8) | rawData[0]);

    // Convert to °C (0.01 °C per digit)
    *temp = (float)rawTemp * 0.01f;

    return 0;
}
