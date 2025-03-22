
/* 
 * File:   tcc.h
 * Author: nathaniel
 *
 * Created on January 19, 2025, 6:34 PM
 */

#ifndef TCC_H
#define	TCC_H

#include "stdbool.h"
#include "stddef.h"
#include "tcc_common.h"
#ifdef	__cplusplus
extern "C" {
#endif

// DOM-IGNORE-END

// *****************************************************************************
// *****************************************************************************
// Section: Data Types
// *****************************************************************************
// *****************************************************************************
/*  The following data type definitions are used by the functions in this
    interface and should be considered part it.
*/

/* Total number of TCC channels in a module */
#define TCC2_NUM_CHANNELS    (2U)

/* TCC Channel numbers

   Summary:
    Identifies channel number within TCC module

   Description:
    This enumeration identifies TCC channel number.

   Remarks:
    None.
*/
typedef enum
{
    TCC2_CHANNEL0,
    TCC2_CHANNEL1,
    TCC2_CHANNEL2,
    TCC2_CHANNEL3,
}TCC2_CHANNEL_NUM;

// *****************************************************************************

/* TCC Channel interrupt status

   Summary:
    Identifies TCC PWM interrupt status flags

   Description:
    This enumeration identifies TCC PWM interrupt status falgs

   Remarks:
    None.
*/
typedef enum
{
    TCC2_PWM_STATUS_OVF = TCC_INTFLAG_OVF_Msk,
    TCC2_PWM_STATUS_FAULT_0 = TCC_INTFLAG_FAULT0_Msk,
    TCC2_PWM_STATUS_FAULT_1 = TCC_INTFLAG_FAULT1_Msk,
    TCC2_PWM_STATUS_MC_0 = TCC_INTFLAG_MC0_Msk,
    TCC2_PWM_STATUS_MC_1 = TCC_INTFLAG_MC1_Msk,
    TCC2_PWM_STATUS_MC_2 = TCC_INTFLAG_MC2_Msk,
    TCC2_PWM_STATUS_MC_3 = TCC_INTFLAG_MC3_Msk,
}TCC2_PWM_STATUS;

// *****************************************************************************
// *****************************************************************************
// Section: Interface Routines
// *****************************************************************************
// *****************************************************************************
/* The following functions make up the methods (set of possible operations) of
   this interface.
*/

// *****************************************************************************
void TCC2_PWMInitialize(void);

void TCC2_PWMStart(void);

void TCC2_PWMStop(void);

void TCC2_PWMDeadTimeSet(uint8_t deadtime_high, uint8_t deadtime_low);

void TCC2_PWMForceUpdate(void);

void TCC2_PWMPatternSet(uint8_t pattern_enable, uint8_t pattern_output);

void TCC2_PWMPeriodInterruptEnable(void);

void TCC2_PWMPeriodInterruptDisable(void);

void TCC2_PWMCallbackRegister(TCC_CALLBACK callback, uintptr_t context);

void TCC2_PWM24bitPeriodSet(uint32_t period);

uint32_t TCC2_PWM24bitPeriodGet(void);

void TCC2_PWM24bitCounterSet(uint32_t count);

__STATIC_INLINE void TCC2_PWM24bitDutySet(TCC2_CHANNEL_NUM channel, uint32_t duty)
{
    TCC2_REGS->TCC_CCBUF[channel] = duty & 0xFFFFFF;
}



#ifdef	__cplusplus
}
#endif

#endif	/* TCC_H */

