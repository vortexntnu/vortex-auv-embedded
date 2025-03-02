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
#include "can1.h"
#include "can_common.h"
#include "clock.h"
#include "i2c.h"  // I2C client backup
// #include "i2c_master.h" // not used
#include "adc0.h"
#include "dma.h"
#include "pm.h"
#include "rtc.h"
#include "sam.h"
#include "samc21e17a.h"
#include "sercom0_i2c.h"
#include "sercom1_i2c.h"
// #include "sercom3_i2c.h"
#include "system_init.h"
#include "tcc.h"
#include "tcc0.h"
#include "tcc_common.h"
#include "usart.h"
#include "wdt.h"

// Encoder
#define SHOULDER_ADDR 0x40
#define WRIST_ADDR 0x41
#define GRIP_ADDR 0x42
#define ANGLE_REGISTER 0xFE

#define TRANSFER_SIZE 16
#define ADC_VREF 5.0f
#define CURRENT_TRESHOLD 1.0f  // 1 A
#define RTC_COMPARE_VAL 100    // should probably be set lower

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

// CAN FD RECIEVE ID table
typedef enum {
    STOP_GRIPPER = 0x469,
    START_GRIPPER,
    SET_PWM,
    RESET_MCU,
} CAN_RECEIVE_ID;
// I2C RECIEVE START BYTE table
typedef enum {
    I2C_SET_PWM,
    I2C_STOP_GRIPPER,
    I2C_START_GRIPPER,
    I2C_RESET_MCU,
} I2C_STARTBYTE_ID;
/* Servo pins enum for ADC reading */
typedef enum {
    SERVO_1,
    SERVO_2,
    SERVO_3,
} SERVO_ADC_PINS;

/* Variable to save Tx/Rx transfer status and context */
static uint32_t status = 0;
static uint32_t xferContext = 0;
/* Variable to save Tx/Rx message */
static uint32_t messageID = 0x469;
static uint32_t rx_messageID = 0x469;
// static uint8_t message[64] = {0};
static uint8_t rx_message[64] = {0};
// static uint8_t messageLength = 0;
static uint8_t rx_messageLength = 0;
static uint16_t timestamp = 0;
static CAN_MSG_RX_FRAME_ATTRIBUTE msgFrameAttr = CAN_MSG_RX_DATA_FRAME;

// ADC Variables
volatile SERVO_ADC_PINS servo_status = SERVO_1;
uint32_t myAppObj = 0;
uint16_t adc_result_array[TRANSFER_SIZE];
float input_voltage;

/* Variable to save application state */
// volatile static STATES states = STATE_CAN_RECEIVE;

static uint8_t encoder_angles[6] = {0};

// Reads the encoder angle Register
// 2 bytes for each encoder
// 2|2|2 SHOULDER, WRIST, GRIP
// Watchdog timer will cause reset
// if stuck in while loop
uint8_t Encoder_Read(uint8_t* data, uint8_t reg) {
    uint16_t rawData[3] = {0};
    uint8_t dataBuffer[2] = {0};

    // Starting Watchdog timer
    WDT_Enable();
    // SHOULDER
    if (!SERCOM1_I2C_WriteRead(SHOULDER_ADDR, &reg, 1, dataBuffer, 2)) {
        WDT_Disable();
        return 0;
    }
    while (SERCOM1_I2C_IsBusy())
        ;
    rawData[0] = (dataBuffer[0] << 6) | (dataBuffer[1] & 0x3F);
    printf("%d\n", rawData[0]);

    // Clearing watchdog timer
    // WDT_Clear();
    // // WRIST
    //
    // if (!SERCOM1_I2C_WriteRead(WRIST_ADDR, &reg, 1, dataBuffer, 2)) {
    //     WDT_Disable();
    //     return 0;
    // }
    // while (SERCOM1_I2C_IsBusy())
    //     ;
    //
    // rawData[1] = (dataBuffer[0] << 6) | (dataBuffer[1] & 0x3F);
    //
    // // Clearing watchdog timer
    // WDT_Clear();
    //
    // // GRIP
    //
    // if (!SERCOM1_I2C_WriteRead(GRIP_ADDR, &reg, 1, dataBuffer, 2)) {
    //     WDT_Disable();
    //     return 0;
    // }
    //
    // while (SERCOM1_I2C_IsBusy())
    //     ;
    // rawData[2] = (dataBuffer[0] << 6) | (dataBuffer[1] & 0x3F);
    //
    data[0] = rawData[0] >> 8;
    data[1] = rawData[0] & 0xFF;
    data[2] = rawData[1] >> 8;
    data[3] = rawData[1] & 0xFF;
    data[4] = rawData[2] >> 8;
    data[5] = rawData[2] & 0xFF;

    // Stopping watchdog timer
    WDT_Disable();
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

    if (tccValue > PWM_MAX) {
        TCC0_PWM24bitDutySet(1, PWM_MAX);
    } else if (tccValue < PWM_MIN) {
        TCC0_PWM24bitDutySet(1, PWM_MIN);
    } else {
        TCC0_PWM24bitDutySet(1, tccValue);
    }

    // WRIST
    tccValue = (wristDuty * (TCC_PERIOD + 1)) / PWM_PERIOD_MICROSECONDS;
    if (tccValue > PWM_MAX) {
        TCC0_PWM24bitDutySet(0, PWM_MAX);
    } else if (tccValue < PWM_MIN) {
        TCC0_PWM24bitDutySet(0, PWM_MIN);
    } else {
        TCC0_PWM24bitDutySet(0, tccValue);
    }

    // GRIP
    tccValue = (gripDuty * (TCC_PERIOD + 1)) / PWM_PERIOD_MICROSECONDS;
    if (tccValue > PWM_MAX) {
        TCC1_PWM24bitDutySet(1, PWM_MAX);
    } else if (tccValue < PWM_MIN) {
        TCC1_PWM24bitDutySet(1, PWM_MIN);
    } else {
        TCC1_PWM24bitDutySet(1, tccValue);
    }
}

// I2C slave backup code
// Used instead of CAN
bool SERCOM_I2C_Callback(SERCOM_I2C_SLAVE_TRANSFER_EVENT event,
                         uintptr_t contextHandle) {
    static uint8_t dataBuffer[7];
    // Data sent recieved i2c will have this format
    // Startbyte|PWM_DATA|
    // Startbyte indiciate what information is being sent

    static uint8_t dataIndex = 0;

    switch (event) {
        case SERCOM_I2C_SLAVE_TRANSFER_EVENT_ADDR_MATCH:
            dataIndex = 0;
            break;

        case SERCOM_I2C_SLAVE_TRANSFER_EVENT_RX_READY:
            /* Read the data sent by I2C Host */

            // Encoder_Read(encoder_angles, ENCODER_ADDR,
            // ANGLE_REGISTER);
            if (dataIndex < sizeof(dataBuffer)) {
                dataBuffer[dataIndex++] = SERCOM3_I2C_ReadByte();
            }

            break;

        case SERCOM_I2C_SLAVE_TRANSFER_EVENT_TX_READY: {
            SERCOM3_I2C_WriteByte(encoder_angles[dataIndex++]);
            break;
        }

        case SERCOM_I2C_SLAVE_TRANSFER_EVENT_STOP_BIT_RECEIVED:
            if (SERCOM3_I2C_TransferDirGet() ==
                SERCOM_I2C_SLAVE_TRANSFER_DIR_WRITE) {
                // First byte indicating what the MCU should do
                switch (dataBuffer[0]) {
                    case I2C_SET_PWM:
                        // Start indexing at the second element
                        SetPWMDutyCycle(dataBuffer + 1);
                        break;
                    case I2C_STOP_GRIPPER:
                        // Turn off sevo enable pin
                        PORT_REGS->GROUP[0].PORT_OUTCLR =
                            (1 << 0) | (1 << 27) | (1 << 28);
                        TCC0_PWMStop();
                        TCC1_PWMStop();
                        ADC0_Disable();
                        PM_IdleModeEnter();
                        break;
                    case I2C_START_GRIPPER:
                        TCC0_PWMStart();
                        TCC1_PWMStart();
                        ADC0_Enable();
                        RTC_Timer32Start();
                        RTC_Timer32CompareSet(RTC_COMPARE_VAL);
                        // Turn on servo enable pin
                        PORT_REGS->GROUP[0].PORT_OUTSET =
                            (1 << 0) | (1 << 27) | (1 << 28);
                        break;
                    case I2C_RESET_MCU:
                        WDT_REGS->WDT_CLEAR = 0x0;
                    default:
                        break;
                }
                // printf("MEssage recieved\n");
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

// Callback function called during CAN interrupt.
// When a recieve interrupt is triggered it will
// set the PWM duty cycle with the data it has recieved
// it will then read the encoders and send the data
// after a successfull transmit reviece interrupts will
// be reenabled
void CAN_Recieve_Callback(uintptr_t context) {
    xferContext = context;

    /* Check CAN Status */
    status = CAN0_ErrorGet();
    printf("Entering callback\n");

    if (((status & CAN_PSR_LEC_Msk) == CAN_ERROR_NONE) ||
        ((status & CAN_PSR_LEC_Msk) == CAN_ERROR_LEC_NC)) {
        switch (rx_messageID) {
            case STOP_GRIPPER:
                // Turn off servo enable pins
                PORT_REGS->GROUP[0].PORT_OUTCLR =
                    (1 << 0) | (1 << 27) | (1 << 28);
                TCC0_PWMStop();
                TCC1_PWMStop();
                ADC0_Disable();
                CAN0_MessageReceive(&rx_messageID, &rx_messageLength,
                                    rx_message, &timestamp,
                                    CAN_MSG_ATTR_RX_FIFO0, &msgFrameAttr);
                printf("STOP_GRIPPER\n");
                // PM_IdleModeEnter();
                break;
            case START_GRIPPER:
                TCC0_PWMStart();
                TCC1_PWMStart();
                ADC0_Enable();
                RTC_Timer32Start();
                RTC_Timer32CompareSet(RTC_COMPARE_VAL);
                // Turn on servo enable pins
                PORT_REGS->GROUP[0].PORT_OUTSET =
                    (1 << 0) | (1 << 27) | (1 << 28);
                printf("START_GRIPPER\n");
                memset(rx_message, 0x00, sizeof(rx_message));
                CAN0_MessageReceive(&rx_messageID, &rx_messageLength,
                                    rx_message, &timestamp,
                                    CAN_MSG_ATTR_RX_FIFO0, &msgFrameAttr);
                break;
            case SET_PWM:
                SetPWMDutyCycle(rx_message);
                // Reading encoders
                if (!Encoder_Read(encoder_angles, ANGLE_REGISTER)) {
                    // Returning to CAN recieve state if read failed
                    memset(rx_message, 0x00, sizeof(rx_message));
                    CAN0_MessageReceive(&rx_messageID, &rx_messageLength,
                                        rx_message, &timestamp,
                                        CAN_MSG_ATTR_RX_FIFO0, &msgFrameAttr);
                    break;
                }
                // Sending ENCODER data over CAN
                if (CAN0_MessageTransmit(
                        messageID, 6, encoder_angles, CAN_MODE_FD_WITHOUT_BRS,
                        CAN_MSG_ATTR_TX_FIFO_DATA_FRAME) == false) {
                }
                printf("SET_PWM");

                break;
            case RESET_MCU:
                // This will cause an immediate system reset
                // Writing anything other than 0xA5 to WDT_CLEAR will
                // reset the device
                printf("RESET_MCU\n");
                WDT_REGS->WDT_CLEAR = 0x0;
                break;
            default:
                break;
        }

        printf(" New Message Received\r\n");
        uint8_t length = rx_messageLength;
        printf(
            " Message - Timestamp : 0x%x ID : 0x%x Length "
            ":0x%x",
            (unsigned int)timestamp, (unsigned int)rx_messageID,
            (unsigned int)rx_messageLength);
        printf("Message : ");
        while (length) {
            printf("0x%x ", rx_message[rx_messageLength - length--]);
        }
        // Only include this if testing without encoders
        // if (CAN0_MessageReceive(&rx_messageID,
        // &rx_messageLength,
        //                         rx_message, &timestamp,
        //                         CAN_MSG_ATTR_RX_FIFO0,
        //                         &msgFrameAttr) == false) {
        // }
        // if (!Encoder_Read(encoder_angles,
        //                   ANGLE_REGISTER)) {
        //     CAN0_MessageReceive(&rx_messageID, &rx_messageLength,
        //                         rx_message, &timestamp,
        //                         CAN_MSG_ATTR_RX_FIFO0,
        //                         &msgFrameAttr);
        //
        //     break;
        // }
        // if (CAN0_MessageTransmit(messageID, 6, encoder_angles,
        //                          CAN_MODE_FD_WITHOUT_BRS,
        //                          CAN_MSG_ATTR_TX_FIFO_DATA_FRAME) == false) {
        // }
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

// Only used for testing SERVOS
void TCC_PeriodEventHandler(uint32_t status, uintptr_t context) {
    /* duty cycle values */
    static int8_t increment1 = 10;
    static uint32_t duty1 = 0;
    static uint32_t duty2 = 0;
    static uint32_t duty3 = 0U;

    TCC0_PWM24bitDutySet(1, duty1);
    TCC1_PWM24bitDutySet(0, duty2);
    TCC1_PWM24bitDutySet(1, duty3);

    /* Increment duty cycle values */
    duty1 += increment1;
    duty2 += increment1;
    duty3 += increment1;

    if (duty1 > PWM_MAX) {
        duty1 = PWM_MAX;
        increment1 *= -1;
    } else if (duty2 < PWM_MIN) {
        duty1 = PWM_MIN;
        increment1 *= -1;
    }
    if (duty2 > PWM_MAX) {
        duty2 = PWM_MAX;
        increment1 *= -1;
    } else if (duty2 < PWM_MIN) {
        duty2 = PWM_MIN;
        increment1 *= -1;
    }
    if (duty3 > PWM_MAX) {
        duty3 = PWM_MAX;
        increment1 *= -1;
    } else if (duty3 < PWM_MIN) {
        duty3 = PWM_MIN;
        increment1 *= -1;
    }
}

// Will turn off servo enable pin if current is to high
void DmacCh0Cb(DMAC_TRANSFER_EVENT returned_evnt, uintptr_t MyDmacContext) {
    if (DMAC_TRANSFER_EVENT_COMPLETE == returned_evnt) {
        bool overCurrent = false;
        for (int sample = 0; sample < TRANSFER_SIZE; sample++) {
            // 2.5 V means 0 Amps
            // TODO: Convert this to average value instead of calculating on
            // runtime
            input_voltage =
                (float)(adc_result_array[sample] * ADC_VREF / 4095U - 2.5) /
                0.4;

            printf(
                "ADC Count = 0x%03x, ADC Input Current = %d.%03d A "
                "\n\r",
                adc_result_array[sample], (int)input_voltage,
                (int)((input_voltage - (int)input_voltage) * 100.0));

            if (input_voltage > CURRENT_TRESHOLD) {
                overCurrent = true;
                break;
            }
        }
        switch (servo_status) {
            case SERVO_1:
                if (overCurrent) {
                    PORT_REGS->GROUP[0].PORT_OUTCLR |= (1 << 27);
                }
                ADC0_REGS->ADC_INPUTCTRL = ADC_POSINPUT_AIN1;
                servo_status = SERVO_2;
                printf("SERVO2\n");
                break;
            case SERVO_2:
                if (overCurrent) {
                    PORT_REGS->GROUP[0].PORT_OUTCLR |= (1 << 28);
                }
                ADC0_REGS->ADC_INPUTCTRL = ADC_POSINPUT_AIN4;
                servo_status = SERVO_3;
                printf("SERVO3\n");
                break;
            case SERVO_3:
                if (overCurrent) {
                    PORT_REGS->GROUP[0].PORT_OUTCLR |= (1 << 0);
                }
                ADC0_REGS->ADC_INPUTCTRL = ADC_POSINPUT_AIN0;
                servo_status = SERVO_1;
                printf("SERVO1\n");
                break;
            default:
                break;
        }

        DMAC_ChannelTransfer(
            DMAC_CHANNEL_0, (const void*)&ADC0_REGS->ADC_RESULT,
            (const void*)adc_result_array, sizeof(adc_result_array));
    } else if (DMAC_TRANSFER_EVENT_ERROR == returned_evnt) {
    }
}

int main(void) {
    /* Initialize all modules */
    NVMCTRL_REGS->NVMCTRL_CTRLB = NVMCTRL_CTRLB_RWS(3);
    PM_Initialize();
    PIN_Initialize();
    CLOCK_Initialize();
    NVMCTRL_Initialize();
    TCC1_PWMInitialize();
    TCC0_PWMInitialize();
    CAN0_Initialize();
    // SERCOM0_I2C_Initialize();  // I2C 3
    SERCOM1_I2C_Initialize();  // I2C 2
    // SERCOM3_I2C_Initialize();    // I2C 1

    SERCOM0_USART_Initialize();  // USART for Debugging

    // SERCOM3_SLAVE_I2C_Initialize();

    EVSYS_Initialize();
    ADC0_Initialize();
    DMAC_Initialize();
    RTC_Initialize();

    NVIC_Initialize();  // Enable interrupts
    /* Start PWM*/

    // Peripherals should be disabled by default and will be enabled
    // by a CAN or I2C message
    // Enable if testing without CAN or I2C
    // TCC1_PWMStart();
    // TCC0_PWMStart();
    // ADC0_Enable();
    // RTC_Timer32Start();
    // RTC_Timer32CompareSet(RTC_COMPARE_VAL);
    //

    CAN0_MessageRAMConfigSet(Can0MessageRAM);

    // Callback functions

    // I2C backup callback function
    // SERCOM3_I2C_CallbackRegister(SERCOM_I2C_Callback, 0);

    // Callback function used for TCC interrupts when testing
    // TCC0_PWMCallbackRegister(TCC_PeriodEventHandler, (uintptr_t)NULL);

    DMAC_ChannelCallbackRegister(DMAC_CHANNEL_0, DmacCh0Cb,
                                 (uintptr_t)&myAppObj);
    CAN0_RxCallbackRegister(CAN_Recieve_Callback, (uintptr_t)STATE_CAN_RECEIVE,
                            CAN_MSG_ATTR_RX_FIFO0);
    CAN0_TxCallbackRegister(CAN_Transmit_Callback,
                            (uintptr_t)STATE_CAN_TRANSMIT);

    memset(rx_message, 0x00, sizeof(rx_message));
    if (CAN0_MessageReceive(&rx_messageID, &rx_messageLength, rx_message,
                            &timestamp, CAN_MSG_ATTR_RX_FIFO0,
                            &msgFrameAttr) == false) {
    }
    printf("Before transmit\n");

    if (CAN0_MessageTransmit(messageID, 6, encoder_angles,
                             CAN_MODE_FD_WITHOUT_BRS,
                             CAN_MSG_ATTR_TX_FIFO_DATA_FRAME) == false) {
    }

    // Servo Enable
    PORT_REGS->GROUP[0].PORT_OUTSET = (1 << 0) | (1 << 27) | (1 << 28);

    printf("Initialize complete\n");
    DMAC_ChannelTransfer(DMAC_CHANNEL_0, (const void*)&ADC0_REGS->ADC_RESULT,
                         (const void*)adc_result_array,
                         sizeof(adc_result_array));
    PM_IdleModeEnter();
    while (true) {
    }

    /* Execution should not come here during normal operation */
    return 0;
}
