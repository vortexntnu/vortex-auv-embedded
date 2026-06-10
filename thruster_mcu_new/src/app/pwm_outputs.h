#ifndef PWM_OUTPUTS_H
#define PWM_OUTPUTS_H

#include <stdint.h>
#include <stdbool.h>
#include "definitions.h"

#ifdef __cplusplus
extern "C" {
#endif


#define PWM_MAX_STEP_US  25U

enum operating_mode {
    PWM_TCC,
    MPWM_TC,
};

struct pwm_output {
    enum operating_mode mode;
    uint8_t  instance;
    uint8_t  channel;
    uint32_t period_ticks;
    uint16_t min_us;
    uint16_t max_us;
    uint16_t neutral_us;
    uint32_t frame_us;
    uint16_t current_pulse_us;
    uint16_t target_pulse_us;
};

/* --- Constants --- */
static const uint32_t TCC0_PERIOD               = 38275;
static const uint32_t TCC1_PERIOD               = 38250;
static const uint32_t TCC2_PERIOD               = 38250;
static const uint32_t TC3_PERIOD                = 38250; 
static const uint32_t THRUSTER_PWM_PERIOD_US    = 20000U; // 50Hz
static const uint32_t LIGHT_PWM_PERIOD_US       = 20000U; // 50Hz

void pwm_thrusters_neutral(void);
void pwm_lights_off(void);
void pwm_thrusters_set_from_payload(const uint8_t *data);
void pwm_light_set_from_payload(const uint8_t *data);
void pwm_slew_outputs(void);

#ifdef __cplusplus
}
#endif

#endif
