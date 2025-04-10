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
#include "adc0.h"
#include "can1.h"
#include "can_common.h"
#include "clock.h"
#include "dma.h"
#include "i2c.h"  // I2C client backup
#include "pm.h"
#include "rtc.h"
#include "sam.h"
#include "samc21e17a.h"
/*#include "sercom0_i2c.h"*/
#include "sercom1_i2c.h"  // Encoder i2c
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
#define I2C_TIMEOUT 100000
#define NUM_ENCODERS 3

#define TRANSFER_SIZE 16
#define ADC_VREF 5.0f
#define CURRENT_TRESHOLD 2.7f  // 1 A
#define VOLTAGE_THRESHOLD 2048
#define RTC_COMPARE_VAL 50

uint8_t Can0MessageRAM[CAN0_MESSAGE_RAM_CONFIG_SIZE]
    __attribute__((aligned(32)));

/* Application's state machine enum */
typedef enum {
    STATE_IDLE,
    STATE_GRIPPER_ACTIVE,
} STATES;

typedef enum {
    STATE_CAN_RECEIVE,
    STATE_CAN_TRANSMIT,
} CAN_STATES;

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

/*CAN*/
static uint32_t status = 0;
static uint32_t xferContext = 0;
static uint32_t messageID = 0x469;
static uint32_t rx_messageID = 0;
static uint8_t rx_message[64] = {0};
static uint8_t rx_messageLength = 0;
static uint16_t timestamp = 0;
static CAN_MSG_RX_FRAME_ATTRIBUTE msgFrameAttr = CAN_MSG_RX_DATA_FRAME;

// ADC Variables
volatile SERVO_ADC_PINS servo_status = SERVO_1;
uint32_t myAppObj = 0;
uint16_t adc_result_array[TRANSFER_SIZE];

volatile static STATES gripper_state = STATE_IDLE;
static uint8_t encoder_angles[7] = {0};

static uint8_t encoder_read(uint8_t* data, uint8_t reg);
static void set_pwm_dutycycle(uint8_t* dutyCycleMicroSeconds);


static inline void enable_servos() {
    PORT_REGS->GROUP[0].PORT_OUTSET = (1 << 0) | (1 << 27) | (1 << 28);
}
static inline void disable_servos() {
    PORT_REGS->GROUP[0].PORT_OUTCLR = (1 << 0) | (1 << 27) | (1 << 28);
}

bool SERCOM_I2C_Callback(SERCOM_I2C_SLAVE_TRANSFER_EVENT event,
                         uintptr_t contextHandle);
void CAN_Recieve_Callback(uintptr_t context);
void CAN_Transmit_Callback(uintptr_t context);
void TCC_PeriodEventHandler(uint32_t status, uintptr_t context);
void Dmac_Channel0_Callback(DMAC_TRANSFER_EVENT returned_evnt,
                            uintptr_t MyDmacContext);

int main(void) {
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

    SERCOM0_USART_Initialize();  // USART for Debugging

    SERCOM3_SLAVE_I2C_Initialize();

    EVSYS_Initialize();
    ADC0_Initialize();
    DMAC_Initialize();
    RTC_Initialize();

    NVIC_Initialize();

    /* Peripherals should be disabled by default and will be enabled
     Enable if testing without CAN or I2C */
    // TCC1_PWMStart();
    // TCC0_PWMStart();
    // ADC0_Enable();
    // RTC_Timer32Start();
    // RTC_Timer32CompareSet(RTC_COMPARE_VAL);

    CAN0_MessageRAMConfigSet(Can0MessageRAM);

    // SERCOM3_I2C_CallbackRegister(SERCOM_I2C_Callback, 0);

    // TCC0_PWMCallbackRegister(TCC_PeriodEventHandler, (uintptr_t)NULL);

    DMAC_ChannelCallbackRegister(DMAC_CHANNEL_0, Dmac_Channel0_Callback,
                                 (uintptr_t)&myAppObj);
    CAN0_RxCallbackRegister(CAN_Recieve_Callback, (uintptr_t)STATE_CAN_RECEIVE,
                            CAN_MSG_ATTR_RX_FIFO0);
    CAN0_TxCallbackRegister(CAN_Transmit_Callback,
                            (uintptr_t)STATE_CAN_TRANSMIT);

    memset(rx_message, 0x00, sizeof(rx_message));
    // Enabling CAN recieve interrupt for fifo0
    CAN0_MessageReceive(&rx_messageID, &rx_messageLength, rx_message,
                        &timestamp, CAN_MSG_ATTR_RX_FIFO0, &msgFrameAttr);

    // Servo Enable
    /*PORT_REGS->GROUP[0].PORT_OUTSET = (1 << 0) | (1 << 27) | (1 << 28);*/
    /*printf("Initialize complete\n");*/
    DMAC_ChannelTransfer(DMAC_CHANNEL_0, (const void*)&ADC0_REGS->ADC_RESULT,
                         (const void*)adc_result_array,
                         sizeof(adc_result_array));
    while (true) {
        switch (gripper_state) {
            case STATE_IDLE:
                PM_IdleModeEnter();
                break;
            case STATE_GRIPPER_ACTIVE:
                break;
            default:
                break;
        }
    }

    return EXIT_FAILURE;
}

static uint8_t encoder_read(uint8_t* data, uint8_t reg) {
    const uint8_t encoder_addresses[NUM_ENCODERS] = {SHOULDER_ADDR, WRIST_ADDR,
                                                     GRIP_ADDR};
    uint8_t status = 0;
    uint32_t timeout;
    uint16_t rawData[NUM_ENCODERS] = {0};
    uint8_t dataBuffer[2] = {0};

    for (uint8_t i = 0; i < NUM_ENCODERS; i++) {
        if (!SERCOM1_I2C_WriteRead(encoder_addresses[i], &reg, 1, dataBuffer,
                                   2)) {
            return 1;
        }
        timeout = I2C_TIMEOUT;
        while (SERCOM1_I2C_IsBusy()) {
            if (--timeout == 0) {
                status |= (1 << i);
                break;
            }
        }
        rawData[i] = (dataBuffer[0] << 6) | (dataBuffer[1] & 0x3F);
    }

    for (int i = 1; i < NUM_ENCODERS; i++) {
        data[2 * i] = rawData[i] >> 8;
        data[2 * i + 1] = rawData[i] & 0xFF;
    }
    data[6] = status;

    return 0;
}

static void set_pwm_dutycycle(uint8_t* dutyCycleMicroSeconds) {
    uint16_t shoulderDuty =
        (dutyCycleMicroSeconds[0] << 8) | dutyCycleMicroSeconds[1];
    uint16_t wristDuty =
        (dutyCycleMicroSeconds[2] << 8) | dutyCycleMicroSeconds[3];
    uint16_t gripDuty =
        (dutyCycleMicroSeconds[4] << 8) | dutyCycleMicroSeconds[5];

    uint32_t tccValue =
        (shoulderDuty * (TCC_PERIOD + 1)) / PWM_PERIOD_MICROSECONDS;

    if (tccValue > PWM_MAX) {
        TCC0_PWM24bitDutySet(3, PWM_MAX);
    } else if (tccValue < PWM_MIN) {
        TCC0_PWM24bitDutySet(3, PWM_MIN);
    } else {
        TCC0_PWM24bitDutySet(3, tccValue);
    }

    tccValue = (wristDuty * (TCC_PERIOD + 1)) / PWM_PERIOD_MICROSECONDS;

    if (tccValue > PWM_MAX) {
        TCC1_PWM24bitDutySet(0, PWM_MAX);
    } else if (tccValue < PWM_MIN) {
        TCC1_PWM24bitDutySet(0, PWM_MIN);
    } else {
        TCC1_PWM24bitDutySet(0, tccValue);
    }

    tccValue = (gripDuty * (TCC_PERIOD + 1)) / PWM_PERIOD_MICROSECONDS;

    if (tccValue > PWM_MAX) {
        TCC1_PWM24bitDutySet(1, PWM_MAX);
    } else if (tccValue < PWM_MIN) {
        TCC1_PWM24bitDutySet(1, PWM_MIN);
    } else {
        TCC1_PWM24bitDutySet(1, tccValue);
    }
}


bool SERCOM_I2C_Callback(SERCOM_I2C_SLAVE_TRANSFER_EVENT event,
                         uintptr_t contextHandle) {
    static uint8_t dataBuffer[7];
    static uint8_t dataIndex = 0;

    switch (event) {
        case SERCOM_I2C_SLAVE_TRANSFER_EVENT_ADDR_MATCH:
            dataIndex = 0;
            break;

        case SERCOM_I2C_SLAVE_TRANSFER_EVENT_RX_READY:

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
                uint8_t start_byte = dataBuffer[0];
                switch (start_byte) {
                    case I2C_SET_PWM:
                        set_pwm_dutycycle(dataBuffer + 1);
                        encoder_read(encoder_angles, ANGLE_REGISTER); 
                        WDT_Clear();
                        break;
                    case I2C_STOP_GRIPPER:
                        WDT_Disable();
                        disable_servos();
                        TCC0_PWMStop();
                        TCC1_PWMStop();
                        ADC0_Disable();
                        gripper_state = STATE_IDLE;
                        break;
                    case I2C_START_GRIPPER:
                        TCC0_PWMStart();
                        TCC1_PWMStart();
                        ADC0_Enable();
                        RTC_Timer32Start();
                        RTC_Timer32CompareSet(RTC_COMPARE_VAL);
                        enable_servos();
                        gripper_state = STATE_GRIPPER_ACTIVE;
                        WDT_Enable();
                        break;
                    case I2C_RESET_MCU:
                        WDT_REGS->WDT_CLEAR = 0x0;
                    default:
                        break;
                }
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

    status = CAN0_ErrorGet();

    if (((status & CAN_PSR_LEC_Msk) == CAN_ERROR_NONE) ||
        ((status & CAN_PSR_LEC_Msk) == CAN_ERROR_LEC_NC)) {
        switch (rx_messageID) {
            case STOP_GRIPPER:
                WDT_Disable();

                disable_servos();
                TCC0_PWMStop();
                TCC1_PWMStop();
                ADC0_Disable();
                CAN0_MessageReceive(&rx_messageID, &rx_messageLength,
                                    rx_message, &timestamp,
                                    CAN_MSG_ATTR_RX_FIFO0, &msgFrameAttr);
                gripper_state = STATE_IDLE;

                break;
            case START_GRIPPER:
                TCC0_PWMStart();
                TCC1_PWMStart();
                ADC0_Enable();
                RTC_Timer32Start();
                RTC_Timer32CompareSet(RTC_COMPARE_VAL);

                enable_servos();
                memset(rx_message, 0x00, sizeof(rx_message));
                CAN0_MessageReceive(&rx_messageID, &rx_messageLength,
                                    rx_message, &timestamp,
                                    CAN_MSG_ATTR_RX_FIFO0, &msgFrameAttr);
                gripper_state = STATE_GRIPPER_ACTIVE;
                WDT_Enable();
                break;
            case SET_PWM:
                set_pwm_dutycycle(rx_message);
                if (encoder_read(encoder_angles, ANGLE_REGISTER)) {
                    memset(rx_message, 0x00, sizeof(rx_message));
                    CAN0_MessageReceive(&rx_messageID, &rx_messageLength,
                                        rx_message, &timestamp,
                                        CAN_MSG_ATTR_RX_FIFO0, &msgFrameAttr);
                    break;
                }
                if (CAN0_MessageTransmit(
                        messageID, 6, encoder_angles, CAN_MODE_FD_WITHOUT_BRS,
                        CAN_MSG_ATTR_TX_FIFO_DATA_FRAME) == false) {
                }
                /*printf("SET_PWM");*/
                WDT_Clear();
                break;
            case RESET_MCU:
                WDT_REGS->WDT_CLEAR = 0x0;
                break;
            default:
                break;
        }

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

    status = CAN0_ErrorGet();

    if (((status & CAN_PSR_LEC_Msk) == CAN_ERROR_NONE) ||
        ((status & CAN_PSR_LEC_Msk) == CAN_ERROR_LEC_NC)) {
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

void Dmac_Channel0_Callback(DMAC_TRANSFER_EVENT returned_evnt,
                            uintptr_t MyDmacContext) {
    if (DMAC_TRANSFER_EVENT_COMPLETE == returned_evnt) {
        bool overCurrent = false;
        uint16_t input_voltage = 0;
        for (int sample = 0; sample < TRANSFER_SIZE; sample++) {
            input_voltage += adc_result_array[sample];
            input_voltage = input_voltage >> 1;  // Dividing by two

            // 2.5 V == 0 A
            /*input_voltage =*/
            /*    (float)(adc_result_array[sample] * ADC_VREF / 4095U - 2.5) /*/
            /*    0.4;*/

            /*printf(*/
            /*    "ADC Count = 0x%03x, ADC Input Current = %d.%03d A "*/
            /*    "\n\r",*/
            /*    adc_result_array[sample], (int)input_voltage,*/
            /*    (int)((input_voltage - (int)input_voltage) * 100.0));*/
        }

        if (input_voltage > VOLTAGE_THRESHOLD) {
            overCurrent = true;
        }
        switch (servo_status) {
            case SERVO_1:
                if (overCurrent) {
                    PORT_REGS->GROUP[0].PORT_OUTCLR |= (1 << 27);
                }
                ADC0_REGS->ADC_INPUTCTRL = ADC_POSINPUT_AIN1;
                servo_status = SERVO_2;
                /*printf("SERVO2\n");*/
                break;
            case SERVO_2:
                if (overCurrent) {
                    PORT_REGS->GROUP[0].PORT_OUTCLR |= (1 << 28);
                }
                ADC0_REGS->ADC_INPUTCTRL = ADC_POSINPUT_AIN4;
                servo_status = SERVO_3;
                /*printf("SERVO3\n");*/
                break;
            case SERVO_3:
                if (overCurrent) {
                    PORT_REGS->GROUP[0].PORT_OUTCLR |= (1 << 0);
                }
                ADC0_REGS->ADC_INPUTCTRL = ADC_POSINPUT_AIN0;
                servo_status = SERVO_1;
                /*printf("SERVO1\n");*/
                break;
            default:
                break;
        }

        DMAC_ChannelTransfer(
            DMAC_CHANNEL_0, (const void*)&ADC0_REGS->ADC_RESULT,
            (const void*)adc_result_array, sizeof(adc_result_array));
    } else if (DMAC_TRANSFER_EVENT_ERROR == returned_evnt) {
        DMAC_ChannelTransfer(
            DMAC_CHANNEL_0, (const void*)&ADC0_REGS->ADC_RESULT,
            (const void*)adc_result_array, sizeof(adc_result_array));
    }
}
