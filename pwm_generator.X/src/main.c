/*
 * File:   main.c
 * Author: nathaniel
 *
 * Created on January 19, 2025, 2:50 PM
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "can1.h"
#include "clock.h"
#include "i2c.h"  // I2C client backup
/*#include "dma.h"*/
#include "pm.h"
#include "sam.h"
#include "samc21e17a.h"
#include "system_init.h"
#include "tc4.h"
#include "tcc.h"
#include "tcc0.h"
#include "tcc2.h"
#include "tcc_common.h"
#include "usart.h"
#include "wdt.h"

uint8_t Can0MessageRAM[CAN0_MESSAGE_RAM_CONFIG_SIZE]
    __attribute__((aligned(32)));

/* Application's state machine enum */
typedef enum {
    STATE_IDLE,
    STATE_GENERATOR_ACTIVE,
} STATES;

typedef enum {
    STATE_CAN_RECEIVE,
    STATE_CAN_TRANSMIT,
} CAN_STATES;

// CAN FD RECIEVE ID table
typedef enum {
    STOP_GENERATOR = 0x369,
    START_GENERATOR,
    SET_PWM,
    LED,
    RESET_MCU,
} CAN_RECEIVE_ID;

// I2C RECIEVE START BYTE table
typedef enum {
    I2C_SET_PWM,
    I2C_STOP_GENERATOR,
    I2C_START_GENERATOR,
    I2C_LED,
    I2C_RESET_MCU,
} I2C_STARTBYTE_ID;

/* Variable to save Tx/Rx transfer status and context */
static uint32_t status = 0;
static uint32_t xferContext = 0;
/* Variable to save Tx/Rx message */
static uint32_t messageID = 0x169;
static uint32_t rx_messageID = 0;
// static uint8_t message[64] = {0};
static uint8_t rx_message[64] = {0};
// static uint8_t messageLength = 0;
static uint8_t rx_messageLength = 0;
static uint16_t timestamp = 0;
static CAN_MSG_RX_FRAME_ATTRIBUTE msgFrameAttr = CAN_MSG_RX_DATA_FRAME;

/* Variable to save application state */
/*volatile static STATES pwm_generator_state = STATE_IDLE;*/

static void SetLEDPWM(uint8_t* dutyCycleMicroSeconds);
bool SERCOM_I2C_Callback(SERCOM_I2C_SLAVE_TRANSFER_EVENT event,
                         uintptr_t contextHandle);
void CAN_Recieve_Callback(uintptr_t context);
void CAN_Transmit_Callback(uintptr_t context);
void TCC_PeriodEventHandler(uint32_t status, uintptr_t context);

int main(void) {
    NVMCTRL_REGS->NVMCTRL_CTRLB = NVMCTRL_CTRLB_RWS(3);
    PM_Initialize();
    PIN_Initialize();
    CLOCK_Initialize();
    NVMCTRL_Initialize();
    TCC0_PWMInitialize();
    TCC1_PWMInitialize();
    TCC2_PWMInitialize();
    TC4_CompareInitialize();
    CAN0_Initialize();

    SERCOM3_USART_Initialize();  // USART for Debugging

    // SERCOM3_SLAVE_I2C_Initialize();

    NVIC_Initialize();

    TCC0_PWMStart();
    TCC1_PWMStart();
    TCC2_PWMStart();

    TC4_CompareStart();

    CAN0_MessageRAMConfigSet(Can0MessageRAM);

    // SERCOM3_I2C_CallbackRegister(SERCOM_I2C_Callback, 0);

    // TCC0_PWMCallbackRegister(TCC_PeriodEventHandler, (uintptr_t)NULL);

    CAN0_RxCallbackRegister(CAN_Recieve_Callback, (uintptr_t)STATE_CAN_RECEIVE,
                            CAN_MSG_ATTR_RX_FIFO0);
    CAN0_TxCallbackRegister(CAN_Transmit_Callback,
                            (uintptr_t)STATE_CAN_TRANSMIT);
    memset(rx_message, 0x00, sizeof(rx_message));
    CAN0_MessageReceive(&rx_messageID, &rx_messageLength, rx_message,
                        &timestamp, CAN_MSG_ATTR_RX_FIFO0, &msgFrameAttr);

    /*printf("Initialize complete\n");*/
    while (true) {
        /*switch (pwm_generator_state) {*/
        /*    case STATE_IDLE:*/
        /*        PM_IdleModeEnter();*/
        /*        break;*/
        /*    case STATE_GENERATOR_ACTIVE:*/
        /*        break;*/
        /*    default:*/
        /*        break;*/
        /*}*/
    }

    /* Execution should not come here during normal operation */
    return EXIT_FAILURE;
}

static void SetThrusterPWM(uint8_t* dutyCycleMicroSeconds) {
    uint16_t dutyCycle =
        dutyCycleMicroSeconds[0] << 8 | dutyCycleMicroSeconds[1];
    uint32_t tccValue =
        (dutyCycle * (TCC_PERIOD + 1)) / PWM_PERIOD_MICROSECONDS;

    for (int thruster = 0; thruster < 8; thruster++) {
        int offset = thruster * 2;
        dutyCycle = (dutyCycleMicroSeconds[offset] << 8) |
                    dutyCycleMicroSeconds[offset + 1];

        tccValue = (dutyCycle * (TCC_PERIOD + 1)) / PWM_PERIOD_MICROSECONDS;

        if (thruster < 4) {
            TCC0_PWM24bitDutySet(thruster, tccValue);
        } else if (thruster < 6) {
            TCC1_PWM24bitDutySet(thruster - 4, tccValue);
        } else {
            TCC2_PWM16bitDutySet(thruster - 6, tccValue);
        }
    }
}

bool SERCOM_I2C_Callback(SERCOM_I2C_SLAVE_TRANSFER_EVENT event,
                         uintptr_t contextHandle) {
    static uint8_t dataBuffer[17];

    static uint8_t dataIndex = 0;

    switch (event) {
        case SERCOM_I2C_SLAVE_TRANSFER_EVENT_ADDR_MATCH:
            dataIndex = 0;
            break;

        case SERCOM_I2C_SLAVE_TRANSFER_EVENT_RX_READY:
            /* Read the data sent by I2C Host */

            if (dataIndex < sizeof(dataBuffer)) {
                dataBuffer[dataIndex++] = SERCOM3_I2C_ReadByte();
            }
            break;

        case SERCOM_I2C_SLAVE_TRANSFER_EVENT_TX_READY: {
            break;
        }

        case SERCOM_I2C_SLAVE_TRANSFER_EVENT_STOP_BIT_RECEIVED:
            if (SERCOM3_I2C_TransferDirGet() ==
                SERCOM_I2C_SLAVE_TRANSFER_DIR_WRITE) {
                // First byte indicating what the MCU should do
                uint8_t start_byte = dataBuffer[0];
                switch (start_byte) {
                    case I2C_SET_PWM:
                        SetThrusterPWM(dataBuffer + 1);
                        break;
                    case I2C_STOP_GENERATOR:
                        TCC0_PWMStop();
                        TCC1_PWMStop();
                        /*pwm_generator_state = STATE_IDLE;*/
                        break;
                    case I2C_START_GENERATOR:
                        TCC0_PWMStart();
                        TCC1_PWMStart();
                        /*pwm_generator_state = STATE_GENERATOR_ACTIVE;*/
                        break;
                    case I2C_LED:
                        TC4_Compare16bitCounterSet(
                            (uint16_t)((dataBuffer[1] << 8) | dataBuffer[2]));
                        break;
                    case I2C_RESET_MCU:
                        WDT_REGS->WDT_CLEAR = 0x0;
                    default:
                        break;
                }
                /* Only used for debugging */
                // printf("Message recieved\n");
                // for (int i = 0; i < 7; i++) {
                //     printf("%x\n", dataBuffer[i]);
                // }
            }
            break;
        default:
            break;
    }

    return true;
}

void CAN_Recieve_Callback(uintptr_t context) {
    xferContext = context;

    /* Check CAN Status */
    status = CAN0_ErrorGet();
    /*printf("Entering callback\n");*/

    if (((status & CAN_PSR_LEC_Msk) == CAN_ERROR_NONE) ||
        ((status & CAN_PSR_LEC_Msk) == CAN_ERROR_LEC_NC)) {
        switch (rx_messageID) {
            case STOP_GENERATOR:
                TCC0_PWMStop();
                TCC1_PWMStop();
                CAN0_MessageReceive(&rx_messageID, &rx_messageLength,
                                    rx_message, &timestamp,
                                    CAN_MSG_ATTR_RX_FIFO0, &msgFrameAttr);
                /*printf("STOP_GRIPPER\n");*/
                // PM_IdleModeEnter();
                /*pwm_generator_state = STATE_IDLE;*/
                break;
            case START_GENERATOR:
                TCC0_PWMStart();
                TCC1_PWMStart();
                // Turn on servo enable pins
                /*printf("START_GRIPPER\n");*/
                memset(rx_message, 0x00, sizeof(rx_message));
                CAN0_MessageReceive(&rx_messageID, &rx_messageLength,
                                    rx_message, &timestamp,
                                    CAN_MSG_ATTR_RX_FIFO0, &msgFrameAttr);
                /*pwm_generator_state = STATE_GENERATOR_ACTIVE;*/
                break;
            case SET_PWM:
                SetThrusterPWM(rx_message);
                // Reading encoders
                // Sending ENCODER data over CAN
                /*printf("SET_PWM");*/
                break;
            case LED:
                TC4_Compare16bitCounterSet(
                    (uint16_t)((rx_message[0] << 8) | rx_message[1]));
                break;
            case RESET_MCU:
                // This will cause an immediate system reset
                // Writing anything other than 0xA5 to WDT_CLEAR will
                // reset the device
                /*printf("RESET_MCU\n");*/
                WDT_REGS->WDT_CLEAR = 0x0;
                break;
            default:
                break;
        }
        /* Only used for debugging */
        /*printf(" New Message Received\r\n");*/
        /*uint8_t length = rx_messageLength;*/
        /*printf(*/
        /*    " Message - Timestamp : 0x%x ID : 0x%x Length "*/
        /*    ":0x%x",*/
        /*    (unsigned int)timestamp, (unsigned int)rx_messageID,*/
        /*    (unsigned int)rx_messageLength);*/
        /*printf("Message : ");*/
        /*while (length) {*/
        /*    printf("0x%x ", rx_message[rx_messageLength - length--]);*/
        /*}*/
    }
}

void CAN_Transmit_Callback(uintptr_t context) {
    /* Check CAN Status */
    status = CAN0_ErrorGet();

    if (((status & CAN_PSR_LEC_Msk) == CAN_ERROR_NONE) ||
        ((status & CAN_PSR_LEC_Msk) == CAN_ERROR_LEC_NC)) {
        // Sending encoder data
        memset(rx_message, 0x00, sizeof(rx_message));
        if (CAN0_MessageReceive(&rx_messageID, &rx_messageLength, rx_message,
                                &timestamp, CAN_MSG_ATTR_RX_FIFO0,
                                &msgFrameAttr) == false) {
        }
    }
}

void TCC_PeriodEventHandler(uint32_t status, uintptr_t context) {
    /* duty cycle values */
    static int8_t increment1 = 10;
    static uint32_t duty1 = 0;

    // Sets PWM on 2 and 2 thrusters at the same time
    for (int i = 0; i < 4; i++) {
        TCC0_PWM24bitDutySet(i, duty1);
        TCC1_PWM24bitDutySet(i, duty1);
    }

    /* Increment duty cycle values */
    duty1 += increment1;

    if (duty1 > PWM_MAX) {
        duty1 = PWM_MAX;
        increment1 *= -1;
    } else if (duty1 < PWM_MIN) {
        duty1 = PWM_MIN;
        increment1 *= -1;
    }
}
