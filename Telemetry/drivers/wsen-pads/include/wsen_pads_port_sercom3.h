#ifndef WSEN_PADS_PORT_SERCOM3_H
#define WSEN_PADS_PORT_SERCOM3_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>
#include "plib_sercom_i2c_master_common.h"

int wsen_init(void);
int wsen_check_device_id(void);
void drdy_init(void);
void wsenCycleStart(void);
void wsenCycleTick(void);
bool wsenCycleDoneOk(float* kPa, float* degC);
bool wsenCycleFailed(SERCOM_I2C_ERROR* errOut);
void wsenReset(void);

#ifdef __cplusplus
}
#endif

#endif