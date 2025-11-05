#include "plib_eic.h"
#include "plib_port.h"

static void leak_sensor_isr(uintptr_t context) {
    (void)context;
    // TODO: handle leak (or set a bool and handle in main.c)
}

void leak_sensor_init(void) {
    PORT_PinPeripheralFunctionConfig(PORT_PIN_PA28, PERIPHERAL_FUNCTION_A);

    EIC_Initialize();  // do this somewhere else?
    EIC_CallbackRegister(EIC_PIN_8, leak_sensor_isr, 0);
    EIC_InterruptEnable(EIC_PIN_8);
}