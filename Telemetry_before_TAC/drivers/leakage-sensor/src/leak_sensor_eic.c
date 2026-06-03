/* #include "plib_eic.h"
#include "plib_port.h" */
#include "definitions.h"
#include "can_facade.h"

#define LEAK_SENSOR_ALARM_STDID (0x100)

static void leak_sensor_isr(uintptr_t context) {
    (void)context;
    uint8_t can_payload[8] = {0};
    CAN_Send(LEAK_SENSOR_ALARM_STDID, can_payload, sizeof(can_payload)); 
}

void leak_sensor_init(void) {
    PORT_PinPeripheralFunctionConfig(PORT_PIN_PA28, PERIPHERAL_FUNCTION_A);

    EIC_CallbackRegister(EIC_PIN_8, leak_sensor_isr, 0);
    EIC_InterruptEnable(EIC_PIN_8);
}