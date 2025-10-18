#ifndef STATE_MACHINE_H
#define STATE_MACHINE_H

#include "can1.h"
#include "can_common.h"
#include "dma.h"
#include "gripper.h"
#include "tc0.h"
#include "tc1.h"


#define CAN_SEND_ANGLES 0x469

#define TRANSFER_SIZE 16
#define ADC_VREF 5.0f
#define CURRENT_TRESHOLD 2.7f  // 1 A
#define VOLTAGE_THRESHOLD 2048

#define EVENT_SET_PWM (1 << 3)
#define EVENT_READ_ENCODER (1 << 4)
#define EVENT_START_GRIPPER (1 << 5)
#define EVENT_TRANSMIT_ANGLES (1 << 3)

typedef enum {
    STOP_GRIPPER = 0x469,
    START_GRIPPER,
    SET_PWM,
    RESET_MCU,
} CAN_RX_ID;

typedef enum {
    SERVO_1,
    SERVO_2,
    SERVO_3,
} SERVO_ADC_PINS;

#ifdef __cplusplus
extern "C" {
#endif

void state_machine(volatile uint32_t* events, struct can_tx_frame* tx_frame, struct can_rx_frame* rx_frame);
void can_rx_callback(uintptr_t context);
void dmac_channel0_callback(DMAC_TRANSFER_EVENT returned_evnt,
                            uintptr_t MyDmacContext);
void tc0_callback(TC_TIMER_STATUS status, uintptr_t context);
void tc1_callback(TC_TIMER_STATUS status, uintptr_t context);



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
#endif  //  __cplusplus

#endif  // !STATE_MACHINE_H
