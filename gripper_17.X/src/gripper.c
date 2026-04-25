#include "gripper.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#if NUM_ENCODERS == 3
static const uint8_t encoder_addresses[NUM_ENCODERS] = {SHOULDER_ADDR,
                                                        WRIST_ADDR, GRIP_ADDR};
#elif NUM_ENCODERS == 2
static const uint8_t encoder_addresses[NUM_ENCODERS] = {WRIST_ADDR, GRIP_ADDR};
#else
#error "Unsupported NUM_ENCODERS"
#endif

typedef void (*tcc_set_fn_t)(uint8_t channel, uint32_t duty);

typedef struct {
    tcc_set_fn_t set_duty;
    uint8_t channel;
} servo_map_t;

static const servo_map_t servo_map[] = {
    {TCC1_PWM24bitDutySet, 1},  // servo 2
    {TCC1_PWM24bitDutySet, 0},  // servo 1
    {TCC0_PWM24bitDutySet, 3},  // servo 0
};

int set_servos_pwm(const uint8_t* pwm_data, uint8_t data_len) {
    uint16_t duty_cycle_us[NUM_ENCODERS];

    if (data_len != sizeof(duty_cycle_us)) {
        return -1;
    }

    memcpy(duty_cycle_us, pwm_data, sizeof(duty_cycle_us));

    for (uint8_t i = 0; i < NUM_ENCODERS; i++) {
        // printf("duty cycle: %d", duty_cycle_us[i]);
        uint32_t tcc_val = ((uint32_t)duty_cycle_us[i] * (TCC_PERIOD + 1u)) /
                           PWM_PERIOD_MICROSECONDS;
        servo_map[i].set_duty(servo_map[i].channel, tcc_val);
    }
    return 0;
}

int start_encoder_read(uint8_t* reg, uint8_t enc_num, uint8_t* out) {
    uint8_t encoder_addr = encoder_addresses[enc_num];
    uint8_t* buf = out;

    if (!SERCOM1_I2C_WriteRead(encoder_addr, reg, 1, buf, 2)) {
        return -1;
    }

    return 0;
}
