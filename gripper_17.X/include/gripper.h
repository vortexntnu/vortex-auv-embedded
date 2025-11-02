
#ifndef GRIPPER_H
#define GRIPPER_H

#include <stdint.h>
#include "sercom1_i2c.h"
#include "tcc.h"
#include "tcc0.h"
#include "adc0.h"
#include "wdt.h"
#include "rtc.h"



// Encoder
#define SHOULDER_ADDR 0x40
#define WRIST_ADDR 0x41
#define GRIP_ADDR 0x42
#define ANGLE_REGISTER 0xFE
#define I2C_TIMEOUT 100000
#define NUM_ENCODERS 3

#define RTC_COMPARE_VAL 50


#ifdef __cplusplus
extern "C"{
#endif // __cplusplus


/**
 * @brief Read encoder angle registers over I2C.
 *
 *
 * @param[in]  reg   Register address to read from each encoder.
 * @param[out] data  Pointer to a buffer that will receive the angle data.
 *                   Must be at least 2 * NUM_ENCODERS bytes long.
 *
 * @return  0  on success,
 *         -1  on failure
 */
int read_encoders(uint8_t reg, uint8_t* data);


/**
 * @brief Set servo PWM duty cycles.
 *
 * @param[in] pwm_data  Pointer to an array containing the PWM duty values
 *                      for each servo channel. Array length must match
 *                      the number of servos.
 */
void set_servos_pwm(const uint8_t* pwm_data);


static inline void stop_gripper(void) {
    WDT_Disable();
    PORT_REGS->GROUP[0].PORT_OUTCLR = (1 << 0) | (1 << 27) | (1 << 28);
    TCC0_PWMStop();
    TCC1_PWMStop();
    ADC0_Disable();
}

static inline void start_gripper(void) {
    TCC0_PWMStart();
    TCC1_PWMStart();
    ADC0_Enable();
    RTC_Timer32Start();
    RTC_Timer32CompareSet(RTC_COMPARE_VAL);
    PORT_REGS->GROUP[0].PORT_OUTSET = (1 << 0) | (1 << 27) | (1 << 28);
}


#ifdef __cplusplus
}
#endif // __cplusplus

#endif // !GRIPPER_H
