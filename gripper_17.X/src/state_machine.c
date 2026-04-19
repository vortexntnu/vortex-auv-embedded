#include "state_machine.h"
#include <stdint.h>
#include <stdio.h>
#include "can1.h"
#include "can_common.h"
#include "dma.h"
#include "gripper.h"
#include "system_init.h"
#include "usart.h"

static uint8_t encoder_num = 0;
static volatile bool read_failed = true;
static uint8_t encoder_rx_buf[2] = {0};
static uint8_t encoder_reg = ANGLE_REGISTER;
static uint8_t raw_encoder_angles[2 * NUM_ENCODERS] = {0};
static volatile bool can_tx_avaliable = false;

struct state_context ctx;

void state_machine_init() {
    can_recieve(&ctx.rx_frame);
}

void state_machine() {
    uint32_t ev = ctx.events;
    ctx.events &= ~ev;

    if (ev & EVENT_SET_PWM) {
        printf("EVENT_SET_PWM\r\n");
        WDT_Clear();
        if (set_servos_pwm(ctx.rx_frame.buf, 4)) {
            struct can_tx_frame tx;
            tx.id = 0x46B;
            tx.len = 1;
            tx.buf[0] = 1;
            can_transmit(&tx);
        }
    }

    if (ev & EVENT_READ_ENCODER_START) {
        start_encoder_read(&encoder_reg, encoder_num,
                           encoder_rx_buf);  // kicks off async I2C
    }

    if (ev & EVENT_READ_ENCODER_DONE) {
        if (read_failed) {
            raw_encoder_angles[2 * encoder_num] = 0xFF;
            raw_encoder_angles[2 * encoder_num + 1] = 0xFF;
        } else {
            uint16_t angle =
                ((uint16_t)encoder_rx_buf[0] << 6) | (encoder_rx_buf[1] & 0x3F);

            raw_encoder_angles[2 * encoder_num] = (uint8_t)(angle & 0xFF);
            raw_encoder_angles[2 * encoder_num + 1] = (uint8_t)(angle >> 8);
        }

        encoder_num++;

        if (encoder_num == NUM_ENCODERS) {
            ctx.events |= EVENT_TRANSMIT_ANGLES;
        } else {
            ctx.events |= EVENT_READ_ENCODER_START;
        }
    }

    if (ev & EVENT_TRANSMIT_ANGLES) {
        ctx.tx_frame.id = CAN_SEND_ANGLES;
        ctx.tx_frame.len = 2 * NUM_ENCODERS;

        for (int i = 0; i < 2 * NUM_ENCODERS; i++) {
            ctx.tx_frame.buf[i] = raw_encoder_angles[i];
        }

        if (can_tx_avaliable){
            can_transmit(&ctx.tx_frame);
        }

        encoder_num = 0;
    }
    can_recieve(&ctx.rx_frame);
    // PM_IdleModeEnter();
}

void can_rx_callback(uintptr_t context) {
    printf("Entering can RX callback\r\n");
    // print_can_frame(ctx.rx_frame.id, ctx.rx_frame.len,
    // ctx.rx_frame.timestamp, ctx.rx_frame.buf); CAN_ERROR err =
    // CAN0_ErrorGet(); if (err) {
    //     return;
    // }
    can_tx_avaliable = true;
    switch (ctx.rx_frame.id) {
        case STOP_GRIPPER:
            stop_gripper();
            break;
        case START_GRIPPER:
            start_gripper();
            break;
        case SET_PWM:
            ctx.events |= EVENT_SET_PWM;
            break;
        case RESET_MCU:
            NVIC_SystemReset();
            break;
        default:
            break;
    }
}

void tc0_callback(TC_TIMER_STATUS status, uintptr_t context) {
    ctx.events |= EVENT_READ_ENCODER_START;
}

void tc1_callback(TC_TIMER_STATUS status, uintptr_t context) {
    ctx.events |= EVENT_TRANSMIT_ANGLES;
}

void i2c1_callback(uintptr_t context) {
    SERCOM_I2C_ERROR err = SERCOM1_I2C_ErrorGet();
    read_failed = (err != SERCOM_I2C_ERROR_NONE);

    ctx.events |= EVENT_READ_ENCODER_DONE;
}

void dmac_channel0_callback(DMAC_TRANSFER_EVENT returned_evnt,
                            uintptr_t MyDmacContext) {
    printf("entering dmac callback\r\n");
    uint16_t* adc_results = (uint16_t*)MyDmacContext;
    static uint8_t servo = SERVO_1;

    DMAC_ChannelTransfer(DMAC_CHANNEL_0, (const void*)&ADC0_REGS->ADC_RESULT,
                         (const void*)adc_results, sizeof(*adc_results));

    if (DMAC_TRANSFER_EVENT_ERROR == returned_evnt) {
        return;
    }

    bool overCurrent = false;
    uint16_t input_voltage = 0;
    for (size_t sample = 0; sample < TRANSFER_SIZE; sample++) {
        input_voltage += adc_results[sample];

        // 2.5 V == 0 A
        /*input_voltage =*/
        /*    (float)(adc_result_array[sample] * ADC_VREF / 4095U - 2.5)
         * /*/
        /*    0.4;*/

        printf(
            "ADC Count = 0x%03x, ADC Input Current = %d.%03d A "
            "\n\r",
            adc_results[sample], (int)input_voltage,
            (int)((input_voltage - (int)input_voltage) * 100.0));
    }
    input_voltage = input_voltage / TRANSFER_SIZE;

    if (input_voltage > VOLTAGE_THRESHOLD) {
        overCurrent = true;
    }
    switch (servo) {
        case SERVO_1:
            if (overCurrent) {
                PORT_REGS->GROUP[0].PORT_OUTCLR |= (1 << 27);
            }
            ADC0_REGS->ADC_INPUTCTRL = ADC_POSINPUT_AIN1;
            servo = SERVO_2;
            break;
        case SERVO_2:
            if (overCurrent) {
                PORT_REGS->GROUP[0].PORT_OUTCLR |= (1 << 28);
            }
            ADC0_REGS->ADC_INPUTCTRL = ADC_POSINPUT_AIN4;
            servo = SERVO_3;
            break;
        case SERVO_3:
            if (overCurrent) {
                PORT_REGS->GROUP[0].PORT_OUTCLR |= (1 << 0);
            }
            ADC0_REGS->ADC_INPUTCTRL = ADC_POSINPUT_AIN0;
            servo = SERVO_1;
            break;
        default:
            break;
    }
}
