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

#include <stdio.h>



// *****************************************************************************
// *****************************************************************************
// Section: Main Entry Point
// *****************************************************************************
// *****************************************************************************

static inline void _delay(uint32_t cycles){

    for (volatile uint32_t i=0; i<cycles; i++);
}

void ReadCells_1to6(){

  const uint8_t CellVoltageAddr[6]={CELL1VOLTAGE, CELL2VOLTAGE, CELL3VOLTAGE, CELL4VOLTAGE, CELL5VOLTAGE, CELL6VOLTAGE};
  uint16_t raw = 0;
  float voltage = 0.0f;
  uint8_t i = 0;

  for (i=0; i<6; i++){
    if (BQ_DirectCommand(CellVoltageAddr[i], &raw , R))
    {
      voltage = raw*0.001f; // Convert mV to V
      printf("Cell %u Voltage: %.3f V\n", i+1, voltage);
    }
    else {
    {
      printf("Failed to read Cell %u Voltage\n", i+1);  
    }
    }


  }

}












int main ( void )
{
    /* Initialize all modules */
    SYS_Initialize ( NULL );

    BQ76942_Init();
    BMS_SetProtectionThresholds();

    ReadCells_1to6();
    BMS_BATTERY_STATUS();
    _delay(1000000);


    

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

