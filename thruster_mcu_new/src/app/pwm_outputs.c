#include "pwm_outputs.h"
#include <stdint.h>

static struct pwm_output thrusters[8] = {
    {
        .mode = PWM_TCC,
        .instance = 2,
        .channel = 0,
        .period_ticks = TCC2_PERIOD,
        .min_us = 1000,
        .max_us = 2000,
        .neutral_us = 1500,
        .frame_us = THRUSTER_PWM_PERIOD_US,
        .current_pulse_us = 1500,
        .target_pulse_us = 1500,
    },
    {
        .mode = PWM_TCC,
        .instance = 2,
        .channel = 1,
        .period_ticks = TCC2_PERIOD,
        .min_us = 1000,
        .max_us = 2000,
        .neutral_us = 1500,
        .frame_us = THRUSTER_PWM_PERIOD_US,
        .current_pulse_us = 1500,
        .target_pulse_us = 1500,
    },
    {
        .mode = PWM_TCC,
        .instance = 1,
        .channel = 0,
        .period_ticks = TCC1_PERIOD,
        .min_us = 1000,
        .max_us = 2000,
        .neutral_us = 1500,
        .frame_us = THRUSTER_PWM_PERIOD_US,
        .current_pulse_us = 1500,
        .target_pulse_us = 1500,
    },
    {
        .mode = PWM_TCC,
        .instance = 1,
        .channel = 1,
        .period_ticks = TCC1_PERIOD,
        .min_us = 1000,
        .max_us = 2000,
        .neutral_us = 1500,
        .frame_us = THRUSTER_PWM_PERIOD_US,
        .current_pulse_us = 1500,
        .target_pulse_us = 1500,
    },
    {
        .mode = PWM_TCC,
        .instance = 0,
        .channel = 1,
        .period_ticks = TCC0_PERIOD,
        .min_us = 1000,
        .max_us = 2000,
        .neutral_us = 1500,
        .frame_us = THRUSTER_PWM_PERIOD_US,
        .current_pulse_us = 1500,
        .target_pulse_us = 1500,
    },
    {
        .mode = PWM_TCC,
        .instance = 0,
        .channel = 0,
        .period_ticks = TCC0_PERIOD,
        .min_us = 1000,
        .max_us = 2000,
        .neutral_us = 1500,
        .frame_us = THRUSTER_PWM_PERIOD_US,
        .current_pulse_us = 1500,
        .target_pulse_us = 1500,
    },
    {
        .mode = PWM_TCC,
        .instance = 0,
        .channel = 3,
        .period_ticks = TCC0_PERIOD,
        .min_us = 1000,
        .max_us = 2000,
        .neutral_us = 1500,
        .frame_us = THRUSTER_PWM_PERIOD_US,
        .current_pulse_us = 1500,
        .target_pulse_us = 1500,
    },
    {
        .mode = PWM_TCC,
        .instance = 0,
        .channel = 2,
        .period_ticks = TCC0_PERIOD,
        .min_us = 1000,
        .max_us = 2000,
        .neutral_us = 1500,
        .frame_us = THRUSTER_PWM_PERIOD_US,
        .current_pulse_us = 1500,
        .target_pulse_us = 1500,
    },
};

static struct pwm_output lights[1] = {{MPWM_TC, 3, 1, TC3_PERIOD, 1100, 1900,
                                       1100, LIGHT_PWM_PERIOD_US, 1100, 1100}};

static inline uint32_t us_to_ticks(uint32_t period_ticks,
                                   uint16_t pulse_us,
                                   uint32_t frame_us) {
    return ((uint32_t)pulse_us * (period_ticks + 1U)) / frame_us;
}

static inline uint16_t clamp(uint16_t value, uint16_t low, uint16_t high) {
    if (value < low) {
        return low;
    } else if (value > high) {
        return high;
    } else {
        return value;
    }
}

static inline void tcc_write(uint8_t instance,
                             uint8_t channel,
                             uint32_t ticks) {
    switch (instance) {
        case 0:
            TCC0_PWM24bitDutySet(channel, ticks);
            break;

        case 1:
            TCC1_PWM24bitDutySet(channel, ticks);
            break;

        case 2:
            TCC2_PWM16bitDutySet(channel, ticks);
            break;

        default:
            break;
    }
}

static void set_pwm_neutral(struct pwm_output* outputs, uint32_t count) {
    for (uint32_t i = 0; i < count; i++) {
        uint32_t ticks =
            us_to_ticks(outputs[i].period_ticks, outputs[i].neutral_us,
                        outputs[i].frame_us);

        if (outputs[i].mode == PWM_TCC) {
            tcc_write(outputs[i].instance, outputs[i].channel, ticks);
        } else if (outputs[i].mode == MPWM_TC) {
            TC3_Compare16bitMatch1Set(ticks);
        }

        outputs[i].current_pulse_us = outputs[i].neutral_us;
        outputs[i].target_pulse_us = outputs[i].neutral_us;
    }
    // WDT_Clear();
}

static void set_pwm_outputs(const uint8_t* data,
                     struct pwm_output* outputs,
                     uint32_t count) {
    const uint16_t* pulse_data = (const uint16_t*)data;
    for (uint32_t i = 0; i < count; i++) {
        uint16_t pulse_us = pulse_data[i];

        pulse_us = clamp(pulse_us, outputs[i].min_us, outputs[i].max_us);

        outputs[i].target_pulse_us = pulse_us;
    }

    // WDT_Clear();
}

static void set_light_output(const uint8_t* data,
                      struct pwm_output* outputs,
                      uint32_t count) {
    const uint16_t* pulse_data = (const uint16_t*)data;
    for (uint32_t i = 0; i < count; i++) {
        uint16_t pulse_us = pulse_data[i];
        pulse_us = clamp(pulse_us, outputs[i].min_us, outputs[i].max_us);

        uint32_t ticks =
            us_to_ticks(outputs[i].period_ticks, pulse_us, outputs[i].frame_us);
        TC3_Compare16bitMatch1Set(ticks);

        outputs[i].current_pulse_us = pulse_us;
        outputs[i].target_pulse_us = pulse_us;
    }

    // WDT_Clear();
}

void slew_pwm_outputs(void) {
    for (uint32_t i = 0; i < 8U; i++) {
        uint16_t target = thrusters[i].target_pulse_us;
        uint16_t current = thrusters[i].current_pulse_us;

        if (current < target) {
            uint16_t step = target - current;
            current += (step > PWM_MAX_STEP_US) ? PWM_MAX_STEP_US : step;
        } else if (current > target) {
            uint16_t step = current - target;
            current -= (step > PWM_MAX_STEP_US) ? PWM_MAX_STEP_US : step;
        }

        if (current != thrusters[i].current_pulse_us) {
            thrusters[i].current_pulse_us = current;
            uint32_t ticks = us_to_ticks(thrusters[i].period_ticks, current,
                                         thrusters[i].frame_us);

            tcc_write(thrusters[i].instance, thrusters[i].channel, ticks);
        }
    }
}


void pwm_thrusters_neutral(void)
{
    set_pwm_neutral(thrusters, 8U);
}

void pwm_lights_off(void)
{
    set_pwm_neutral(lights, 1U);
}

void pwm_thrusters_set_from_payload(const uint8_t *data)
{
    set_pwm_outputs(data, thrusters, 8U);
}

void pwm_light_set_from_payload(const uint8_t *data)
{
    set_light_output(data, lights, 1U);
}
