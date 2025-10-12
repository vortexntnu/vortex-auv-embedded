#include "system_init.h"
#include "gripper.h"
#include "state_machine.h"


uint8_t Can0MessageRAM[CAN0_MESSAGE_RAM_CONFIG_SIZE]
    __attribute__((aligned(32)));
static struct can_rx_frame rx_frame;
static uint16_t adc_result_array[TRANSFER_SIZE];
static volatile uint32_t events;


int main(void) {
    system_init();

    CAN0_MessageRAMConfigSet(Can0MessageRAM);

    CAN0_RxCallbackRegister(can_rx_callback, (uintptr_t) &rx_frame,
                            CAN_MSG_ATTR_RX_FIFO0);
    DMAC_ChannelCallbackRegister(DMAC_CHANNEL_0, dmac_channel0_callback, 0);
    TC0_TimerCallbackRegister(tc0_callback, (uintptr_t)&events);
    TC1_TimerCallbackRegister(tc1_callback, (uintptr_t)&events);


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

