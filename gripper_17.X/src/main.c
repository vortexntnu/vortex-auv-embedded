#include <stdio.h>
#include "state_machine.h"
#include "system_init.h"

uint8_t Can0MessageRAM[CAN0_MESSAGE_RAM_CONFIG_SIZE]
    __attribute__((aligned(32)));

struct state_context state_context;
static uint16_t adc_result_array[TRANSFER_SIZE];



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
    CAN0_RxCallbackRegister(can_rx_callback, (uintptr_t)&state_context,
                            CAN_MSG_ATTR_RX_FIFO0);
    DMAC_ChannelCallbackRegister(DMAC_CHANNEL_0, dmac_channel0_callback, (const uintptr_t) adc_result_array);
    TC0_TimerCallbackRegister(tc0_callback, (uintptr_t)&state_context.events);
    TC1_TimerCallbackRegister(tc1_callback, (uintptr_t)&state_context.events);

    // TCC0_PWMCallbackRegister(TCC_PeriodEventHandler, (uintptr_t)NULL);
    // DMAC_ChannelTransfer(DMAC_CHANNEL_0, (const void*)&ADC0_REGS->ADC_RESULT,
    //                      (const void*)adc_result_array,
    //                      sizeof(adc_result_array));
    SERCOM1_I2C_CallbackRegister(i2c1_callback, (uintptr_t)&state_context.events);
    can_recieve(&state_context.rx_frame);

    TC0_TimerStart();
    // TC1_TimerStart();

    // WDT_Enable();

    printf("Start Gripper\r\n");
    start_gripper();

    while (true) {
        // PM_IdleModeEnter();
        state_machine(&state_context);
    }

    return EXIT_FAILURE;
}
