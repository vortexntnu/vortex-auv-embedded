#include "system_init.h"
#include "gripper.h"


uint8_t Can0MessageRAM[CAN0_MESSAGE_RAM_CONFIG_SIZE]
    __attribute__((aligned(32)));
static struct can_rx_frame rx_frame;
static uint16_t adc_result_array[TRANSFER_SIZE];
static volatile uint32_t events;



static void callback_init(void);
void can_rx_callback(uintptr_t context);
void dmac_channel0_callback(DMAC_TRANSFER_EVENT returned_evnt,
                            uintptr_t MyDmacContext);
void tc0_callback(TC_TIMER_STATUS status, uintptr_t context);
void tc1_callback(TC_TIMER_STATUS status, uintptr_t context);

int main(void) {
    system_init();

    CAN0_MessageRAMConfigSet(Can0MessageRAM);

    callback_init();


    DMAC_ChannelTransfer(DMAC_CHANNEL_0, (const void*)&ADC0_REGS->ADC_RESULT,
                         (const void*)adc_result_array,
                         sizeof(adc_result_array));

    can_recieve(&rx_frame);


    static struct can_tx_frame angles_tx_frame;

    WDT_Enable();

    while (true) {
        PM_IdleModeEnter();

        uint32_t ev = events;
        events &= ~ev;

        if (ev & EVENT_SET_PWM){
            set_servos_pwm(rx_frame.buf);
            WDT_Clear();
        }

        if (ev & EVENT_READ_ENCODER){
            angles_tx_frame.id = CAN_SEND_ANGLES;
            angles_tx_frame.len = 6;
            read_encoders(ANGLE_REGISTER, angles_tx_frame.buf);
        }

        if (ev & EVENT_TRANSMIT_ANGLES){
            can_transmit(&angles_tx_frame);
        }

        can_recieve(&rx_frame);
    }

    return EXIT_FAILURE;
}


void can_rx_callback(uintptr_t context) {
    if (CAN0_ErrorGet()) {
        return;
    }
    switch (rx_frame.id) {
        case STOP_GRIPPER:
            stop_gripper();
            break;
        case START_GRIPPER:
            start_gripper();
            break;
        case SET_PWM:
            set_servos_pwm(rx_frame.buf);
            WDT_Clear();
            break;
        case RESET_MCU:
            NVIC_SystemReset();
            break;
        default:
            break;
    }
}

void tc0_callback(TC_TIMER_STATUS status, uintptr_t context) {
    events |= EVENT_READ_ENCODER;
}

void tc1_callback(TC_TIMER_STATUS status, uintptr_t context) {
    events |= EVENT_TRANSMIT_ANGLES;
}

void dmac_channel0_callback(DMAC_TRANSFER_EVENT returned_evnt,
                            uintptr_t MyDmacContext) {
    static uint8_t servo = SERVO_1;

    if (DMAC_TRANSFER_EVENT_COMPLETE == returned_evnt) {
        bool overCurrent = false;
        uint16_t input_voltage = 0;
        for (size_t sample = 0; sample < TRANSFER_SIZE; sample++) {
            input_voltage += adc_result_array[sample];
            input_voltage = input_voltage >> 1;  // Dividing by two

            // 2.5 V == 0 A
            /*input_voltage =*/
            /*    (float)(adc_result_array[sample] * ADC_VREF / 4095U - 2.5)
             * /*/
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

        DMAC_ChannelTransfer(
            DMAC_CHANNEL_0, (const void*)&ADC0_REGS->ADC_RESULT,
            (const void*)adc_result_array, sizeof(adc_result_array));
    } else if (DMAC_TRANSFER_EVENT_ERROR == returned_evnt) {
        DMAC_ChannelTransfer(
            DMAC_CHANNEL_0, (const void*)&ADC0_REGS->ADC_RESULT,
            (const void*)adc_result_array, sizeof(adc_result_array));
    }
}


static void callback_init(void){
    CAN0_RxCallbackRegister(can_rx_callback, STATE_CAN_RECEIVE,
                            CAN_MSG_ATTR_RX_FIFO0);
    DMAC_ChannelCallbackRegister(DMAC_CHANNEL_0, dmac_channel0_callback, 0);
    TC0_TimerCallbackRegister(tc0_callback, (uintptr_t)NULL);
    TC1_TimerCallbackRegister(tc1_callback, (uintptr_t)NULL);
}
