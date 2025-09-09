/* 
 * File:   system_init.h
 * Author: nathaniel
 *
 * Created on January 19, 2025, 4:29 PM
 */

#ifndef SYSTEM_INIT_H
#define	SYSTEM_INIT_H



#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "can1.h"
#include "can_common.h"
#include "clock.h"
#include "i2c.h"  // I2C client backup
/*#include "dma.h"*/
#include "pm.h"
#include "tc4.h"
#include "tcc.h"
#include "tcc0.h"
#include "tcc2.h"
#include "tcc_common.h"
#include "usart.h"
#include "wdt.h"





#ifdef	__cplusplus
extern "C" {
#endif

typedef enum {
    STATE_CAN_RECEIVE,
    STATE_CAN_TRANSMIT,
} CAN_STATES;

typedef enum {
    STOP_GENERATOR,
    START_GENERATOR,
    SET_PWM,
    LED,
    RESET_MCU,
} STATES;

struct ThrusterTable{
    uint8_t tcc_num;
    uint8_t channel;
    uint32_t period;
};
/**
 *@brief Initializes all system peripherals
 */
void system_init(void);

#ifdef	__cplusplus
}
#endif

#endif	/* SYSTEM_INIT_H */

