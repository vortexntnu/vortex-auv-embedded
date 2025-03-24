#include "pwm.h"
#include "hardware/pwm.h"
#include "hardware/clocks.h"
#include "hardware/irq.h"
#include "pico/stdlib.h"
#include <string.h>

// Global variables used by PWM functions
float clkdiv;
uint min;
uint max;

int slice_map[30];
uint slice_active[8];
uint esc_pos[32];
uint esc_pos_buf[16];
pwm_config slice_cfg[8];

uint min_us = 500;
uint max_us = 2500;
float us_per_unit = 0.f;

void wrap_cb(void) {
    uint offset;
    for (uint8_t i = 0; i < 8; i++) {
        if (slice_active[i] == 0)
            continue;
        pwm_clear_irq(i);
        offset = 16 * (esc_pos_buf[i] % 2);
        pwm_set_chan_level(i, 0, esc_pos[offset + 1]);
    }
}

void esc_set_bounds(uint a, uint b) {
    min_us = a;
    max_us = b;
    if (us_per_unit > 0.f) {
        min = (uint)(min_us / us_per_unit);
        max = (uint)(max_us / us_per_unit);
    }
}

int esc_init(void) {
    for (uint8_t i = 0; i < 30; i++) {
        slice_map[i] = -1;
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
    if (clkdiv == 0)
        return -1;
    us_per_unit = 1.f / (PWM_FREQ * 1e4f) / 1e-6f;
    min = (uint)(min_us / us_per_unit);
    max = (uint)(max_us / us_per_unit);
    return 0;
}

int esc_clock_manual(uint freq) {
    clkdiv = (float)(freq * 1e3) / (float)(PWM_FREQ * 1e4);
    if (clkdiv == 0)
        return -1;
    min = (uint)(0.025f * 1e4f);
    max = (uint)(0.125f * 1e4f);
    return 0;
}

int esc_attach(uint pin) {
    uint slice = pwm_gpio_to_slice_num(pin);
    if (slice_active[slice] >= 2)
        return 1;
    gpio_set_function(pin, GPIO_FUNC_PWM);
    slice_map[pin] = slice;
    if (slice_active[slice] == 0) {
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
    if (slice_map[pin] < 0)
        return 1;
    uint val = (uint)(((float)angle / 180.0f) * (max - min)) + min;
    uint pos = slice_map[pin] + (pin % 2);
    esc_pos[16 * esc_pos_buf[pos] + pos] = val;
    esc_pos_buf[pos] = (esc_pos_buf[pos] + 1) % 2;
    return 0;
}

int esc_us(uint pin, uint us) {
    if (slice_map[pin] < 0)
        return 1;
    uint pos = slice_map[pin] + (pin % 2);
    esc_pos[16 * esc_pos_buf[pos] + pos] = us;
    esc_pos_buf[pos] = (esc_pos_buf[pos] + 1) % 2;
    return 0;
}
