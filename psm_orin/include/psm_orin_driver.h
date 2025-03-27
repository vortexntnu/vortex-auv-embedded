
#ifndef PSM_ORIN_DRIVER_H
#define PSM_ORIN_DRIVER_H
#endif // !PSM_ORIN_DRIVER_H

#include <errno.h>
#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <unistd.h>

#ifdef __cplusplus
extern "C" {
#endif  //

int i2c_psm_init();
int i2c_write_register(uint8_t reg, uint16_t value);
uint16_t i2c_read_register(uint8_t reg);
void config_ads();
void set_mux_diff(int pair);
int16_t read_adc();
double read_voltage();
void read_scaled_measurements(double* voltage, double* current);

#ifdef __cplusplus
}
#endif  // __cplusplus
