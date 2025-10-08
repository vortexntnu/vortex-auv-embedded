#ifndef SYSTEM_INIT_H
#define SYSTEM_INIT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
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
#include "tcc.h"
#include "tcc0.h"
#include "tcc_common.h"
#include "usart.h"
#include "wdt.h"
#include "tc0.h"

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
    STOP_GRIPPER,
    START_GRIPPER,
    SET_PWM,
    RESET_MCU,
} CAN_RECEIVE_ID;

typedef enum {
    I2C_SET_PWM,
    I2C_STOP_GRIPPER,
    I2C_START_GRIPPER,
    I2C_RESET_MCU,
} I2C_STARTBYTE_ID;

typedef enum {
    SERVO_1,
    SERVO_2,
    SERVO_3,
} SERVO_ADC_PINS;

/**
 *@brief Initializes all system peripherals
 */
void system_init(void);



#ifdef __cplusplus
}
#endif

#endif /* SYSTEM_INIT_H */
