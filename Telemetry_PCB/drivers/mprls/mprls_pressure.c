#include "mprls_pressure.h"
#include "peripheral/sercom/i2c_master/plib_sercom3_i2c_master.h"
#include <stdint.h>
#include <unistd.h>

int start_measurement() {
    uint8_t write_data[3] = {MPRLS_REG, 0, 0};
    if (!SERCOM3_I2C_Write(MPRLS_ADDRESS, write_data, 3)) {
        return -1;
    }
    return 0;
}

int read_pressure(double* pressure) {
    uint8_t i2c_data[4] = {0};

    if (!SERCOM3_I2C_Read(MPRLS_ADDRESS, i2c_data, 4)) {
        return -1;
    }
    while (SERCOM3_I2C_IsBusy()){

    }
    printf("%d, %d, %d, %d\n", i2c_data[0], i2c_data[1], i2c_data[2], i2c_data[3]);

    /* if (i2c_write_read(write_data, 3, i2c_data, 4, MPRLS_ADDRESS)) {
        return -1;
    } */
    // uint8_t status = i2c_data[0];
    int32_t pressure_counts =
        (int32_t)((i2c_data[1] << 16) | (i2c_data[2] << 8) | i2c_data[3]);
    double scale =
        (PRESSURE_MAX - PRESSURE_MIN) / (double)(COUNTS_MAX - COUNTS_MIN);
    *pressure = (pressure_counts - COUNTS_MIN) * scale + PRESSURE_MIN;

    return 0;
}