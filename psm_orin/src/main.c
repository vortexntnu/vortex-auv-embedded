#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stddef.h> 
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <stdlib.h>
#include <errno.h> 

#define PSM_ADDRESS 0x48
#define REG_CONV    0x00
#define REG_CFG     0x01

#define CFG_OS_SINGLE       0x8000
#define CFG_MUX_DIFF_0_1    0x0000
#define CFG_MUX_DIFF_2_3    0x3000
#define CFG_PGA_6_144V      0x0000
#define CFG_MODE_SINGLE     0x0100
#define CFG_DR_128SPS       0x0080
#define CFG_COMP_MODE       0x0010
#define CFG_COMP_POL        0x0008
#define CFG_COMP_LAT        0x0004
#define CFG_COMP_QUE_DIS    0x0003

int i2c_fd;

int i2c_write_register(uint8_t reg, uint16_t value);
uint16_t i2c_read_register(uint8_t reg);
void config_ads();
void set_mux_diff(int pair);
int16_t read_adc();
double read_voltage();
void read_scaled_measurements(double *voltage, double *current);


int main(int argc , char** argv) {
    char *i2c_device = "/dev/i2c-7";
    if((i2c_fd = open(i2c_device, O_RDWR)) < 0) {
        perror("Failed to open I2C device!");
        return 1;
    }
    if (ioctl(i2c_fd, I2C_SLAVE, PSM_ADDRESS) < 0) {
        perror("Failed to select I2C device");
        return 1;
    }
    
    printf("ADS1115 Differential Measurements\n");
    while (1) {
        double voltage, current;
        read_scaled_measurements(&voltage, &current);
        printf("Voltage: %.2fV\n", voltage);
        printf("Current: %.2fA\n", current);
        sleep(1);
    }
    close(i2c_fd);
    return 0;
}

int i2c_write_register(uint8_t reg, uint16_t value) {
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

uint16_t i2c_read_register(uint8_t reg) {
    uint8_t buf[2];
    if(write(i2c_fd, &reg, 1) != 1) {
        perror("\r\nFailed to write register address %x\r\n", reg);
        return -1;
    }
    if(read(i2c_fd, buf, 2) != 2) {
        perror("Failed to read from register %x", reg);
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

void set_mux_diff(int pair) {
    uint16_t config = i2c_read_register(REG_CFG);
    config &= ~0x7000;
    if(pair == 0) {
        config |= CFG_MUX_DIFF_0_1;
    } else {
        config |= CFG_MUX_DIFF_2_3;
    }
    i2c_write_register(REG_CFG, config);
}

int16_t read_adc() {
    config_ads();
    usleep(100000);
    uint16_t result = i2c_read_register(REG_CONV);
    return (result > 32767) ? result - 65536 : result;
}

double read_voltage() {
    int16_t raw = read_adc();
    return(raw * 6.144) / 32768.0;
}

void read_scaled_measurements(double *voltage, double *current) {
    set_mux_diff(0);
    *voltage = read_voltage() * 11.236;
    set_mux_diff(1);
    *current = (0.595 - read_voltage()) / 0.0255;
 }

