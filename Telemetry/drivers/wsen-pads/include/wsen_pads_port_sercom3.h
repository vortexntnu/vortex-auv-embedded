#ifndef WSEN_PADS_PORT_SERCOM3_H
#define WSEN_PADS_PORT_SERCOM3_H

#ifdef __cplusplus
extern "C" {
#endif

void wsen_init(void);
int wsen_check_device_id(void);
int read_pressure(float* pressure);
int read_temp(float* temp);

#ifdef __cplusplus
}
#endif

#endif