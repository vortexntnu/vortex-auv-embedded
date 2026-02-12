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

#include <stdio.h>



// *****************************************************************************
// *****************************************************************************
// Section: Main Entry Point
// *****************************************************************************
// *****************************************************************************
extern volatile bool rxReady;
extern uint32_t rx_messageID;
extern uint8_t  rx_message[64];
extern uint8_t  rx_messageLength;
extern uint16_t timestamp;



int main ( void )
{
    /* Initialize all modules */
    SYS_Initialize ( NULL );

    spi_driver_self_test_run();

    bq76942_init();
    bms_set_protection_threshold();
    read_cells_1to6();
    bms_battery_status();
  
    //bms_sample_temps();
    

    while ( true )
    {
        /* Maintain state machines of all polled MPLAB Harmony modules. */
        SYS_Tasks ( );
    }

    
    /* Execution should not come here during normal operation */

    return ( EXIT_FAILURE );
}


/*******************************************************************************
 End of File
*/

