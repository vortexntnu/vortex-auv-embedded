#include "state_machine.h"
#include "system_init.h"

uint8_t Can0MessageRAM[CAN0_MESSAGE_RAM_CONFIG_SIZE]
    __attribute__((aligned(32)));

struct state_context state_context;
static uint16_t adc_result_array[TRANSFER_SIZE];


int main(void) {
    system_init();

    CAN0_MessageRAMConfigSet(Can0MessageRAM);
    CAN0_RxCallbackRegister(can_rx_callback, (uintptr_t)&state_context,
                            CAN_MSG_ATTR_RX_FIFO0);
    DMAC_ChannelCallbackRegister(DMAC_CHANNEL_0, dmac_channel0_callback, 0);
    TC0_TimerCallbackRegister(tc0_callback, (uintptr_t)&state_context.events);
    TC1_TimerCallbackRegister(tc1_callback, (uintptr_t)&state_context.events);
    DMAC_ChannelTransfer(DMAC_CHANNEL_0, (const void*)&ADC0_REGS->ADC_RESULT,
                         (const void*)adc_result_array,
                         sizeof(adc_result_array));
    SERCOM1_I2C_CallbackRegister(i2c1_callback, (uintptr_t)&state_context.events);
    can_recieve(&state_context.rx_frame);

    WDT_Enable();

    printf("Start Gripper\r\n");

    while (true) {
        PM_IdleModeEnter();
        state_machine(&state_context);
    }

    return EXIT_FAILURE;
}
