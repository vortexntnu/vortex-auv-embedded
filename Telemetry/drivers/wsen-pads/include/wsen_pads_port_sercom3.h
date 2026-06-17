#ifndef WSEN_PADS_PORT_SERCOM3_H
#define WSEN_PADS_PORT_SERCOM3_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>
#include "definitions.h"

int wsen_init(void);
int wsen_check_device_id(void);
void spi_init(void);
void drdy_init(void);
void wsen_cycle_start(void);
void wsen_cycle_tick(void);
bool wsen_cycle_done_ok(float* kPa, float* degC);
bool wsen_cycle_failed(void);
void wsen_reset(void);

#ifdef __cplusplus
}
#endif

#endif