#include "app/pwr_mode.h"
#include "definitions.h"
#include "sam.h"

static pwr_mode_state_t g_pwr_state= PWR_MODE_ACTIVE;


void pwr_mode_init(void)
{
    /* Start in the lightest sleep mode */
    pwr_set_idle0();
}

void pwr_set_idle0(void)
{
    
    PM_REGS->PM_SLEEPCFG = PM_SLEEPCFG_SLEEPMODE_IDLE0;

    while ((PM_REGS->PM_SLEEPCFG & PM_SLEEPCFG_SLEEPMODE_Msk) !=
           PM_SLEEPCFG_SLEEPMODE_IDLE0)
    {
    }
}

void pwr_set_stb_lowpower(void)
{
    /* Enter MCU standby; wake-up comes from an enabled interrupt. */
    PM_REGS->PM_STDBYCFG = PM_STDBYCFG_VREGSMOD_LP;
    PM_REGS->PM_SLEEPCFG = PM_SLEEPCFG_SLEEPMODE_STANDBY;

    while ((PM_REGS->PM_SLEEPCFG & PM_SLEEPCFG_SLEEPMODE_Msk) !=
           PM_SLEEPCFG_SLEEPMODE_STANDBY)
    {
    }
}

void pwr_enter_sleep(void)
{
    /* Sleep until the next interrupt. */
    __WFI(); // Wait For Interrupt instruction to enter sleep mode
    //not sure if correct method 
}

void pwr_set_state(pwr_mode_state_t state){
    g_pwr_state = state; // Update the global state variable
}

pwr_mode_state_t pwr_get_state(void)
{
    return g_pwr_state; // Return the current power mode state
}