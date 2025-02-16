#include <same51j20a.h>
#include "interrupts.h"
#include "port.h"


void NVIC_Initialize(void) {

    // Enable EIC_EXTINT_15 interrupt in NVIC
    NVIC_SetPriorityGrouping( 0x00 );

    /* Enable NVIC Controller */
    __DMB();
    __enable_irq();

    /* Enable the interrupt sources and configure the priorities as configured
     * from within the "Interrupt Manager" of MHC. */
    
    /* High priority for Brown-Out detection */
    NVIC_SetPriority(SUPC_BODDET_IRQn, 1);
    NVIC_EnableIRQ(SUPC_BODDET_IRQn);
            
    NVIC_SetPriority(EIC_EXTINT_15_IRQn, 7);
    NVIC_EnableIRQ(EIC_EXTINT_15_IRQn);

    /* Enable Usage fault */
    SCB->SHCSR |= (SCB_SHCSR_USGFAULTENA_Msk);
    /* Trap divide by zero */
    SCB->CCR   |= SCB_CCR_DIV_0_TRP_Msk;

    /* Enable Bus fault */
    SCB->SHCSR |= (SCB_SHCSR_BUSFAULTENA_Msk);

    /* Enable memory management fault */
    SCB->SHCSR |= (SCB_SHCSR_MEMFAULTENA_Msk);
}

void EIC_Initialize(void) {
    // Enable falling edge detection on EXTINT[15] (for PA15) configure
    NVMCTRL_REGS->NVMCTRL_CTRLA = (uint16_t)NVMCTRL_CTRLA_RWS(5U) | NVMCTRL_CTRLA_AUTOWS_Msk;
    
    //EIC_REGS->EIC_CTRLA |= EIC_CTRLA_SWRST_Msk;
    EIC_REGS->EIC_CTRLA |= EIC_CTRLA_CKSEL_CLK_ULP32K;
    while ((EIC_REGS->EIC_SYNCBUSY & EIC_SYNCBUSY_ENABLE_Msk) == EIC_SYNCBUSY_ENABLE_Msk){
        ;
    }
    
    EIC_REGS->EIC_CONFIG[0] |= EIC_CONFIG_SENSE3_RISE;
    EIC_REGS->EIC_CONFIG[1] |= EIC_CONFIG_SENSE7_RISE; // Falling edge trigger for EXTINT15
    EIC_REGS->EIC_INTENSET |= (1U << 15U);                    // Enable interrupt for EXTINT15
    EIC_REGS->EIC_CTRLA |= EIC_CTRLA_ENABLE_Msk;           // Enable EIC

    // Wait for sync
    while ((EIC_REGS->EIC_SYNCBUSY & EIC_SYNCBUSY_ENABLE_Msk) == EIC_SYNCBUSY_ENABLE_Msk){
        ;
    }
    return;
}

void Interrupts_Initialize(void) {
    EIC_Initialize();
    NVIC_Initialize();
    return;
}

// External Interrupt Handler for EXTINT15
void __attribute__((used)) EIC_EXTINT_15_Handler(void){
    LED0_Toggle();
    EIC_REGS->EIC_INTFLAG |= (1 << 15); 
    return;
}
