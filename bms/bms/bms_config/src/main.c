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
#include "ic_bms/bms_spi.h"
#include "ic_bms/spi_test.h"
#include "app/can_facade.h"
#include "app/can_telemetry.h"
#include "app/pwr_mode.h"
#include "app/state_machine.h"
#include "peripheral/port/plib_port.h"
#include <stdio.h>
#include <sys/types.h>


// *****************************************************************************
// *****************************************************************************
// Section: Main Entry Point
// *****************************************************************************
// *****************************************************************************
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



int main ( void )
{
    /* Initialize all modules */
    SYS_Initialize ( NULL );
    pwr_mode_init();
    CAN_Init();
    CAN_telemetry_init();
    RTC_Timer32CallbackRegister(TelemetryRtcCb, 0);
    RTC_Timer32InterruptEnable(RTC_TIMER32_INT_MASK_CMP0);
    RTC_Timer32Start();
    EIC_CallbackRegister(EIC_PIN_14, CAN_Wake_EIC_Callback, 0); 
    EIC_InterruptEnable(EIC_PIN_14); 
    
    //spi_driver_self_test_run(); 
    bq76942_init();
    bothoff_init();
    sm_init();
    //bms_battery_status();

  while ( true )
    {
        /* Maintain state machines of all polled MPLAB Harmony modules. */
        SYS_Tasks ( );
        state_machine();
    }

    
    /* Execution should not come here during normal operation */

    return ( EXIT_FAILURE );
}


/*******************************************************************************
 End of File
*/

