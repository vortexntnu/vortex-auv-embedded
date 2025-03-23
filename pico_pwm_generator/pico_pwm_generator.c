#define PULLUP_ENABLE
#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include <pico/i2c_slave.h>
#include <string.h>
#include "hardware/pwm.h"
#include "hardware/pll.h"
#include "hardware/clocks.h"
#include "hardware/structs/pll.h"
#include "hardware/structs/clocks.h"
// Removed fixmath.h dependency
#include "hardware/irq.h"
#include "pico/time.h"

// I2C defines
#define     I2C_SDA     16
#define     I2C_SCL     17
#define     I2C_PORT    i2c0
#define     I2C_ADDRESS 0x21

// PWM defines
#define PWM_FREQ    50
#define ESC1        18
#define ESC2        19
#define ESC3        20
#define ESC4        21
#define ESC5        22
#define ESC6        26
#define ESC7        27
#define ESC8        28

float clkdiv;
uint min;
uint max;

int slice_map[30];
uint slice_active[8];  // Renamed from "slice_actice" to "slice_active"
uint esc_pos[32];
uint esc_pos_buf[16];
pwm_config slice_cfg[8];

uint min_us = 500;
uint arm_us = 1500;
uint max_us = 2500;
float us_per_unit = 0.f;  // Changed type from fix16_t to float

bool LED_STATE = false;

struct i2c_data {
    uint8_t     mem[256];
    uint8_t     mem_address;
    bool        mem_address_written;
    uint8_t     i2c_state;
} i2c_data;

// I2C functions declarations
void i2c_slave_handler(i2c_inst_t *i2c, i2c_slave_event_t event);
void i2c_setup();
void led_sequence();

// PWM functions declarations
void wrap_cb(void);
void esc_set_bounds(uint a, uint b);
int esc_init(void);
int esc_clock_auto(void);
int esc_clock_source(uint src); // Corrected function name here
int esc_clock_manual(uint freq);
int esc_attach(uint pin);
int esc_set_speed(uint pin, uint angle);
int esc_us(uint pin, uint us);

int main()
{
    i2c_data.i2c_state = 0;
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);

    led_sequence();

    stdio_init_all();
    i2c_setup();

    while (true) {
        led_sequence();
    }
}

void i2c_slave_handler(i2c_inst_t *i2c, i2c_slave_event_t event) {
    switch (event) {
    case I2C_SLAVE_RECEIVE: // master has written some data
        if (!i2c_data.mem_address_written) {
            // writes always start with the memory address
            i2c_data.mem_address = i2c_read_byte_raw(i2c);
            i2c_data.mem_address_written = true;
        } else {
            // save into memory
            i2c_data.mem[i2c_data.mem_address] = i2c_read_byte_raw(i2c);
            i2c_data.mem_address++;
        }
        break;
    case I2C_SLAVE_REQUEST: // master is requesting data
        // load from memory
        i2c_write_byte_raw(i2c, i2c_data.mem[i2c_data.mem_address]);
        i2c_data.mem_address++;
        break;
    case I2C_SLAVE_FINISH: // master has signalled Stop / Restart
        i2c_data.mem_address_written = false;
        break;
    default:
        break;
    }
}

void i2c_setup() {
    gpio_init(I2C_SDA);
    gpio_init(I2C_SCL);

    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);

    #ifdef PULLUP_ENABLE
        gpio_pull_up(I2C_SDA);
        gpio_pull_up(I2C_SCL);
        printf("I2C internal pullup enabled. If you want to use external pull ups, you need to comment out line number 1!\r\n");
    #else
        printf("I2C internal pullup disabled. if you want to use them, uncomment line number 1!\r\n");
    #endif // PULLUP_ENABLE

    i2c_slave_init(I2C_PORT, I2C_ADDRESS, &i2c_slave_handler);
}

void led_sequence() {
    switch(i2c_data.i2c_state) {
        // Special case for startup, does not really make sense to use i2c_state
        // But it makes sense to use the same function for all sequences xd
        case 0: {
            for(uint8_t i = 0; i < 20; i++) {
                gpio_put(PICO_DEFAULT_LED_PIN, LED_STATE);
                LED_STATE = !LED_STATE;
                sleep_ms(50);
            }
            i2c_data.i2c_state = 1;
            break;
        }

        case 1: {
            gpio_put(PICO_DEFAULT_LED_PIN, LED_STATE);
            LED_STATE = !LED_STATE;
            sleep_ms(100);
            break;
        }

        case 2: {
            for(uint8_t i = 0; i < 5; i++) {
                LED_STATE = !LED_STATE;
                gpio_put(PICO_DEFAULT_LED_PIN, LED_STATE);
                sleep_ms(5);
            }
            i2c_data.i2c_state = 1;
            break;
        }

        default: {
            i2c_data.i2c_state = 1;
            break;
        }
    }
}

void wrap_cb(void) {
    uint offset;
    for(uint8_t i = 0; i < 8; i++) {
        if(slice_active[i] == 0) {
            continue;
        }
        pwm_clear_irq(i);
        offset = 16 * (esc_pos_buf[i] % 2);
        pwm_set_chan_level(i, 0, esc_pos[offset + 1]);
    }
}

void esc_set_bounds(uint a, uint b) {
    min_us = a;
    max_us = b;

    if(us_per_unit > 0.f) {
        min = (uint)(min_us / us_per_unit);
        max = (uint)(max_us / us_per_unit);
    }
}

int esc_init(void) {
    for(uint8_t i = 0; i < 30; i++) {
        slice_map[i] = -1;  // Fixed typo: "slic_map" -> "slice_map"
    }
    memset(slice_active, 0, 8 * sizeof(uint));
    memset(esc_pos, 0, 32 * sizeof(uint));
    memset(esc_pos_buf, 0, 16 * sizeof(uint));

    irq_add_shared_handler(PWM_IRQ_WRAP, wrap_cb, PICO_SHARED_IRQ_HANDLER_DEFAULT_ORDER_PRIORITY);
    return 0;
}

int esc_clock_auto(void) {
    return esc_clock_source(CLOCKS_FC0_SRC_VALUE_PLL_SYS_CLKSRC_PRIMARY);
}

int esc_clock_source(uint src) {
    clkdiv = (float)frequency_count_khz(src) * 1e3f / (PWM_FREQ * 1e4f);
    if(clkdiv == 0) {
        return -1;
    }
    // Calculate us_per_unit using floating point arithmetic.
    us_per_unit = 1.f / (PWM_FREQ * 1e4f) / 1e-6f;
    min = (uint)(min_us / us_per_unit);
    max = (uint)(max_us / us_per_unit);
    return 0;
}

int esc_clock_manual(uint freq) {
    clkdiv = (float)(freq * 1e3) / (float)(PWM_FREQ * 1e4);
    if(clkdiv == 0) {
        return -1;
    }
    min = (uint)(0.025f * 1e4f);
    max = (uint)(0.125f * 1e4f);
    return 0;
}

int esc_attach(uint pin) {
    uint slice = pwm_gpio_to_slice_num(pin);
    if(slice_active[slice] >= 2) {
        return 1;
    }
    gpio_set_function(pin, GPIO_FUNC_PWM);
    slice_map[pin] = slice;
    if(slice_active[slice] == 0) {
        pwm_clear_irq(slice);
        pwm_set_irq_enabled(slice, true);
        slice_cfg[slice] = pwm_get_default_config();
        pwm_config_set_wrap(&slice_cfg[slice], 1e4);
        pwm_config_set_clkdiv(&slice_cfg[slice], clkdiv);
        pwm_init(slice, &slice_cfg[slice], true);
        pwm_set_chan_level(slice, pin % 2, 90);
    }
    ++slice_active[slice];
    irq_set_enabled(PWM_IRQ_WRAP, true);
    return 0;
}

int esc_set_speed(uint pin, uint angle) {
    if(slice_map[pin] < 0) {
        return 1;
    }
    // Replace fixmath arithmetic with floating point arithmetic:
    uint val = (uint)(((float)angle / 180.0f) * (max - min)) + min;
    uint pos = slice_map[pin] + (pin % 2);
    esc_pos[16 * esc_pos_buf[pos] + pos] = val;
    esc_pos_buf[pos] = (esc_pos_buf[pos] + 1) % 2;
    return 0;
}

int esc_us(uint pin, uint us) {
    if(slice_map[pin] < 0) {
        return 1;
    }
    uint pos = slice_map[pin] + (pin % 2);
    esc_pos[16 * esc_pos_buf[pos] + pos] = us;
    esc_pos_buf[pos] = (esc_pos_buf[pos] + 1) % 2;
    return 0;
}
