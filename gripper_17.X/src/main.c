#include "state_machine.h"
#include "system_init.h"

uint8_t Can0MessageRAM[CAN0_MESSAGE_RAM_CONFIG_SIZE]
    __attribute__((aligned(32)));
static struct can_rx_frame rx_frame;
static struct can_tx_frame tx_frame;
static volatile uint32_t events;
static uint16_t adc_result_array[TRANSFER_SIZE];

int main(void) {
    system_init();

    CAN0_MessageRAMConfigSet(Can0MessageRAM);
    CAN0_RxCallbackRegister(can_rx_callback, (uintptr_t)&rx_frame,
                            CAN_MSG_ATTR_RX_FIFO0);
    DMAC_ChannelCallbackRegister(DMAC_CHANNEL_0, dmac_channel0_callback, 0);
    TC0_TimerCallbackRegister(tc0_callback, (uintptr_t)&events);
    TC1_TimerCallbackRegister(tc1_callback, (uintptr_t)&events);
    DMAC_ChannelTransfer(DMAC_CHANNEL_0, (const void*)&ADC0_REGS->ADC_RESULT,
                         (const void*)adc_result_array,
                         sizeof(adc_result_array));

    can_recieve(&rx_frame);

    WDT_Enable();

    while (true) {
        PM_IdleModeEnter();
        state_machine(&events, &tx_frame, &rx_frame);
    }

    return EXIT_FAILURE;
}
