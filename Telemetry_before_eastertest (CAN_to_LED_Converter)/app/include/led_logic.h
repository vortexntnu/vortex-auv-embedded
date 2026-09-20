#pragma once
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef LED_LOGIC_NUM_SUBSYSTEMS
#define LED_LOGIC_NUM_SUBSYSTEMS  (11u)
#endif

#ifndef LED_LOGIC_MAX_FAULTS
#define LED_LOGIC_MAX_FAULTS      (LED_LOGIC_NUM_SUBSYSTEMS)
#endif

#ifndef LED_LOGIC_OFF_MS
#define LED_LOGIC_OFF_MS          (200u)
#endif

#ifndef LED_LOGIC_HOLD_MS
#define LED_LOGIC_HOLD_MS         (1800u)
#endif

typedef enum {
    LED_SEV_WARN  = 0,
    LED_SEV_FAULT = 1
} led_severity_t;

void led_logic_init(void);

// Native indicators
void led_logic_set_pressure_ok(bool ok);
void led_logic_set_orin_wifi(bool connected);
void led_logic_set_pi_wifi(bool connected);

// Fault/state control
void led_logic_set_subsystem(led_severity_t sev,
                             uint8_t subsystem_id_4bit,
                             bool has_detail,
                             uint8_t detail_mask_4bit);

void led_logic_clear_subsystem(uint8_t subsystem_id_4bit);
void led_logic_clear_detail(uint8_t subsystem_id_4bit);
void led_logic_clear_all(void);

bool led_logic_has_any_faults(void);

// Tick-driven display/sequence logic
void led_logic_tick(uint32_t now_ms);

#ifdef __cplusplus
}
#endif