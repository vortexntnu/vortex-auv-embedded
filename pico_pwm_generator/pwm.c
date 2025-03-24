#include "pwm.h"
#include "hardware/pwm.h"
#include "hardware/clocks.h"
#include "hardware/irq.h"
#include "pico/stdlib.h"
#include <string.h>

int esc_attach(const uint pin) {
    gpio_set_function(pin, GPIO_FUNC_PWM);
    const uint8_t slice = pwm_gpio_to_slice_num(pin);

    const uint freq = 50;
    uint32_t source_hz = clock_get_hz(clk_sys);
    uint32_t div16_top = 16 * source_hz / freq; 
    uint32_t top = 1;

    while(true) {
        if(div16_top >= 16 * 5 && div16_top % 5 == 0 && top * 5 <= 65534) {
            div16_top /= 5;
            top *= 5;
        } else if(div16_top >= 16 * 3 && div16_top % 3 == 0 && top * 3 <= 65534) {
            div16_top /= 3;
            top *= 3;
        } else if(div16_top >= 16 * 2 && top * 2 <= 65534) {
            div16_top /= 2;
            top *= 2;
        } else {
            break;
        }
    }

    if(div16_top < 16) {
        return 2;
    } else if(div16_top >= 256 * 16) {
        return 1;
    }
    pwm_hw->slice[slice].div = div16_top;
    pwm_hw->slice[slice].top = top;

    return 0;
}

int esc_set_pwm(const uint pin, uint16_t esc_pwm) {

    if(esc_pwm < MIN_PWM) {
        esc_pwm = MIN_PWM;
    } else if(esc_pwm > MAX_PWM) {
        esc_pwm = MAX_PWM;
    }

    const uint8_t slice = pwm_gpio_to_slice_num(pin);
    const uint8_t channel = pwm_gpio_to_channel(pin);
    const uint32_t top = pwm_hw->slice[slice].top;

    const uint32_t duty = (esc_pwm * (top + 1)) / 20000;

    pwm_set_chan_level(slice, channel, duty);
    pwm_set_enabled(slice, true);

    return 0;
}