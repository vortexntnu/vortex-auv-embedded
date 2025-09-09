#ifndef I2C_H
#define I2C_H
#include "common.h"

// I2C configuration defines
// Check if pullups should be enabled or not
#if !defined(PULLUP_ENABLE) && !defined(PULLUP_DISABLE)
    #define PULLUP_ENABLE
    WARNING("\r\n\t\tUsing internal pullup resistors on the SDA and SCL pins.")
#endif // PULLUP

#if !defined(I2C_ADDRESS) && \
    !defined(I2C_ADDR) && \
    !defined(I2C_SLAVE_ADDRESS) && \
    !defined(I2C_SLAVE_ADDR) && \
    !defined(I2C_PICO_ADDRESS) && \
    !defined(I2C_PICO_ADDR) && \
    !defined(PICO_ADDRESS) && \
    !defined(PICO_ADDR)
    #define I2C_ADDRESS 0x21
    WARNING("\r\n\t\tNo valid user defined I2C address detected. Defaulting to I2C address 0x21") 
#endif // Define I2C Address

#if (defined(I2C_SDA) || defined(I2C_SCL)) && !defined(I2C_PORT)
    WARNING("\r\n\t\tDetected User defined I2C pins but no user defined I2C port. The I2C port will default to i2c0. Make sure that this is a compatible port or define you own port.")
#endif // I2C Port Warning

#if !defined(I2C_SDA)
    #define I2C_SDA 16
    WARNING("\r\n\t\tNo valid user defined I2C SDA pin detected. Defaulting to GPIO 16")
#endif // I2C_SDA PIN DEFINE

#if !defined(I2C_SCL)
    #define I2C_SCL 17
    WARNING("\r\n\t\tNo valid user defined I2C SCL pin detected. Defaulting to GPIO 17")
#endif // I2C_SCL PIN DEFINE

#if !defined(I2C_PORT)
    #define I2C_PORT    i2c0
    WARNING("\r\n\t\tNo valid user defined I2C port detected. Defaulting to i2c0")
#endif // I2C_PORT DEFINE
#define I2C_MSG_LENGTH  17

#include <stdint.h>
#include "hardware/i2c.h"
#include "pico/i2c_slave.h"
#include "pico/stdlib.h"

#ifdef __cplusplus
extern "C" {
#endif

// Structure for I2C shared data
typedef struct {
    uint8_t mem[256];
    uint8_t mem_address;   // used as a counter here
    uint8_t i2c_state;
} i2c_data_t;

// Declare the shared I2C data as external so it can be used by other modules.
extern volatile i2c_data_t i2c_data;

// Function prototypes for I2C functions
void i2c_slave_handler(i2c_inst_t *i2c, i2c_slave_event_t event);
void i2c_setup(void);

#ifdef __cplusplus
}
#endif

#endif // I2C_H
