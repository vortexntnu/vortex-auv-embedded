#ifndef BMP280_SERVICE_H
#define BMP280_SERVICE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int8_t bmp280_init_device(void);
int8_t bmp280_read_sample(float *temperature, float *pressure);

#ifdef __cplusplus
}
#endif

#endif