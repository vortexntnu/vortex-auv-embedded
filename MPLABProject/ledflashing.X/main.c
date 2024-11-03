#include <sam.h>
#include <stdint.h>
#include <stdbool.h>

// Delay function for a simple blocking delay
void delay_ms(uint32_t ms) {
    for (volatile uint32_t i = 0; i < (ms * 3000); i++) {
        __asm("NOP"); // "No Operation"
    }
}

// Configure button and LED pins
void configure_pins(void) {
    // Enable the clock for Port B and C
    GCLK_REGS->GCLK_PCHCTRL[4] = GCLK_PCHCTRL_GEN(0x1U)  | GCLK_PCHCTRL_CHEN_Msk;

    while ((GCLK_REGS->GCLK_PCHCTRL[4] & GCLK_PCHCTRL_CHEN_Msk) != GCLK_PCHCTRL_CHEN_Msk)
    {
        /* Wait for synchronization */
    }

    
    MCLK_REGS->MCLK_APBBMASK |= MCLK_APBBMASK_PORT_Msk;
    MCLK_REGS->MCLK_APBAMASK |= MCLK_APBAMASK_EIC_Msk;

   PORT_REGS->GROUP[1].PORT_OUT = 0x80080000U;
   PORT_REGS->GROUP[1].PORT_PINCFG[31] = 0x7U;

   PORT_REGS->GROUP[1].PORT_PMUX[15] = 0x0U;

   /************************** GROUP 2 Initialization *************************/
   PORT_REGS->GROUP[2].PORT_DIR = 0x40000U;
   PORT_REGS->GROUP[2].PORT_PINCFG[18] = 0x0U;

   PORT_REGS->GROUP[2].PORT_PMUX[9] = 0x0U;
}

// Toggle LED on PC18
void toggle_led(void) {
    PORT_REGS->GROUP[2].PORT_OUTTGL = (1 << 18);
}

// Configure External Interrupt Controller (EIC)
void configure_interrupts(void) {
    // Enable falling edge detection on EXTINT[15] (for PB31) configure
    NVMCTRL_REGS->NVMCTRL_CTRLA = (uint16_t)NVMCTRL_CTRLA_RWS(5U) | NVMCTRL_CTRLA_AUTOWS_Msk;
    
    EIC_REGS->EIC_CTRLA |= EIC_CTRLA_SWRST_Msk;
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

    // Enable EIC_EXTINT_15 interrupt in NVIC
    NVIC_SetPriorityGrouping( 0x00 );

    /* Enable NVIC Controller */
    __DMB();
    __enable_irq();

    /* Enable the interrupt sources and configure the priorities as configured
     * from within the "Interrupt Manager" of MHC. */
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



int main(void) {
    configure_pins();       // Configure LED and button pins
    configure_interrupts(); // Set up interrupts for button press
    

    while (1) {

    } 
    return 0;
}

// External Interrupt Handler for EXTINT15
void __attribute__((used)) EIC_EXTINT_15_InterruptHandler(void){

    
    if (EIC_REGS->EIC_INTFLAG & (1 << 15)) {  // Check if EXTINT15 triggered the interrupt
        toggle_led();                         // Toggle the LED
    }
    
    EIC_REGS->EIC_INTFLAG = (1 << 15);    // Clear the interrupt flag for EXTINT15
    return;
}