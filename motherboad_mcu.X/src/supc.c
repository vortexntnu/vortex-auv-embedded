#include <same51j20a.h>
#include "supc.h"
#include "port.h"


void SUPC_Initialize(void) {
    /* Enable BOD33 */
    SUPC_REGS->SUPC_BOD33 |= SUPC_BOD33_ENABLE_Msk;
    
    while((SUPC_REGS->SUPC_STATUS & SUPC_STATUS_B33SRDY_Msk) != SUPC_STATUS_B33SRDY_Msk) {
        ;
    }
    
    SUPC_REGS->SUPC_INTFLAG = SUPC_INTFLAG_BOD33DET_Msk;
    /* Enable the BOD33 detection interrupt */
    SUPC_REGS->SUPC_INTENSET = SUPC_INTENSET_BOD33DET_Msk;
    
    return;
}

/* Handler for interrupts generated by crossing BOD threshold*/
void __attribute__((used)) SUPC_BODDET_Handler(void) {
    if ((SUPC_REGS->SUPC_STATUS & SUPC_STATUS_BOD33DET_Msk) != SUPC_STATUS_BOD33DET_Msk) {
        LED0_Toggle();
        SUPC_REGS->SUPC_INTFLAG = SUPC_INTFLAG_BOD33DET_Msk;
    }
}

void __attribute__((used)) SUPC_OTHER_Handler(void) {
    if ((SUPC_REGS->SUPC_STATUS & SUPC_STATUS_BOD33DET_Msk) != SUPC_STATUS_BOD33DET_Msk) {
        LED0_Toggle();
        SUPC_REGS->SUPC_INTFLAG = SUPC_INTFLAG_BOD33DET_Msk;
    }
}


