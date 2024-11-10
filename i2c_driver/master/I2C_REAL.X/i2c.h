/*
 * File:   i2c.h
 * Author: nathaniel
 *
 * Created on November 2, 2024, 6:33 PM
 */

#ifndef I2C_H
#define I2C_H

// #include <cstdint>
#ifdef __cplusplus
extern "C" {
#endif

#include "same54p20a.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#define I2C_SERCOM SERCOM3_REGS
#define I2C_BAUDRATE 100000

#ifdef __cplusplus
}
#endif

enum {
    I2C_TRANSFER_WRITE = 0,
    I2C_TRANSFER_READ = 1,
};

void sercom3_i2c_configure_pins(void);

void debug_pins(void);
void sercom3_i2c_configure_master(void);
// sercom3_I2C start function
// void i2c_start(uint8_t address, uint8_t direction);

/**
 * \brief I2C write function
 *
 * @param address: "The address of the client device"
 * @param data: "Write data, one byte at the time"
 * @param wrLength: "Write data length "
 *
 */
bool sercom3_i2c_write(uint8_t address, uint8_t* data, uint32_t wrLength);
/**
 * \brief I2C read function
 *
 * @param address: "The address of the client device"
 * @param data: "Read data, one byte at the time"
 * @oaram rdLength: "Read data length "
 *
 */
bool sercom3_i2c_read(uint8_t address, uint8_t* data, uint32_t rdLength);
/**
 * \brief I2C read and write function
 *
 * @param address: "The address of the client device"
 * @param wrData: "Write data, one byte at the time"
 * @param wrLength: "Write data length "
 * @param rdData: "Read data, one byte at the time"
 * @param rdLength: "Read data length "
 *
 */
bool sercom3_i2c_write_read(uint16_t address,
                            uint8_t* wrData,
                            uint32_t wrLength,
                            uint8_t* rdData,
                            uint32_t rdLength);

/**
 *\brief I2C ping function
 *
 * @param address: "I2C client address"
 */
bool sercom3_i2c_ping(uint8_t address);
/**
 *\brief I2C scanner, checks all I2C addreses
 *
 */
void sercom3_i2c_scanner();

typedef enum {
    /* SERCOM PLib Task Error State */
    SERCOM_I2C_STATE_ERROR = -1,

    /* SERCOM PLib Task Idle State */
    SERCOM_I2C_STATE_IDLE,

    /* SERCOM PLib Task Address Send State */
    SERCOM_I2C_STATE_ADDR_SEND,

    SERCOM_I2C_REINITIATE_TRANSFER,
    /* SERCOM PLib Task Read Transfer State */
    SERCOM_I2C_STATE_TRANSFER_READ,

    /* SERCOM PLib Task Write Transfer State */
    SERCOM_I2C_STATE_TRANSFER_WRITE,

    /* SERCOM PLib Task High Speed Slave Address Send State */
    SERCOM_I2C_STATE_TRANSFER_ADDR_HS,

    /* SERCOM PLib Task Transfer Done State */
    SERCOM_I2C_STATE_TRANSFER_DONE,

} SERCOM_I2C_STATE;

typedef enum {
    /* No error has occurred. */
    SERCOM_I2C_ERROR_NONE,

    /* A bus transaction was NAK'ed */
    SERCOM_I2C_ERROR_NAK,

    /* A bus error has occurred. */
    SERCOM_I2C_ERROR_BUS,

} SERCOM_I2C_ERROR;

typedef void (*SERCOM_I2C_CALLBACK)(
    /*Transfer context*/
    uintptr_t contextHandle

);
typedef struct {
    bool isHighSpeed;

    bool txMasterCode;

    bool transferDir;

    uint16_t address;

    uint8_t masterCode;

    uint8_t* writeBuffer;

    uint8_t* readBuffer;

    size_t writeSize;

    size_t readSize;

    size_t writeCount;

    size_t readCount;

    /* State */
    SERCOM_I2C_STATE state;

    /* Transfer status */
    SERCOM_I2C_ERROR error;

    /* Transfer Event Callback */
    SERCOM_I2C_CALLBACK callback;

    uintptr_t context;
} SERCOM_I2C_OBJ;

SERCOM_I2C_ERROR sercom3_i2c_error_get(void);

void SERCOM3_I2C_CallbackRegister(SERCOM_I2C_CALLBACK callback,
                                  uintptr_t contextHandle);

#endif /* I2C_H */
