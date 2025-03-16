

#include "tc4.h"

// *****************************************************************************
// *****************************************************************************
// Section: Global Data
// *****************************************************************************
// *****************************************************************************

volatile static TC_TIMER_CALLBACK_OBJ TC4_CallbackObject;

// *****************************************************************************
// *****************************************************************************
// Section: TC4 Implementation
// *****************************************************************************
// *****************************************************************************

// *****************************************************************************
/* Initialize the TC module in Timer mode */
void TC4_TimerInitialize( void )
{
    /* Reset TC */
    TC4_REGS->COUNT16.TC_CTRLA = TC_CTRLA_SWRST_Msk;

    while((TC4_REGS->COUNT16.TC_SYNCBUSY & TC_SYNCBUSY_SWRST_Msk) == TC_SYNCBUSY_SWRST_Msk)
    {
        /* Wait for Write Synchronization */
    }

    /* Configure counter mode & prescaler */
    TC4_REGS->COUNT16.TC_CTRLA = TC_CTRLA_MODE_COUNT16 | TC_CTRLA_PRESCALER_DIV1024 | TC_CTRLA_PRESCSYNC_PRESC ;

    /* Configure in Match Frequency Mode */
    TC4_REGS->COUNT16.TC_WAVE = (uint8_t)TC_WAVE_WAVEGEN_NPWM;

    /* Configure timer period */
    TC4_REGS->COUNT16.TC_CC[0U] = 46874U;

    /* Clear all interrupt flags */
    TC4_REGS->COUNT16.TC_INTFLAG = (uint8_t)TC_INTFLAG_Msk;

    TC4_CallbackObject.callback = NULL;
    /* Enable interrupt*/
    /*TC4_REGS->COUNT16.TC_INTENSET = (uint8_t)(TC_INTENSET_OVF_Msk);*/


    while((TC4_REGS->COUNT16.TC_SYNCBUSY) != 0U)
    {
        /* Wait for Write Synchronization */
    }
}

/* Enable the TC counter */
void TC4_TimerStart( void )
{
    TC4_REGS->COUNT16.TC_CTRLA |= TC_CTRLA_ENABLE_Msk;
    while((TC4_REGS->COUNT16.TC_SYNCBUSY & TC_SYNCBUSY_ENABLE_Msk) == TC_SYNCBUSY_ENABLE_Msk)
    {
        /* Wait for Write Synchronization */
    }
}

/* Disable the TC counter */
void TC4_TimerStop( void )
{
    TC4_REGS->COUNT16.TC_CTRLA &= ~TC_CTRLA_ENABLE_Msk;
    while((TC4_REGS->COUNT16.TC_SYNCBUSY & TC_SYNCBUSY_ENABLE_Msk) == TC_SYNCBUSY_ENABLE_Msk)
    {
        /* Wait for Write Synchronization */
    }
}

uint32_t TC4_TimerFrequencyGet( void )
{
    return (uint32_t)(46875U);
}

void TC4_TimerCommandSet(TC_COMMAND command)
{
    TC4_REGS->COUNT16.TC_CTRLBSET = (uint8_t)((uint32_t)command << TC_CTRLBSET_CMD_Pos);
    while((TC4_REGS->COUNT16.TC_SYNCBUSY) != 0U)
    {
        /* Wait for Write Synchronization */
    }
}

/* Get the current timer counter value */
uint16_t TC4_Timer16bitCounterGet( void )
{
    /* Write command to force COUNT register read synchronization */
    TC4_REGS->COUNT16.TC_CTRLBSET |= (uint8_t)TC_CTRLBSET_CMD_READSYNC;

    while((TC4_REGS->COUNT16.TC_SYNCBUSY & TC_SYNCBUSY_CTRLB_Msk) == TC_SYNCBUSY_CTRLB_Msk)
    {
        /* Wait for Write Synchronization */
    }

    while((TC4_REGS->COUNT16.TC_CTRLBSET & TC_CTRLBSET_CMD_Msk) != 0U)
    {
        /* Wait for CMD to become zero */
    }

    /* Read current count value */
    return (uint16_t)TC4_REGS->COUNT16.TC_COUNT;
}

/* Configure timer counter value */
void TC4_Timer16bitCounterSet( uint16_t count )
{
    TC4_REGS->COUNT16.TC_COUNT = count;

    while((TC4_REGS->COUNT16.TC_SYNCBUSY & TC_SYNCBUSY_COUNT_Msk) == TC_SYNCBUSY_COUNT_Msk)
    {
        /* Wait for Write Synchronization */
    }
}

/* Configure timer period */
void TC4_Timer16bitPeriodSet( uint16_t period )
{
    TC4_REGS->COUNT16.TC_CC[0] = period;
    while((TC4_REGS->COUNT16.TC_SYNCBUSY & TC_SYNCBUSY_CC0_Msk) == TC_SYNCBUSY_CC0_Msk)
    {
        /* Wait for Write Synchronization */
    }
}

/* Read the timer period value */
uint16_t TC4_Timer16bitPeriodGet( void )
{
    return (uint16_t)TC4_REGS->COUNT16.TC_CC[0];
}



/* Register callback function */
void TC4_TimerCallbackRegister( TC_TIMER_CALLBACK callback, uintptr_t context )
{
    TC4_CallbackObject.callback = callback;

    TC4_CallbackObject.context = context;
}

/* Timer Interrupt handler */
void __attribute__((used)) TC4_Handler( void )
{
    if (TC4_REGS->COUNT16.TC_INTENSET != 0U)
    {
        TC_TIMER_STATUS status;
        status = (TC_TIMER_STATUS) TC4_REGS->COUNT16.TC_INTFLAG;
        /* Clear interrupt flags */
        TC4_REGS->COUNT16.TC_INTFLAG = (uint8_t)TC_INTFLAG_Msk;
        if((TC4_CallbackObject.callback != NULL) && (status != TC_TIMER_STATUS_NONE))
        {
            uintptr_t context = TC4_CallbackObject.context;
            TC4_CallbackObject.callback(status, context);
        }
    }
}

