#include "i2c.h"
#include "pwm.h" 
#include "pico/stdlib.h"
#include <string.h>

// Define the shared I2C data
volatile i2c_data_t i2c_data = { .mem = {0}, .mem_address = 0, .i2c_state = 0 };

void i2c_slave_handler(i2c_inst_t *i2c, i2c_slave_event_t event) {
    switch (event) {
        case I2C_SLAVE_RECEIVE: {
            // Read one byte and store it in the buffer.
            uint8_t byte = i2c_read_byte_raw(i2c);
            if (i2c_data.mem_address < sizeof(i2c_data.mem))
                i2c_data.mem[i2c_data.mem_address++] = byte;
            // Indicate that a new I2C message is being received.
            i2c_data.i2c_state = 2;
            break;
        }
        case I2C_SLAVE_REQUEST: {
            // When a master requests a read, send a default response.
            uint8_t response = (i2c_data.mem_address > 0) ? i2c_data.mem[0] : 0;
            i2c_write_byte_raw(i2c, response);
            break;
        }
        case I2C_SLAVE_FINISH: {
            // If the expected message length is received, process it.
            if(i2c_data.mem_address == I2C_MSG_LENGTH) {
                // Discard the dummy byte and process the following 16 bytes as eight 16-bit values.
                for (int i = 0; i < 8; i++) {
                    uint16_t pwm_val = ((uint16_t)i2c_data.mem[1 + i * 2] << 8) |
                                        i2c_data.mem[1 + i * 2 + 1];
                    switch(i) {
                        case 0: esc_us(ESC1, pwm_val); break;
                        case 1: esc_us(ESC2, pwm_val); break;
                        case 2: esc_us(ESC3, pwm_val); break;
                        case 3: esc_us(ESC4, pwm_val); break;
                        case 4: esc_us(ESC5, pwm_val); break;
                        case 5: esc_us(ESC6, pwm_val); break;
                        case 6: esc_us(ESC7, pwm_val); break;
                        case 7: esc_us(ESC8, pwm_val); break;
                    }
                }
            }
            // Reset the message buffer and state.
            i2c_data.mem_address = 0;
            i2c_data.i2c_state = 1;
            break;
        }
        default: {
            i2c_data.i2c_state = 1;
            break;
        }
    }
}

void i2c_setup(void) {
    // Initialize I2C interface at 100kHz. Needed for some reason even for slaves...
    //i2c_init(I2C_PORT, 100 * 1000);
    gpio_init(I2C_SDA);
    gpio_init(I2C_SCL);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);

    #ifdef PULLUP_ENABLE
        gpio_pull_up(I2C_SDA);
        gpio_pull_up(I2C_SCL); 
    #endif // PULLUP Check

    // Initialize as I2C slave with the given address and handler.
    i2c_slave_init(I2C_PORT, I2C_ADDRESS, &i2c_slave_handler);
}
