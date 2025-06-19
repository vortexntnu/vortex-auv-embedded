

#include "system_init.h"
#include "same51j20a.h"



void pin_init(){
    
    // SERCOM0
    PORT_REGS->GROUP[0].PORT_PINCFG[8] = 0x1;
    PORT_REGS->GROUP[0].PORT_PINCFG[9] = 0x1;
    PORT_REGS->GROUP[0].PORT_PMUX[4] = 0x22;


    //SERCOM3
    PORT_REGS->GROUP[0].PORT_PINCFG[22] = 0x1;
    PORT_REGS->GROUP[0].PORT_PINCFG[23] = 0x1;
    PORT_REGS->GROUP[0].PORT_PMUX[11] = 0x22;


}


void nvm_ctrl_init(){
   NVMCTRL_REGS->NVMCTRL_CTRLA = (uint16_t)NVMCTRL_CTRLA_RWS(5U) | NVMCTRL_CTRLA_AUTOWS_Msk;
}


void nvic_init(){



    /* Priority 0 to 7 and no sub-priority. 0 is the highest priority */
    NVIC_SetPriorityGrouping( 0x00 );

    /* Enable NVIC Controller */
    __DMB();
    __enable_irq();

    /* Enable the interrupt sources and configure the priorities as configured
     * from within the "Interrupt Manager" of MHC. */
    NVIC_SetPriority(SERCOM3_0_IRQn, 6);
    NVIC_EnableIRQ(SERCOM3_0_IRQn);
    NVIC_SetPriority(SERCOM3_1_IRQn, 6);
    NVIC_EnableIRQ(SERCOM3_1_IRQn);
    NVIC_SetPriority(SERCOM3_2_IRQn, 6);
    NVIC_EnableIRQ(SERCOM3_2_IRQn);
    NVIC_SetPriority(SERCOM3_OTHER_IRQn, 6);
    NVIC_EnableIRQ(SERCOM3_OTHER_IRQn);



    NVIC_SetPriority(SERCOM0_0_IRQn, 7);
    NVIC_EnableIRQ(SERCOM0_0_IRQn);
    NVIC_SetPriority(SERCOM0_1_IRQn, 7);
    NVIC_EnableIRQ(SERCOM0_1_IRQn);
    NVIC_SetPriority(SERCOM0_2_IRQn, 7);
    NVIC_EnableIRQ(SERCOM0_2_IRQn);
    NVIC_SetPriority(SERCOM0_OTHER_IRQn, 7);
    NVIC_EnableIRQ(SERCOM0_OTHER_IRQn);

    /* Enable Usage fault */
    SCB->SHCSR |= (SCB_SHCSR_USGFAULTENA_Msk);
    /* Trap divide by zero */
    SCB->CCR   |= SCB_CCR_DIV_0_TRP_Msk;

    /* Enable Bus fault */
    SCB->SHCSR |= (SCB_SHCSR_BUSFAULTENA_Msk);

    /* Enable memory management fault */
    SCB->SHCSR |= (SCB_SHCSR_MEMFAULTENA_Msk);




}
