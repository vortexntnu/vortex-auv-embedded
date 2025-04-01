#include "../include/psm_orin_driver.h"
#include <stdint.h>
#include <unistd.h>

static int i2c_fd;

static int i2c_write_register(uint8_t reg, uint16_t value) {
    uint8_t data[3];
    data[0] = reg;
    data[1] = (value >> 8) & 0xFF;
    data[2] = value & 0xFF;

    if (write(i2c_fd, data, 3) != 3) {
        perror("\r\nFailed to write to I2C register on ADS1115\r\n!");
        return -1;
    }
    return 0;
}

static int i2c_read_register(uint8_t reg, uint16_t* data) {
    uint8_t buf[2];
    if (write(i2c_fd, &reg, 1) != 1) {
        perror("\r\nFailed to write register address\r\n"),
            printf("%x\r\n", reg);
        return -1;
    }
    if (read(i2c_fd, buf, 2) != 2) {
        perror("Failed to read from register"), printf("%x\r\n", reg);
        return -1;
    }
    *data = (buf[0] << 8) | buf[1];
    return 0;
}

static inline uint8_t start_conversion(uint16_t config) {
    config |= CFG_OS_SINGLE;
    if (i2c_write_register(REG_CFG, config)) {
        return 1;
    }
    return 0;
}

int i2c_psm_init() {
    char* i2c_device = "/dev/i2c-7";
    if ((i2c_fd = open(i2c_device, O_RDWR)) < 0) {
        perror("Failed to open I2C device!");
        return -1;
    }
    if (ioctl(i2c_fd, I2C_SLAVE, PSM_ADDRESS) < 0) {
        perror("Failed to select I2C device");
        return -1;
    }
    return 0;
}

int read_measurements(double* voltage, double* current) {
    static uint16_t default_config =
        CFG_OS_SINGLE | CFG_MUX_DIFF_0_1 | CFG_PGA_6_144V | CFG_MODE_SINGLE |
        CFG_DR_128SPS | CFG_COMP_MODE | CFG_COMP_POL | CFG_COMP_LAT |
        CFG_COMP_QUE_DIS;

    uint16_t config = default_config;
    config &= ~0x7000;
    config |= CFG_MUX_DIFF_0_1;
    if (start_conversion(config))
        return -1;
    usleep(10000);
    int16_t raw_voltage;
    if (i2c_read_register(REG_CONV, (uint16_t*)&raw_voltage))
        return -1;
    // Aproximately 0.8 V lost over diode
    *voltage = ((raw_voltage * VOLTAGE_RANGE) / 32768.0) * VOLTAGE_SCALE + 0.8;

    config = default_config;
    config &= ~0x7000;
    config |= CFG_MUX_DIFF_2_3;
    if (start_conversion(config))
        return -1;
    usleep(10000);
    int16_t raw_current;
    if (i2c_read_register(REG_CONV, (uint16_t*)&raw_current))
        return -1;
    *current = (CURRENT_OFFSET - ((raw_current * VOLTAGE_RANGE) / 32768.0)) /
               CURRENT_SENSITIVITY;
    return 0;
}
