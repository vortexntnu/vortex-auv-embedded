#ifndef I2C_H
#define I2C_H

// Check if pullups should be enabled or not
#if !defined(PULLUP_ENABLE) && !defined(PULLUP_DISABLE)
    #define PULLUP_ENABLE
#endif // PULLUP

#include <stdint.h>
#include "hardware/i2c.h"
#include "pico/i2c_slave.h"
#include "pico/stdlib.h"

// I2C configuration defines
#define I2C_SDA         16
#define I2C_SCL         17
#define I2C_PORT        i2c0
#define I2C_ADDRESS     0x21
#define I2C_MSG_LENGTH  17

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
