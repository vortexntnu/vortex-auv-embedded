#ifndef SYSTEM_INIT_H
#define SYSTEM_INIT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include "adc0.h"
#include "can1.h"
#include "can_common.h"
#include "clock.h"
#include "dma.h"
#include "i2c.h"  // I2C client backup
#include "pm.h"
#include "rtc.h"
#include "sercom1_i2c.h"  // Encoder i2c
#include "tc0.h"
#include "tcc.h"
#include "tcc0.h"
#include "tcc_common.h"
#include "wdt.h"

#ifdef DEBUG
#include "usart.h"
#endif

// Encoder
#define SHOULDER_ADDR 0x40
#define WRIST_ADDR 0x41
#define GRIP_ADDR 0x42
#define ANGLE_REGISTER 0xFE
#define I2C_TIMEOUT 100000
#define NUM_ENCODERS 3

#define TRANSFER_SIZE 16
#define ADC_VREF 5.0f
#define CURRENT_TRESHOLD 2.7f  // 1 A
#define VOLTAGE_THRESHOLD 2048
#define RTC_COMPARE_VAL 50

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    STATE_CAN_RECEIVE,
    STATE_CAN_TRANSMIT,
} CAN_STATES;

typedef enum {
    STOP_GRIPPER = 0x469,
    START_GRIPPER,
    SET_PWM,
    RESET_MCU,
} CAN_RECEIVE_ID;

typedef enum {
    SERVO_1,
    SERVO_2,
    SERVO_3,
} SERVO_ADC_PINS;

/**
 *@brief Initializes all system peripherals
 */
void system_init(void);

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
#endif

#endif /* SYSTEM_INIT_H */
