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

typedef enum {
    LED_SW_KILLSWITCH = 0x00,
    LED_SW_AUTONOMOUS = 0x01,
    LED_SW_MANUAL     = 0x02,
    LED_SW_REFERENCE  = 0x03
} led_software_mode_t;

/*
 * Startup faults.
 * These are shown in the rotating binary alarm display.
 *
 * Software mode is NOT here anymore.
 * It is shown as a constant indicator on physical LED index 5.
 */
static const uint8_t startup_faults[] = {
    1, // PI
    2, // ORIN
    3, // MCU_POWER
    //4, // KILLSWITCH
    //5, // Pressure
    8  // THRUSTERS
};

void led_logic_init(void);

// Native indicators
void led_logic_set_pressure_ok(bool ok);
void led_logic_set_orin_wifi(bool connected);
void led_logic_set_pi_wifi(bool connected);
void led_logic_set_software_mode(led_software_mode_t mode);

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