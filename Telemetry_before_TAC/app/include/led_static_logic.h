#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    LED_STATIC_SW_KILLSWITCH = 0x00,
    LED_STATIC_SW_AUTONOMOUS = 0x01,
    LED_STATIC_SW_MANUAL     = 0x02,
    LED_STATIC_SW_REFERENCE  = 0x03
} led_static_software_mode_t;

void led_static_logic_init(void);

void led_static_set_pi_wifi(bool connected);
void led_static_set_orin_wifi(bool connected);

void led_static_set_temperature_ok(bool ok);
void led_static_set_pressure_ok(bool ok);

void led_static_set_software_mode(led_static_software_mode_t mode);

#ifdef __cplusplus
}
#endif