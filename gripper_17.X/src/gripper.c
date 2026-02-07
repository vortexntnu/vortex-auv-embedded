#include "gripper.h"
#include <string.h>



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


void set_servos_pwm(const uint8_t* pwm_data) {
    uint16_t pwm[3];
    memcpy(pwm, pwm_data, sizeof(pwm));

    uint32_t tcc_val = (pwm[0] * (TCC_PERIOD + 1u)) / PWM_PERIOD_MICROSECONDS;
    TCC0_PWM24bitDutySet(3, tcc_val);

    tcc_val = (pwm[1] * (TCC_PERIOD + 1u)) / PWM_PERIOD_MICROSECONDS;
    TCC1_PWM24bitDutySet(0, tcc_val);

    tcc_val = (pwm[2] * (TCC_PERIOD + 1u)) / PWM_PERIOD_MICROSECONDS;
    TCC1_PWM24bitDutySet(1, tcc_val);
}



