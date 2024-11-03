#include <sam.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h> 
#define CPU_CLOCK_FREQUENCY 120000000

#pragma config BOD33_DIS = SET
#pragma config BOD33USERLEVEL = 0x1cU
#pragma config BOD33_ACTION = RESET
#pragma config BOD33_HYST = 0x2U
#pragma config NVMCTRL_BOOTPROT = 0
#pragma config NVMCTRL_SEESBLK = 0x0U
#pragma config NVMCTRL_SEEPSZ = 0x0U
#pragma config RAMECC_ECCDIS = SET
#pragma config WDT_ENABLE = CLEAR
#pragma config WDT_ALWAYSON = CLEAR
#pragma config WDT_PER = CYC8192
#pragma config WDT_WINDOW = CYC8192
#pragma config WDT_EWOFFSET = CYC8192
#pragma config WDT_WEN = CLEAR
#pragma config NVMCTRL_REGION_LOCKS = 0xffffffffU



static void OSCCTRL_Initialize(void)
{

}

static void OSC32KCTRL_Initialize(void)
{

    OSC32KCTRL_REGS->OSC32KCTRL_RTCCTRL = OSC32KCTRL_RTCCTRL_RTCSEL(0U);
}

static void FDPLL0_Initialize(void)
{
    GCLK_REGS->GCLK_PCHCTRL[1] = GCLK_PCHCTRL_GEN(0x2U)  | GCLK_PCHCTRL_CHEN_Msk;
    while ((GCLK_REGS->GCLK_PCHCTRL[1] & GCLK_PCHCTRL_CHEN_Msk) != GCLK_PCHCTRL_CHEN_Msk)
    {
        /* Wait for synchronization */
    }

    /****************** DPLL0 Initialization  *********************************/

    /* Configure DPLL    */
    OSCCTRL_REGS->DPLL[0].OSCCTRL_DPLLCTRLB = OSCCTRL_DPLLCTRLB_FILTER(0U) | OSCCTRL_DPLLCTRLB_LTIME(0x0U)| OSCCTRL_DPLLCTRLB_REFCLK(0U) ;


    OSCCTRL_REGS->DPLL[0].OSCCTRL_DPLLRATIO = OSCCTRL_DPLLRATIO_LDRFRAC(0U) | OSCCTRL_DPLLRATIO_LDR(119U);

    while((OSCCTRL_REGS->DPLL[0].OSCCTRL_DPLLSYNCBUSY & OSCCTRL_DPLLSYNCBUSY_DPLLRATIO_Msk) == OSCCTRL_DPLLSYNCBUSY_DPLLRATIO_Msk)
    {
        /* Waiting for the synchronization */
    }

    /* Enable DPLL */
    OSCCTRL_REGS->DPLL[0].OSCCTRL_DPLLCTRLA = OSCCTRL_DPLLCTRLA_ENABLE_Msk   ;

    while((OSCCTRL_REGS->DPLL[0].OSCCTRL_DPLLSYNCBUSY & OSCCTRL_DPLLSYNCBUSY_ENABLE_Msk) == OSCCTRL_DPLLSYNCBUSY_ENABLE_Msk )
    {
        /* Waiting for the DPLL enable synchronization */
    }

    while((OSCCTRL_REGS->DPLL[0].OSCCTRL_DPLLSTATUS & (OSCCTRL_DPLLSTATUS_LOCK_Msk | OSCCTRL_DPLLSTATUS_CLKRDY_Msk)) !=
                (OSCCTRL_DPLLSTATUS_LOCK_Msk | OSCCTRL_DPLLSTATUS_CLKRDY_Msk))
    {
        /* Waiting for the Ready state */
    }
}


static void DFLL_Initialize(void)
{
}


static void GCLK0_Initialize(void)
{

    /* selection of the CPU clock Division */
    MCLK_REGS->MCLK_CPUDIV = MCLK_CPUDIV_DIV(0x01U);

    while((MCLK_REGS->MCLK_INTFLAG & MCLK_INTFLAG_CKRDY_Msk) != MCLK_INTFLAG_CKRDY_Msk)
    {
        /* Wait for the Main Clock to be Ready */
    }
    GCLK_REGS->GCLK_GENCTRL[0] = GCLK_GENCTRL_DIV(1U) | GCLK_GENCTRL_SRC(7U) | GCLK_GENCTRL_GENEN_Msk;

    while((GCLK_REGS->GCLK_SYNCBUSY & GCLK_SYNCBUSY_GENCTRL_GCLK0) == GCLK_SYNCBUSY_GENCTRL_GCLK0)
    {
        /* wait for the Generator 0 synchronization */
    }
}

static void GCLK1_Initialize(void)
{
    GCLK_REGS->GCLK_GENCTRL[1] = GCLK_GENCTRL_DIV(2U) | GCLK_GENCTRL_SRC(7U) | GCLK_GENCTRL_GENEN_Msk;

    while((GCLK_REGS->GCLK_SYNCBUSY & GCLK_SYNCBUSY_GENCTRL_GCLK1) == GCLK_SYNCBUSY_GENCTRL_GCLK1)
    {
        /* wait for the Generator 1 synchronization */
    }
}

static void GCLK2_Initialize(void)
{
    GCLK_REGS->GCLK_GENCTRL[2] = GCLK_GENCTRL_DIV(48U) | GCLK_GENCTRL_SRC(6U) | GCLK_GENCTRL_GENEN_Msk;

    while((GCLK_REGS->GCLK_SYNCBUSY & GCLK_SYNCBUSY_GENCTRL_GCLK2) == GCLK_SYNCBUSY_GENCTRL_GCLK2)
    {
        /* wait for the Generator 2 synchronization */
    }
}

void CLOCK_Initialize (void)
{
    /* MISRAC 2012 deviation block start */
    /* MISRA C-2012 Rule 2.2 deviated in this file.  Deviation record ID - H3_MISRAC_2012_R_2_2_DR_2 */

    /* Function to Initialize the Oscillators */
    OSCCTRL_Initialize();

    /* Function to Initialize the 32KHz Oscillators */
    OSC32KCTRL_Initialize();

    DFLL_Initialize();
    GCLK2_Initialize();
    FDPLL0_Initialize();
    GCLK0_Initialize();
    GCLK1_Initialize();

    /* MISRAC 2012 deviation block end */

    /* Selection of the Generator and write Lock for EIC */
    GCLK_REGS->GCLK_PCHCTRL[4] = GCLK_PCHCTRL_GEN(0x1U)  | GCLK_PCHCTRL_CHEN_Msk;

    while ((GCLK_REGS->GCLK_PCHCTRL[4] & GCLK_PCHCTRL_CHEN_Msk) != GCLK_PCHCTRL_CHEN_Msk)
    {
        /* Wait for synchronization */
    }

    /* Configure the AHB Bridge Clocks */
    MCLK_REGS->MCLK_AHBMASK = 0xffffffU;

    /* Configure the APBA Bridge Clocks */
    MCLK_REGS->MCLK_APBAMASK = 0x7ffU;


}


// Configure button and LED pins
void configure_pins(void) {
    // Enable the clock for Port B and C


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
    configure_pins(); 
    CLOCK_Initialize(); // Configure LED and button pins
    configure_interrupts(); // Set up interrupts for button press
    

    while (1) {

    } 
    return 0;
}

// External Interrupt Handler for EXTINT15
void __attribute__((used)) EIC_EXTINT_15_Handler(void){

    EIC_REGS->EIC_INTFLAG = (1 << 15); 
    toggle_led();
       // Clear the interrupt flag for EXTINT15
    return;
}
