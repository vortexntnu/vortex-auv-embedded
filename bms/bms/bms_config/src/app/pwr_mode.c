#include "app/pwr_mode.h"
#include "definitions.h"
#include "sam.h"

static pwr_mode_state_t g_pwr_state= PWR_MODE_ACTIVE;


void pwr_mode_init(void)
{
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
    PM_REGS->PM_STDBYCFG = PM_STDBYCFG_VREGSMOD_LP;
    PM_REGS->PM_SLEEPCFG = PM_SLEEPCFG_SLEEPMODE_STANDBY;

    while ((PM_REGS->PM_SLEEPCFG & PM_SLEEPCFG_SLEEPMODE_Msk) !=
           PM_SLEEPCFG_SLEEPMODE_STANDBY)
    {
    }
}

void pwr_enter_sleep(void)
{
    __WFI();
}

void pwr_set_state(pwr_mode_state_t state){
    g_pwr_state = state; 
}

pwr_mode_state_t pwr_get_state(void)
{
    return g_pwr_state;
}
