

/*
 * File:   i2c_master.h
 * Author: nathaniel
 *
 * Created on January 19, 2025, 4:21 PM
 */

#ifndef SERCOM1_I2C_H
#define SERCOM1_I2C_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "sercom_i2c_common.h"

#ifdef __cplusplus
extern "C" {
#endif

void SERCOM1_I2C_Initialize(void);

bool SERCOM1_I2C_Read(uint16_t address, uint8_t* pdata, uint32_t length);

bool SERCOM1_I2C_Write(uint16_t address, uint8_t* pdata, uint32_t length);

bool SERCOM1_I2C_WriteRead(uint16_t address,
                           uint8_t* wdata,
                           uint32_t wlength,
                           uint8_t* rdata,
                           uint32_t rlength);

bool SERCOM1_I2C_IsBusy(void);

SERCOM_I2C_ERROR SERCOM1_I2C_ErrorGet(void);

void SERCOM1_I2C_CallbackRegister(SERCOM_I2C_CALLBACK callback,
                                  uintptr_t contextHandle);

bool SERCOM1_I2C_TransferSetup(SERCOM_I2C_TRANSFER_SETUP* setup,
                               uint32_t srcClkFreq);

#ifdef __cplusplus
}
#endif

#endif /* SERCOM1_I2C_H */
