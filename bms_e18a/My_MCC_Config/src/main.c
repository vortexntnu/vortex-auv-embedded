/*******************************************************************************
  Main Source File

  Company:
    Microchip Technology Inc.

  File Name:
    main.c

  Summary:
    This file contains the "main" function for a project.

  Description:
    This file contains the "main" function for a project.  The
    "main" function calls the "SYS_Initialize" function to initialize the state
    machines of all modules in the system
 *******************************************************************************/

// *****************************************************************************
// *****************************************************************************
// Section: Included Files
// *****************************************************************************
// *****************************************************************************

#include <stddef.h>                     // Defines NULL
#include <stdbool.h>                    // Defines true
#include <stdlib.h>                     // Defines EXIT_FAILURE
#include "definitions.h"                // SYS function prototypes
#include "ic_bms/spi_test.h"

// *****************************************************************************
// *****************************************************************************
// Section: Main Entry Point
// *****************************************************************************
// *****************************************************************************
/*
static void CAN_Wake_EIC_Callback(uintptr_t context) // called when a CAN wake-up interrupt occurs on EIC pin 14
{
    (void)context;
    sm_on_can_wake();
}


static void TelemetryRtcCb(RTC_TIMER32_INT_MASK intCause, uintptr_t context) // called every 100ms by the RTC timer interrupt
{
    (void)intCause;
    (void)context;
    sm_on_rtc_tick();
    CAN_telemetry_tickISR();
}

*/

int main ( void )
{
    /* Initialize all modules */
    SYS_Initialize ( NULL );
    voltage_test_init();

    while ( true )
    {
        SYS_Tasks();
        spi_write_probe_step();
    }


    /* Execution should not come here during normal operation */

    return ( EXIT_FAILURE );
}


/*******************************************************************************
 End of File
*/
