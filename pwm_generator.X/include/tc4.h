
/*******************************************************************************
  Timer/Counter(TC4) PLIB

  Company
    Microchip Technology Inc.

  File Name
    plib_tc0.h

  Summary
    TC4 PLIB Header File.

  Description
    This file defines the interface to the TC peripheral library. This
    library provides access to and control of the associated peripheral
    instance.

  Remarks:
    None.

*******************************************************************************/

#ifndef TC4_H      // Guards against multiple inclusion
#define TC4_H

// *****************************************************************************
// *****************************************************************************
// Section: Included Files
// *****************************************************************************
// *****************************************************************************
/* This section lists the other files that are included in this file.
*/

#include "tc_common.h"

// DOM-IGNORE-BEGIN
#ifdef __cplusplus // Provide C Compatibility

    extern "C" {

#endif
// DOM-IGNORE-END

// *****************************************************************************
// *****************************************************************************
// Section: Data Types
// *****************************************************************************
// *****************************************************************************
/* The following data type definitions are used by the functions in this
    interface and should be considered part it.
*/

// *****************************************************************************
// *****************************************************************************
// Section: Interface Routines
// *****************************************************************************
// *****************************************************************************
/* The following functions make up the methods (set of possible operations) of
   this interface.
*/

// *****************************************************************************

void TC4_CompareInitialize( void );

void TC4_CompareStart( void );

void TC4_CompareStop( void );

uint32_t TC4_CompareFrequencyGet( void );

bool TC4_Compare16bitPeriodSet( uint16_t period );

uint16_t TC4_Compare16bitPeriodGet( void );

uint16_t TC4_Compare16bitCounterGet( void );

void TC4_Compare16bitCounterSet( uint16_t count );

bool TC4_Compare16bitMatch0Set( uint16_t compareValue );

bool TC4_Compare16bitMatch1Set( uint16_t compareValue );



TC_COMPARE_STATUS TC4_CompareStatusGet( void );

void TC4_CompareCommandSet(TC_COMMAND command);

// DOM-IGNORE-BEGIN
#ifdef __cplusplus  // Provide C++ Compatibility

    }

#endif
// DOM-IGNORE-END

#endif /* PLIB_TC4_H */
