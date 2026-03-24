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
#include <stdio.h>
#include <stdlib.h>                     // Defines EXIT_FAILURE
#include "app/state_machine.h"
#include "config/default/peripheral/systick/plib_systick.h"
#include "definitions.h"                // SYS function prototypes
#include "ic_bms/bms_spi.h"
#include "ic_bms/spi_test.h"
#include "peripheral/port/plib_port.h"

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
    SYSTICK_TimerStart();
    BOTHOFF_Clear();

    BQ769x2_Init(); 
    //BOTHOFF_Clear();
    // bms_init_comm_voltage(); 
    SYSTICK_DelayMs(100U);
    //bms_alert_irq_init();
    

    //CommandSubcommands(FET_ENABLE); // FET_ENABLE
    voltage_test_init(); 
    

    //can_scope_test_init();

    //printf("CAN scope test running\r\n");

    while ( true )
    {
        SYS_Tasks();

        voltage_test_step(); 
        //bms_alert_step();
        //spi_write_probe_step(); 

        //can_scope_test_step();
        //state_machine();
    }


    /* Execution should not come here during normal operation */

    return ( EXIT_FAILURE );
}


/*******************************************************************************
 End of File
*/
