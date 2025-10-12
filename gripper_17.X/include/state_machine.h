#ifndef STATE_MACHINE_H
#define STATE_MACHINE_H

#include "can1.h"
#include "dma.h"
#include "gripper.h"
#include "tc0.h"
#include "tc1.h"

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

void can_rx_callback(uintptr_t context);
void dmac_channel0_callback(DMAC_TRANSFER_EVENT returned_evnt,
                            uintptr_t MyDmacContext);
void tc0_callback(TC_TIMER_STATUS status, uintptr_t context);
void tc1_callback(TC_TIMER_STATUS status, uintptr_t context);

#ifdef __cplusplus
}
#endif  //  __cplusplus

#endif  // !STATE_MACHINE_H
