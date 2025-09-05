#ifndef PWM_H
#define PWM_H
#include "common.h"

#include <stdint.h>
#include "hardware/pwm.h"
#include "hardware/clocks.h"
#include "hardware/irq.h"
#include "pico/stdlib.h"

// PWM configuration defines
#define PWM_FREQ        50
#define ESC1            18
#define ESC2            19
#define ESC3            20
#define ESC4            21
#define ESC5            22
#define ESC6            26
#define ESC7            27
#define ESC8            28

#define ARMING_PWM      1500
#define STARTUP_DELAY   1500

#define MAX_PWM         1800
#define MIN_PWM         1200

#ifdef __cplusplus
extern "C" {
#endif

// PWM function prototypes
int esc_set_pwm(const uint pin, uint16_t esc_pwm);
int esc_attach(const uint pin);

#ifdef __cplusplus
}
#endif

#endif // PWM_H
