#include "gripper.h"



int read_encoders(uint8_t reg, uint8_t* data) {
    static const uint8_t encoder_addresses[NUM_ENCODERS] = {
        SHOULDER_ADDR, WRIST_ADDR, GRIP_ADDR};
    uint32_t timeout;

    for (uint8_t i = 0; i < NUM_ENCODERS; i++) {
        uint8_t buf[2] = {0xFF, 0xFF};

        if (!SERCOM1_I2C_WriteRead(encoder_addresses[i], &reg, 1, buf, 2)) {
            return -1;
        }

        timeout = I2C_TIMEOUT;

        while (SERCOM1_I2C_IsBusy()) {
            if (--timeout == 0) {
                break;
            }
        }

        if (timeout == 0 || (buf[0] == 0xFF && buf[1] == 0xFF)) {
            data[2 * i] = 0xFF;
            data[2 * i + 1] = 0xFF;
            continue;
        }

        uint16_t raw_angle = (buf[0] << 6) | (buf[1] & 0x3F);
        data[2 * i] = raw_angle >> 8;
        data[2 * i + 1] = raw_angle & 0xFF;
    }
    return 0;
}



void set_servos_pwm(const uint8_t* pwm_data) {
    uint16_t shoulder_duty = (pwm_data[0] << 8) | pwm_data[1];
    uint16_t wrist_duty = (pwm_data[2] << 8) | pwm_data[3];
    uint16_t grip_duty = (pwm_data[4] << 8) | pwm_data[5];

    uint32_t tcc_val =
        (shoulder_duty * (TCC_PERIOD + 1)) / PWM_PERIOD_MICROSECONDS;
    TCC0_PWM24bitDutySet(3, tcc_val);

    tcc_val = (wrist_duty * (TCC_PERIOD + 1)) / PWM_PERIOD_MICROSECONDS;
    TCC1_PWM24bitDutySet(0, tcc_val);

    tcc_val = (grip_duty * (TCC_PERIOD + 1)) / PWM_PERIOD_MICROSECONDS;
    TCC1_PWM24bitDutySet(1, tcc_val);
}



