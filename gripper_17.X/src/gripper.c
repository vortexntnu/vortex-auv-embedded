#include "gripper.h"
#include <string.h>


typedef void (*tcc_set_fn_t)(uint8_t channel, uint32_t duty);

typedef struct {
    tcc_set_fn_t set_duty;
    uint8_t channel;
} servo_map_t;

static const servo_map_t servo_map[] = {
    { TCC0_PWM24bitDutySet, 3 }, // servo 0
    { TCC1_PWM24bitDutySet, 0 }, // servo 1
    { TCC1_PWM24bitDutySet, 1 }, // servo 2
};

void set_servos_pwm(const uint8_t* pwm_data, uint8_t num_servos) {
    uint16_t duty_cycle_us[3];
    memcpy(duty_cycle_us, pwm_data, sizeof duty_cycle_us);

    for (uint8_t i = 0; i < num_servos; i++) {
        uint32_t tcc_val =
            ((uint32_t)duty_cycle_us[i] * (TCC_PERIOD + 1u)) / PWM_PERIOD_MICROSECONDS;
        servo_map[i].set_duty(servo_map[i].channel, tcc_val);
    }
}



int read_encoders(uint8_t reg, uint8_t enc_num, uint8_t* out){
    static const uint8_t encoder_addresses[NUM_ENCODERS] = {
        SHOULDER_ADDR, WRIST_ADDR, GRIP_ADDR};
    uint8_t encoder_addr = encoder_addresses[enc_num];
    uint8_t* buf = out + enc_num;

    if (!SERCOM1_I2C_WriteRead(encoder_addr, &reg, 1, buf, 2)) {
        return -1;
    }

    return 0;
}




