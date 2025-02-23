#include <same51j20a.h>
//#include <stdint.h>
#include "supc.h"
#include "port.h"

void SUPC_Initialize(void) {
    SUPC_REGS->SUPC_INTENSET = SUPC_INTENSET_BOD33DET_Msk;
    return;
}

/* 
 * Handler for interrupts generated by crossing BOD33 threshold 
 * Toggles 
 */
void __attribute__((used)) SUPC_BODDET_Handler(void) {
    if ((SUPC_REGS->SUPC_STATUS & SUPC_STATUS_BOD33DET_Msk) != SUPC_STATUS_BOD33DET_Msk) {
        LED0_Toggle();
        SUPC_REGS->SUPC_INTFLAG = SUPC_INTFLAG_BOD33DET_Msk;
    }
}



