#ifndef PWM_H
#define PWM_H

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

#ifdef __cplusplus
extern "C" {
#endif

// PWM function prototypes
void wrap_cb(void);
void esc_set_bounds(uint a, uint b);
int esc_init(void);
int esc_clock_auto(void);
int esc_clock_source(uint src);
int esc_clock_manual(uint freq);
int esc_attach(uint pin);
int esc_set_speed(uint pin, uint angle);
int esc_us(uint pin, uint us);

#ifdef __cplusplus
}
#endif

#endif // PWM_H
