#include <stdio.h>
#include "gripper.h"
#include "state_machine.h"
#include "system_init.h"
#include "uart_protocol.h"

uint8_t Can0MessageRAM[CAN0_MESSAGE_RAM_CONFIG_SIZE]
    __attribute__((aligned(32)));

static uint16_t adc_result_array[TRANSFER_SIZE];
extern volatile bool int123;

void TCC_PeriodEventHandler(uint32_t status, uintptr_t context) {
    /* duty cycle values */
    static int8_t increment1 = 1;
    static uint32_t duty1 = 9000;
    static uint32_t duty2 = 9000;
    static uint32_t duty3 = 9000;
    // printf("test");

    /* Increment duty cycle values */
    duty1 += increment1;
    duty2 += increment1;
    duty3 += increment1;

    // printf("Duty %ld", duty1);

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

    TCC0_PWM24bitDutySet(1, duty1);
    TCC1_PWM24bitDutySet(0, duty2);
    TCC1_PWM24bitDutySet(1, duty3);
}

int main(void) {
    system_init();

    CAN0_MessageRAMConfigSet(Can0MessageRAM);
    CAN0_RxCallbackRegister(can_rx_callback, 0, CAN_MSG_ATTR_RX_FIFO0);
    DMAC_ChannelCallbackRegister(DMAC_CHANNEL_0, dmac_channel0_callback,
                                 (const uintptr_t)adc_result_array);
    TC0_TimerCallbackRegister(tc0_callback, 0);
    TC1_TimerCallbackRegister(tc1_callback, 0);

    // TCC0_PWMCallbackRegister(TCC_PeriodEventHandler, (uintptr_t)NULL);
    DMAC_ChannelTransfer(DMAC_CHANNEL_0, (const void*)&ADC0_REGS->ADC_RESULT,
                         (const void*)adc_result_array,
                         sizeof(adc_result_array));
    SERCOM1_I2C_CallbackRegister(i2c1_callback, 0);

    uart_proto_init();

    SERCOM0_USART_ReceiverEnable();
    SERCOM0_USART_TransmitterEnable();

    TC0_TimerStart();
    // TC1_TimerStart();

    // WDT_Enable();

    // printf("Start Gripper\r\n");
    // start_gripper();
    stop_gripper();
    //
    //   struct can_tx_frame tx;
    //   tx.id = 0x369;
    //   tx.len = 8;
    //   tx.buf[0] = 1;
    //   tx.buf[1] = 2;
    //   tx.buf[2] = 3;
    //   tx.buf[3] = 4;
    //   tx.buf[4] = 5;
    //   tx.buf[5] = 6;
    //   tx.buf[6] = 7;
    //   tx.buf[7] = 8;
    // printf("sending can frame\r\n");
    //   can_transmit(&tx);
    state_machine_init();
    //
    // PORT_REGS->GROUP[0].PORT_OUTCLR = (1 << 0) | (1 << 27) | (1 << 28);

    // const char msg[] = "UART test\r\n";
    // SERCOM0_USART_Write((void *)msg, sizeof(msg) - 1);
    //

    uint8_t rx_byte = 0;
    SERCOM0_USART_Read(&rx_byte, 1);
    while (true) {
        state_machine();
        uart_gripper_task();
    }

    return EXIT_FAILURE;
}
