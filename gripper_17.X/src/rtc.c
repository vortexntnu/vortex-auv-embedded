

/*******************************************************************************
  Real Time Counter (RTC) PLIB

  Company:
    Microchip Technology Inc.

  File Name:
    plib_rtc_timer.c

  Summary:
    RTC PLIB Implementation file

  Description:
    This file defines the interface to the RTC peripheral library. This
    library provides access to and control of the associated peripheral
    instance in timer mode.

*******************************************************************************/
// DOM-IGNORE-BEGIN
/*******************************************************************************
* Copyright (C) 2018 Microchip Technology Inc. and its subsidiaries.
*
* Subject to your compliance with these terms, you may use Microchip software
* and any derivatives exclusively with Microchip products. It is your
* responsibility to comply with third party license terms applicable to your
* use of third party software (including open source software) that may
* accompany Microchip software.
*
* THIS SOFTWARE IS SUPPLIED BY MICROCHIP "AS IS". NO WARRANTIES, WHETHER
* EXPRESS, IMPLIED OR STATUTORY, APPLY TO THIS SOFTWARE, INCLUDING ANY IMPLIED
* WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY, AND FITNESS FOR A
* PARTICULAR PURPOSE.
*
* IN NO EVENT WILL MICROCHIP BE LIABLE FOR ANY INDIRECT, SPECIAL, PUNITIVE,
* INCIDENTAL OR CONSEQUENTIAL LOSS, DAMAGE, COST OR EXPENSE OF ANY KIND
* WHATSOEVER RELATED TO THE SOFTWARE, HOWEVER CAUSED, EVEN IF MICROCHIP HAS
* BEEN ADVISED OF THE POSSIBILITY OR THE DAMAGES ARE FORESEEABLE. TO THE
* FULLEST EXTENT ALLOWED BY LAW, MICROCHIP'S TOTAL LIABILITY ON ALL CLAIMS IN
* ANY WAY RELATED TO THIS SOFTWARE WILL NOT EXCEED THE AMOUNT OF FEES, IF ANY,
* THAT YOU HAVE PAID DIRECTLY TO MICROCHIP FOR THIS SOFTWARE.
*******************************************************************************/
// DOM-IGNORE-END

#include "rtc.h"

void RTC_Initialize(void)
{
    RTC_REGS->MODE0.RTC_CTRLA = RTC_MODE0_CTRLA_SWRST_Msk;

    while((RTC_REGS->MODE0.RTC_SYNCBUSY & RTC_MODE0_SYNCBUSY_SWRST_Msk) == RTC_MODE0_SYNCBUSY_SWRST_Msk)
    {
        /* Wait for Synchronization after Software Reset */
    }


    RTC_REGS->MODE0.RTC_CTRLA = RTC_MODE0_CTRLA_MODE(0) | RTC_MODE0_CTRLA_PRESCALER(0x1) | RTC_MODE0_CTRLA_COUNTSYNC_Msk |RTC_MODE0_CTRLA_MATCHCLR_Msk ;

   RTC_REGS->MODE0.RTC_COMP = 0x100;

    RTC_REGS->MODE0.RTC_EVCTRL = 0x100;
}


bool RTC_PeriodicIntervalHasCompleted (RTC_PERIODIC_INT_MASK period)
{
   bool periodIntervalComplete = false;
   if( (RTC_REGS->MODE0.RTC_INTFLAG & period) == period)
   {
       periodIntervalComplete = true;

       /* Clear Periodic Interval Interrupt */
       RTC_REGS->MODE0.RTC_INTFLAG = period;
   }

    return periodIntervalComplete;
}

bool RTC_Timer32CompareHasMatched(void)
{
   bool status = false;

   if((RTC_REGS->MODE0.RTC_INTFLAG & RTC_MODE0_INTFLAG_CMP0_Msk) == RTC_MODE0_INTFLAG_CMP0_Msk)
   {
       status = true;
       RTC_REGS->MODE0.RTC_INTFLAG = RTC_MODE0_INTFLAG_CMP0_Msk;
   }

   return status;
}
bool RTC_Timer32CounterHasOverflowed ( void )
{
   bool status = false;

   if((RTC_REGS->MODE0.RTC_INTFLAG & RTC_MODE0_INTFLAG_OVF_Msk) == RTC_MODE0_INTFLAG_OVF_Msk)
   {
       status = true;
       RTC_REGS->MODE0.RTC_INTFLAG = RTC_MODE0_INTFLAG_OVF_Msk;
   }

   return status;
}


void RTC_Timer32Start ( void )
{
    RTC_REGS->MODE0.RTC_CTRLA |= RTC_MODE0_CTRLA_ENABLE_Msk;

    while((RTC_REGS->MODE0.RTC_SYNCBUSY & RTC_MODE0_SYNCBUSY_ENABLE_Msk) == RTC_MODE0_SYNCBUSY_ENABLE_Msk)
    {
        /* Wait for synchronization after Enabling RTC */
    }
}


void RTC_Timer32Stop ( void )
{
    RTC_REGS->MODE0.RTC_CTRLA &= ~(RTC_MODE0_CTRLA_ENABLE_Msk);

    while((RTC_REGS->MODE0.RTC_SYNCBUSY & RTC_MODE0_SYNCBUSY_ENABLE_Msk) == RTC_MODE0_SYNCBUSY_ENABLE_Msk)
    {
        /* Wait for Synchronization after Disabling RTC */
    }
}

void RTC_Timer32CounterSet ( uint32_t count )
{
    RTC_REGS->MODE0.RTC_COUNT = count;

    while((RTC_REGS->MODE0.RTC_SYNCBUSY & RTC_MODE0_SYNCBUSY_COUNT_Msk) == RTC_MODE0_SYNCBUSY_COUNT_Msk)
    {
        /* Wait for Synchronization after writing value to Count Register */
    }
}

void RTC_Timer32CompareSet ( uint32_t compareValue )
{
    RTC_REGS->MODE0.RTC_COMP = compareValue;

    while((RTC_REGS->MODE0.RTC_SYNCBUSY & RTC_MODE0_SYNCBUSY_COMP0_Msk) == RTC_MODE0_SYNCBUSY_COMP0_Msk)
    {
        /* Wait for Synchronization after writing Compare Value */
    }
}
uint32_t RTC_Timer32CounterGet ( void )
{
    while((RTC_REGS->MODE0.RTC_SYNCBUSY & RTC_MODE0_SYNCBUSY_COUNT_Msk) == RTC_MODE0_SYNCBUSY_COUNT_Msk)
    {
        /* Wait for Synchronization before reading value from Count Register */
    }
   return(RTC_REGS->MODE0.RTC_COUNT);
}

uint32_t RTC_Timer32PeriodGet ( void )
{
    /* Get 32Bit Compare Value */
    return (RTC_MODE0_COUNT_COUNT_Msk);
}

uint32_t RTC_Timer32FrequencyGet ( void )
{
    /* Return Frequency of RTC Clock */
    return RTC_COUNTER_CLOCK_FREQUENCY;
}


