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
#include "tc1.h"
#include "tcc.h"
#include "tcc0.h"
#include "tcc_common.h"
#include "wdt.h"

#ifdef DEBUG
#include "usart.h"
#endif




#ifdef __cplusplus
extern "C" {
#endif




/**
 *@brief Initializes all system peripherals
 */
void system_init(void);


#ifdef __cplusplus
}
#endif

#endif /* SYSTEM_INIT_H */
