#include "state_machine.h"
#include <stdint.h>
#include <stdio.h>
#include "can1.h"
#include "usart.h"
#include "can_common.h"
#include "dma.h"

static uint8_t encoder_num = 0;
static bool read_failed = true;


void state_machine(struct state_context* ctx) {
    uint32_t ev = ctx->events;
    ctx->events &= ~ev;

    if (ev & EVENT_SET_PWM) {
        set_servos_pwm(ctx->rx_frame.buf, NUM_ENCODERS);
        WDT_Clear();
    }

    if (ev & EVENT_READ_ENCODER) {

        ctx->tx_frame.id = CAN_SEND_ANGLES;
        ctx->tx_frame.len = 6;
        read_encoders(ANGLE_REGISTER, encoder_num, ctx->tx_frame.buf);

    }


    if (read_failed && (encoder_num != 0)) {
        uint8_t prev_enc = encoder_num - 1;
        ctx->tx_frame.buf[2*prev_enc] = 0xFF;
        ctx->tx_frame.buf[2*prev_enc + 1] = 0xFF;
    }

    if (ev & EVENT_TRANSMIT_ANGLES || (encoder_num == NUM_ENCODERS)) {
        can_transmit(&ctx->tx_frame);
        encoder_num = 0;
    }

    can_recieve(&ctx->rx_frame);
}

void can_rx_callback(uintptr_t context) {
    struct state_context* ctx = (struct state_context*)context;

    // print_can_frame(ctx->rx_frame.id, ctx->rx_frame.len, ctx->rx_frame.timestamp, ctx->rx_frame.buf);
    // CAN_ERROR err = CAN0_ErrorGet();
    // if (err) {
    //     return;
    // }
    switch (ctx->rx_frame.id) {
        case STOP_GRIPPER:
            stop_gripper();
            break;
        case START_GRIPPER:
            start_gripper();
            break;
        case SET_PWM:
            ctx->events |= EVENT_SET_PWM;
            break;
        case RESET_MCU:
            NVIC_SystemReset();
            break;
        default:
            break;
    }
}

void tc0_callback(TC_TIMER_STATUS status, uintptr_t context) {
    volatile uint32_t* events = (volatile uint32_t*)context;
    *events |= EVENT_READ_ENCODER;
}

void tc1_callback(TC_TIMER_STATUS status, uintptr_t context) {
    volatile uint32_t* events = (volatile uint32_t*)context;
    *events |= EVENT_TRANSMIT_ANGLES;
}

void i2c1_callback(uintptr_t context) {
    SERCOM_I2C_ERROR err = SERCOM1_I2C_ErrorGet();

    if (err == SERCOM_I2C_ERROR_NONE){
        read_failed = false;
    } else {
        read_failed = true;
    }

    encoder_num += 1;

    volatile uint32_t* events = (volatile uint32_t*)context;


    if (encoder_num == 3) {
        *events |= EVENT_TRANSMIT_ANGLES;
        return;
    }

    *events |= EVENT_READ_ENCODER;
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
