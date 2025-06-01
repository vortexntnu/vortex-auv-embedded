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
#include "can_common.h"
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

typedef enum {
    STATE_CAN_RECEIVE,
    STATE_CAN_TRANSMIT,
} CAN_STATES;

typedef enum {
    STOP_GENERATOR = 0x369,
    START_GENERATOR,
    SET_PWM,
    LED,
    RESET_MCU,
} CAN_RECEIVE_ID;

typedef enum {
    I2C_SET_PWM,
    I2C_STOP_GENERATOR,
    I2C_START_GENERATOR,
    I2C_LED,
    I2C_RESET_MCU,
} I2C_STARTBYTE_ID;

// CAN
static uint32_t status = 0;
static uint32_t xferContext = 0;
static uint32_t messageID = 0x169;
static uint32_t rx_messageID = 0;
static uint8_t rx_message[64] = {0};
static uint8_t rx_messageLength = 0;
static uint16_t timestamp = 0;
static CAN_MSG_RX_FRAME_ATTRIBUTE msgFrameAttr = CAN_MSG_RX_DATA_FRAME;


static void SetThrusterPWM(uint8_t* dutyCycleMicroSeconds) {
    uint16_t dutyCycle =
        dutyCycleMicroSeconds[0] << 8 | dutyCycleMicroSeconds[1];
    uint32_t tccValue =
        (dutyCycle * (TCC_PERIOD + 1)) / PWM_PERIOD_MICROSECONDS;
    TCC0_PWM24bitDutySet(0, tccValue);

    dutyCycle = dutyCycleMicroSeconds[2] << 8 | dutyCycleMicroSeconds[3];
    tccValue = (dutyCycle * (TCC_PERIOD + 1)) / PWM_PERIOD_MICROSECONDS;
    TCC0_PWM24bitDutySet(1, tccValue);

    dutyCycle = dutyCycleMicroSeconds[4] << 8 | dutyCycleMicroSeconds[5];
    tccValue = (dutyCycle * (TCC_PERIOD + 1)) / PWM_PERIOD_MICROSECONDS;
    TCC0_PWM24bitDutySet(2, tccValue);

    dutyCycle = dutyCycleMicroSeconds[6] << 8 | dutyCycleMicroSeconds[7];
    tccValue = (dutyCycle * (TCC_PERIOD + 1)) / PWM_PERIOD_MICROSECONDS;
    TCC0_PWM24bitDutySet(3, tccValue);

    dutyCycle = dutyCycleMicroSeconds[8] << 8 | dutyCycleMicroSeconds[9];
    tccValue = (dutyCycle * (TCC_PERIOD + 1)) / PWM_PERIOD_MICROSECONDS;
    TCC1_PWM24bitDutySet(0, tccValue);

    dutyCycle = dutyCycleMicroSeconds[10] << 8 | dutyCycleMicroSeconds[11];
    tccValue = (dutyCycle * (TCC_PERIOD + 1)) / PWM_PERIOD_MICROSECONDS;
    TCC1_PWM24bitDutySet(1, tccValue);

    dutyCycle = dutyCycleMicroSeconds[12] << 8 | dutyCycleMicroSeconds[13];
    tccValue = (dutyCycle * (TCC2_PERIOD + 1)) / PWM_PERIOD_MICROSECONDS;
    TCC2_PWM16bitDutySet(0, (uint16_t)tccValue);

    dutyCycle = dutyCycleMicroSeconds[14] << 8 | dutyCycleMicroSeconds[15];
    tccValue = (dutyCycle * (TCC2_PERIOD + 1)) / PWM_PERIOD_MICROSECONDS;
    TCC2_PWM16bitDutySet(1, (uint16_t)tccValue);

    
    WDT_Clear();
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
                        break;
                    case I2C_START_GENERATOR:
                        TCC0_PWMStart();
                        TCC1_PWMStart();
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
                break;
            case START_GENERATOR:
                TCC0_PWMStart();
                TCC1_PWMStart();
                /*printf("START_GRIPPER\n");*/
                memset(rx_message, 0x00, sizeof(rx_message));
                CAN0_MessageReceive(&rx_messageID, &rx_messageLength,
                                    rx_message, &timestamp,
                                    CAN_MSG_ATTR_RX_FIFO0, &msgFrameAttr);
                break;
            case SET_PWM:
                SetThrusterPWM(rx_message);
                /*printf("SET_PWM");*/
                break;
            case LED:
                TC4_Compare16bitCounterSet(
                    (uint16_t)((rx_message[0] << 8) | rx_message[1]));
                break;
            case RESET_MCU:
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

    /*SERCOM3_SLAVE_I2C_Initialize();*/

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
    
    WDT_Enable();
    while (true) {
    }

    return EXIT_FAILURE;
}



