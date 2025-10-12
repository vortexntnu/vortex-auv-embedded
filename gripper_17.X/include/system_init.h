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

#define CAN_SEND_ANGLES 0x469



#ifdef __cplusplus
extern "C" {
#endif




/**
 *@brief Initializes all system peripherals
 */
void system_init(void);

static inline bool can_transmit(struct can_tx_frame* frame) {
    return CAN0_MessageTransmit(frame->id, frame->len, frame->buf,
                                CAN_MODE_FD_WITHOUT_BRS,
                                CAN_MSG_ATTR_TX_FIFO_DATA_FRAME);
}

static inline bool can_recieve(struct can_rx_frame* frame) {
    return CAN0_MessageReceive(&frame->id, &frame->len, frame->buf,
                               &frame->timestamp, CAN_MSG_ATTR_RX_FIFO0,
                               &frame->msg_atr);
}

#ifdef __cplusplus
}
#endif

#endif /* SYSTEM_INIT_H */
