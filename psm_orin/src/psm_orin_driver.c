#include "../include/psm_orin_driver.h"
#include <stdint.h>
#include <unistd.h>

static int i2c_fd;

#define VOLTAGE_SCALE 11.236
#define VOLTAGE_RANGE 6.144
#define CURRENT_OFFSET 0.595
#define CURRENT_SENSITIVITY 0.0255

int i2c_psm_init(){
    char *i2c_device = "/dev/i2c-7";
    if((i2c_fd = open(i2c_device, O_RDWR)) < 0) {
        perror("Failed to open I2C device!");
        return 1;
    }
    if (ioctl(i2c_fd, I2C_SLAVE, PSM_ADDRESS) < 0) {
        perror("Failed to select I2C device");
        return 1;
    }
    return 0;
}


static int i2c_write_register(uint8_t reg, uint16_t value) {
    uint8_t data[3];
    data[0] = reg;
    data[1] = (value >> 8) & 0xFF;
    data[2] = value & 0xFF;

    if(write(i2c_fd, data, 3) != 3) {
        perror("\r\nFailed to write to I2C register on ADS1115\r\n!");
        return -1;
    }
    return 0;
}

static uint16_t i2c_read_register(uint8_t reg) {
    uint8_t buf[2];
    if(write(i2c_fd, &reg, 1) != 1) {
        perror("\r\nFailed to write register address\r\n"), printf("%x\r\n", reg);
        return -1;
    }
    if(read(i2c_fd, buf, 2) != 2) {
        perror("Failed to read from register"), printf("%x\r\n", reg);
        return -1;
    }
    return(buf[0] << 8) | buf[1];
}

void config_ads() {
    uint16_t config = CFG_OS_SINGLE | CFG_MUX_DIFF_0_1 | CFG_PGA_6_144V
                    | CFG_MODE_SINGLE | CFG_DR_128SPS | CFG_COMP_MODE
                    | CFG_COMP_POL | CFG_COMP_LAT | CFG_COMP_QUE_DIS;
    i2c_write_register(REG_CFG, config);
}

static void start_conversion(uint16_t config) {
    config |= CFG_OS_SINGLE;
    i2c_write_register(REG_CFG, config);
}

static uint16_t read_conversion() {
    usleep(10000);
    return i2c_read_register(REG_CONV);
}

void read_measurements(double *voltage, double *current) {
    uint16_t config;

    config = i2c_read_register(REG_CFG);
    config &= ~0x7000;         
    config |= CFG_MUX_DIFF_0_1; 
    start_conversion(config);  
    int16_t raw_voltage = (int16_t) read_conversion();
    *voltage = ((raw_voltage * VOLTAGE_RANGE) / 32768.0) * VOLTAGE_SCALE;

    config = i2c_read_register(REG_CFG);
    config &= ~0x7000;        
    config |= CFG_MUX_DIFF_2_3;
    start_conversion(config); 
    int16_t raw_current = (int16_t) read_conversion();
    *current = (CURRENT_OFFSET - ((raw_current * VOLTAGE_RANGE) / 32768.0)) / CURRENT_SENSITIVITY;
}

