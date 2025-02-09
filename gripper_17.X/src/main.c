/*
 * File:   main.c
 * Author: nathaniel
 *
 * Created on January 19, 2025, 2:50 PM
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "can1.h"
#include "can_common.h"
#include "clock.h"
#include "i2c.h"  // I2C client backup
// #include "i2c_master.h" // not used
#include "sam.h"
#include "sercom0_i2c.h"
#include "sercom1_i2c.h"
#include "sercom3_i2c.h"
#include "system_init.h"
#include "tcc.h"
#include "tcc0.h"
// #include "usart.h"

// Encoder
#define ENCODER_ADDR 0x40
#define ENCODER2_ADDR 0x41
#define ENCODER3_ADDR 0x42
#define ANGLE_REGISTER 0xFE

uint8_t Can0MessageRAM[CAN0_MESSAGE_RAM_CONFIG_SIZE]
    __attribute__((aligned(32)));

/* Application's state machine enum */
typedef enum {
    STATE_CAN_RECEIVE,
    STATE_SET_PWM,
    STATE_READ_ENCODER,
    STATE_CAN_TRANSMIT,
    STATE_CAN_XFER_SUCCESSFUL,
    STATE_CAN_XFER_ERROR,
    STATE_IDLE,
} STATES;

/* Variable to save Tx/Rx transfer status and context */
static uint32_t status = 0;
static uint32_t xferContext = 0;
/* Variable to save Tx/Rx message */
static uint32_t messageID = 0;
static uint32_t rx_messageID = 0;
// static uint8_t message[64] = {0};
static uint8_t rx_message[64] = {0};
// static uint8_t messageLength = 0;
static uint8_t rx_messageLength = 0;
static uint16_t timestamp = 0;
static CAN_MSG_RX_FRAME_ATTRIBUTE msgFrameAttr = CAN_MSG_RX_DATA_FRAME;

/* Variable to save application state */
// volatile static STATES states = STATE_CAN_RECEIVE;

static uint8_t encoder_angles[6];

// Reads the encoder angle Register
// 2 bytes for each encoder
// 2|2|2 SHOULDER, WRIST, GRIP
uint8_t Encoder_Read(uint8_t* data, uint8_t address, uint8_t reg) {
    uint16_t rawData[3] = {0};
    uint8_t dataBuffer[2] = {0};
    // SHOULDER
    if (!SERCOM0_I2C_WriteRead(address, &reg, 1, dataBuffer, 2)) {
        return 0;
    }
    while (SERCOM0_I2C_IsBusy());
    rawData[0] = (dataBuffer[1] << 6) | (dataBuffer[0] & 0x3F);
    // WRIST

    if (!SERCOM1_I2C_WriteRead(address, &reg, 1, dataBuffer, 2)) {
        return 0;
    }
    while (SERCOM1_I2C_IsBusy()); 

    rawData[1] = (dataBuffer[1] << 6) | (dataBuffer[0] & 0x3F);
    // GRIP

    if (!SERCOM3_I2C_WriteRead(address, &reg, 1, dataBuffer, 2)) {
        return 0;
    }
    
    while (SERCOM3_I2C_IsBusy());
    rawData[2] = (dataBuffer[1] << 6) | (dataBuffer[0] & 0x3F);

    data[0] = rawData[0] >> 8;
    data[1] = rawData[0] & 0xFF;
    data[2] = rawData[1] >> 8;
    data[3] = rawData[1] & 0xFF;
    data[4] = rawData[2] >> 8;
    data[5] = rawData[2] & 0xFF;
    return 1;
}

// Function to set the TCC duty cycle for a specific channel
void SetPWMDutyCycle(uint8_t* dutyCycleMicroSeconds) {
    uint16_t shoulderDuty =
        (dutyCycleMicroSeconds[0] << 8) | dutyCycleMicroSeconds[1];
    uint16_t wristDuty =
        (dutyCycleMicroSeconds[2] << 8) | dutyCycleMicroSeconds[3];
    uint16_t gripDuty =
        (dutyCycleMicroSeconds[4] << 8) | dutyCycleMicroSeconds[5];

    // SHOULDER
    uint32_t tccValue =
        (shoulderDuty * (TCC_PERIOD + 1)) / PWM_PERIOD_MICROSECONDS;

    TCC0_PWM24bitDutySet(3, tccValue);

    // WRIST
    tccValue = (wristDuty * (TCC_PERIOD + 1)) / PWM_PERIOD_MICROSECONDS;
    TCC1_PWM24bitDutySet(0, tccValue);

    // GRIP
    tccValue = (gripDuty * (TCC_PERIOD + 1)) / PWM_PERIOD_MICROSECONDS;
    TCC1_PWM24bitDutySet(1, tccValue);
}

// I2C client backup code
bool SERCOM_I2C_Callback(SERCOM_I2C_SLAVE_TRANSFER_EVENT event,
                         uintptr_t contextHandle) {
    static uint8_t
        dataBuffer[7];  // Buffer to store {channel, high byte, low byte}
    static uint8_t dataIndex = 0;

    switch (event) {
        case SERCOM_I2C_SLAVE_TRANSFER_EVENT_ADDR_MATCH:
            dataIndex = 0;
            break;

        case SERCOM_I2C_SLAVE_TRANSFER_EVENT_RX_READY:
            /* Read the data sent by I2C Host */

            if (dataIndex < sizeof(dataBuffer)) {
                dataBuffer[dataIndex++] = SERCOM2_I2C_ReadByte();
            }

            break;

        case SERCOM_I2C_SLAVE_TRANSFER_EVENT_TX_READY: {
            SERCOM2_I2C_WriteByte(encoder_angles[dataIndex++]);
            break;
        }

        case SERCOM_I2C_SLAVE_TRANSFER_EVENT_STOP_BIT_RECEIVED:
            if (dataIndex == 7 && ((SERCOM2_I2C_TransferDirGet() ==
                                    SERCOM_I2C_SLAVE_TRANSFER_DIR_WRITE))) {
                SetPWMDutyCycle(
                    dataBuffer +
                    1);  // Because of start byte start indexing at +1

                Encoder_Read(encoder_angles, ENCODER_ADDR, ANGLE_REGISTER);
            }
            break;
        default:
            break;
    }

    return true;
}

// Callback function called during CAN interrupt.
// When a recieve interrupt is triggered it will
// set the PWM duty cycle with the data it has recieved 
// it will then read the encoders and send the data
// after a successfull transmit reviece interrupts will
// be reenabled
void APP_CAN_Callback(uintptr_t context) {
    xferContext = context;

    /* Check CAN Status */
    status = CAN0_ErrorGet();

    if (((status & CAN_PSR_LEC_Msk) == CAN_ERROR_NONE) ||
        ((status & CAN_PSR_LEC_Msk) == CAN_ERROR_LEC_NC)) {
        switch ((STATES)context) {
            case STATE_CAN_RECEIVE:

                SetPWMDutyCycle(rx_message);

                // Only include this if testing without encoders
                // if (CAN0_MessageReceive(&rx_messageID, &rx_messageLength,
                //                         rx_message, &timestamp,
                //                         CAN_MSG_ATTR_RX_FIFO0,
                //                         &msgFrameAttr) == false) {
                // }

                if (!Encoder_Read(encoder_angles, ENCODER_ADDR,
                                  ANGLE_REGISTER)) {
                    break;
                }

                if (CAN0_MessageTransmit(
                        messageID, 6, encoder_angles, CAN_MODE_FD_WITHOUT_BRS,
                        CAN_MSG_ATTR_TX_FIFO_DATA_FRAME) == false) {
                }
                break;
            case STATE_CAN_TRANSMIT: {
                if (CAN0_MessageReceive(&rx_messageID, &rx_messageLength,
                                        rx_message, &timestamp,
                                        CAN_MSG_ATTR_RX_FIFO0,
                                        &msgFrameAttr) == false) {
                }
                // states = STATE_CAN_XFER_SUCCESSFUL;
                break;
            }
            default:
                break;
        }
    } else {
        // states = STATE_CAN_XFER_ERROR;
    }
}
int main(void) {
    /* Initialize all modules */
    NVMCTRL_REGS->NVMCTRL_CTRLB = NVMCTRL_CTRLB_RWS(3);
    CLOCK_Initialize();
    NVMCTRL_Initialize();
    PIN_Initialize();
    TCC1_PWMInitialize();
    TCC0_PWMInitialize();
    CAN0_Initialize();
    SERCOM0_I2C_Initialize();  // I2C 3
    SERCOM1_I2C_Initialize();  // I2C 2
    SERCOM3_I2C_Initialize();  // I2C 1
    // SERCOM4_USART_Initialize();  // USART for Debugging

    // SERCOM2_I2C_Initialize();  // CLient (backup)
    NVIC_Initialize();  // Enable interrupts

    /* Start PWM*/
    TCC1_PWMStart();
    TCC0_PWMStart();

    CAN0_MessageRAMConfigSet(Can0MessageRAM);

    // Callback functions
    // SERCOM2_I2C_CallbackRegister(SERCOM_I2C_Callback, 0);

    CAN0_RxCallbackRegister(APP_CAN_Callback, (uintptr_t)STATE_CAN_RECEIVE,
                            CAN_MSG_ATTR_RX_FIFO0);
    CAN0_TxCallbackRegister(APP_CAN_Callback, (uintptr_t)STATE_CAN_TRANSMIT);

    if (CAN0_MessageReceive(&rx_messageID, &rx_messageLength, rx_message,
                            &timestamp, CAN_MSG_ATTR_RX_FIFO0,
                            &msgFrameAttr) == false) {
    }

    while (true) {
        // switch (states) {
        //     case STATE_CAN_RECEIVE:
        //         CAN0_RxCallbackRegister(APP_CAN_Callback,
        //                                 (uintptr_t)STATE_CAN_RECEIVE,
        //                                 CAN_MSG_ATTR_RX_FIFO0);
        //         states = STATE_IDLE;
        //         memset(rx_message, 0x00, sizeof(rx_message));
        //         /* Receive New Message */
        //         if (CAN0_MessageReceive(&rx_messageID, &rx_messageLength,
        //                                 rx_message, &timestamp,
        //                                 CAN_MSG_ATTR_RX_FIFO0,
        //                                 &msgFrameAttr) == false) {
        //         }
        //         break;
        //
        //     case STATE_IDLE:
        //         // Waiting for CAN interrupt
        //         break;
        //     case STATE_SET_PWM:
        //
        //         // Update and set the duty cycle for the channel
        //         SetPWMDutyCycle(rx_message);
        //
        //         states = STATE_READ_ENCODER;
        //         // states = STATE_CAN_RECEIVE;
        //         break;
        //
        //     case STATE_READ_ENCODER:
        //         if (!Encoder_Read(encoder_angles, ENCODER_ADDR,
        //                           ANGLE_REGISTER)) {
        //             states = STATE_CAN_RECEIVE;
        //         } else {
        //             states = STATE_CAN_TRANSMIT;
        //         }
        //         break;
        //     case STATE_CAN_TRANSMIT:
        //         messageID = 0x469;
        //         CAN0_TxCallbackRegister(APP_CAN_Callback,
        //                                 (uintptr_t)STATE_CAN_TRANSMIT);
        //         states = STATE_IDLE;
        //         if (CAN0_MessageTransmit(
        //                 messageID, 6, encoder_angles,
        //                 CAN_MODE_FD_WITHOUT_BRS,
        //                 CAN_MSG_ATTR_TX_FIFO_DATA_FRAME) == false) {
        //         }
        //         break;
        //
        //     case STATE_CAN_XFER_ERROR:
        //         // if ((STATES)xferContext == STATE_CAN_RECEIVE) {
        //         // } else {
        //         // }
        //         states = STATE_CAN_RECEIVE;
        //         break;
        //
        //     case STATE_CAN_XFER_SUCCESSFUL:
        //         if ((STATES)xferContext == STATE_CAN_RECEIVE) {
        //             states = STATE_SET_PWM;
        //         } else if ((STATES)xferContext == STATE_CAN_TRANSMIT) {
        //             states = STATE_CAN_RECEIVE;
        //         }
        //         break;
        //
        //     default:
        //         break;
        // }
    }

    /* Execution should not come here during normal operation */
    return 0;
}
