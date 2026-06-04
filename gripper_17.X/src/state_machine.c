#include "state_machine.h"
#include <stdint.h>
#include <stdio.h>
#include "can1.h"
#include "can_common.h"
#include "dma.h"
#include "gripper.h"
#include "system_init.h"
#include "usart.h"
#include "uart_protocol.h"

static uint8_t encoder_num = 0;
static volatile bool read_failed = true;
static uint8_t encoder_rx_buf[4] = {0};
static uint8_t encoder_reg = ANGLE_REGISTER;
static uint8_t raw_encoder_angles[2 * NUM_ENCODERS] = {0};
static volatile bool can_tx_avaliable = false;
static uint8_t servo = SERVO_1;
static uint16_t input_voltage = 0;
static bool adc_ready = false;
static bool gripper_on = false;

struct state_context ctx;

void state_machine_init() {
    can_recieve(&ctx.rx_frame);
}

void state_machine(void)
{
    uint32_t ev = ctx.events;
    ctx.events &= ~ev;

    if (ev & EVENT_SET_PWM)
    {
        WDT_Clear();

        /*
         * For serial, the received data should now come from the UART packet,
         * not ctx.rx_frame.buf. Ideally this event stores the latest UART payload
         * somewhere like ctx.rx_payload.
         */
        if (set_servos_pwm(ctx.rx_frame.buf, 4))
        {
            uint8_t ack = 1;
            uart_proto_send_packet(SET_PWM, &ack, 1);
        }
    }

    if (ev & EVENT_READ_ENCODER_START)
    {
        start_encoder_read(&encoder_reg,
                           encoder_num,
                           encoder_rx_buf + 2 * encoder_num);
    }

    if (ev & EVENT_READ_ENCODER_DONE)
    {
        if (read_failed)
        {
            raw_encoder_angles[2 * encoder_num] = 0xFF;
            raw_encoder_angles[2 * encoder_num + 1] = 0xFF;
        }
        else
        {
            uint16_t angle =
                ((uint16_t)encoder_rx_buf[0 + 2 * encoder_num] << 6) |
                (encoder_rx_buf[1 + 2 * encoder_num] & 0x3F);

            raw_encoder_angles[2 * encoder_num] =
                (uint8_t)(angle & 0xFF);

            raw_encoder_angles[2 * encoder_num + 1] =
                (uint8_t)((angle >> 8) & 0xFF);
        }

        encoder_num++;

        if (encoder_num == NUM_ENCODERS)
        {
            ctx.events |= EVENT_TRANSMIT_ANGLES;
        }
        else
        {
            ctx.events |= EVENT_READ_ENCODER_START;
        }
    }

    if (ev & EVENT_TRANSMIT_ANGLES)
    {
        if (gripper_on)
        {
            uart_proto_send_packet(
                CAN_SEND_ANGLES,
                raw_encoder_angles,
                2 * NUM_ENCODERS);
        }

        encoder_num = 0;
    }

    if (adc_ready)
    {
        uint8_t voltage_payload[4];

        voltage_payload[0] = servo;
        voltage_payload[1] = servo;
        voltage_payload[2] = (uint8_t)(input_voltage & 0xFF);
        voltage_payload[3] = (uint8_t)((input_voltage >> 8) & 0xFF);

        uart_proto_send_packet(
            CAN_SEND_VOLTAGE,
            voltage_payload,
            sizeof(voltage_payload));

        adc_ready = false;
    }

    /*
     * Remove this:
     *
     * can_recieve(&ctx.rx_frame);
     *
     * UART receive is handled by uart_proto_init()
     * and uart_proto_pop_packet().
     */
}


void uart_gripper_task(void)
{
    uart_packet_t packet;

    while (uart_proto_pop_packet(&packet))
    {
        can_tx_avaliable = true;  // Rename this later, but okay for now.

        switch (packet.id)
        {
            case STOP_GRIPPER:
                stop_gripper();
                gripper_on = false;
                break;

            case START_GRIPPER:
                start_gripper();
                gripper_on = true;
                break;

            case SET_PWM:
                set_servos_pwm(packet.payload, packet.len);
                break;

            case RESET_MCU:
                NVIC_SystemReset();
                break;

            default:
                break;
        }
    }
}

void can_rx_callback(uintptr_t context) {
    // printf("Entering can RX callback\r\n");
    // print_can_frame(ctx.rx_frame.id, ctx.rx_frame.len,
    // ctx.rx_frame.timestamp, ctx.rx_frame.buf); CAN_ERROR err =
    // CAN0_ErrorGet(); if (err) {
    //     return;
    // }
    can_tx_avaliable = true;
    switch (ctx.rx_frame.id) {
        case STOP_GRIPPER:
            stop_gripper();
            gripper_on = false;
            break;
        case START_GRIPPER:
            start_gripper();
            gripper_on = true;
            // ctx.events |= EVENT_READ_ENCODER_START;
            break;
        case SET_PWM:
            // ctx.events |= EVENT_SET_PWM;
            set_servos_pwm(ctx.rx_frame.buf, 4);
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
    // printf("entering dmac callback\r\n");
    uint16_t* adc_results = (uint16_t*)MyDmacContext;

    DMAC_ChannelTransfer(DMAC_CHANNEL_0, (const void*)&ADC0_REGS->ADC_RESULT,
                         (const void*)adc_results, sizeof(*adc_results));

    if (DMAC_TRANSFER_EVENT_ERROR == returned_evnt) {
        return;
    }

    bool overCurrent = false;
    for (size_t sample = 0; sample < TRANSFER_SIZE; sample++) {
        input_voltage += adc_results[sample];

        // 2.5 V == 0 A
        /*input_voltage =*/
        /*    (float)(adc_result_array[sample] * ADC_VREF / 4095U - 2.5)
         * /*/
        /*    0.4;*/

        // printf(
        //     "ADC Count = 0x%03x, ADC Input Current = %d.%03d A "
        //     "\n\r",
        //     adc_results[sample], (int)input_voltage,
        //     (int)((input_voltage - (int)input_voltage) * 100.0));
    }
    input_voltage = input_voltage / TRANSFER_SIZE;
    adc_ready = true;

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
