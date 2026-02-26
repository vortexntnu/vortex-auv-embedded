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
#include "peripheral/port/plib_port.h"

#include <stdio.h>
#include <sys/types.h>


// *****************************************************************************
// *****************************************************************************
// Section: Main Entry Point
// *****************************************************************************
// *****************************************************************************
extern volatile bool rxReady;
extern uint32_t rx_messageID;
extern uint8_t  rx_message[64];
extern uint8_t  rx_messageLength;


static volatile bool can_wake_irq = false;
static uint32_t no_can_ticks=0;
#define STANDBY_TIMEOUT_TICKS 50 

static void CAN_Wake_EIC_Callback(uintptr_t context)
{
    (void)context;
    can_wake_irq = true;
}



static void TelemetryRtcCb(RTC_TIMER32_INT_MASK intCause, uintptr_t context)
{
    (void)intCause;
    (void)context;
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
    bms_set_protection_threshold();
    bms_battery_status();
   

  
    //bms_sample_temps();
    

  while ( true )
    {
        /* Maintain state machines of all polled MPLAB Harmony modules. */
        SYS_Tasks ( );
        CAN_voltage_send();
        CAN_bothoff_rx();
        pwr_enter_sleep();

      switch (pwr_get_state())
        {
          case PWR_MODE_ACTIVE:
          {
            CAN_voltage_send();
            CAN_bothoff_rx();
            if (rxReady || can_wake_irq)
            {
              no_can_ticks=0;
              can_wake_irq=false;
            }
            else {
              no_can_ticks++;
            }

            if (no_can_ticks >= STANDBY_TIMEOUT_TICKS)
            {
              STB_Set();
              pwr_set_stb_lowpower();
              pwr_set_state(PWR_MODE_STANDBY);

            }
            break;
          }
          case PWR_MODE_STANDBY:
          {
            if (can_wake_irq)
            {
              can_wake_irq=false;
              no_can_ticks=0;

              STB_Clear(); //tranceiver normal mode
              pwr_set_idle0();
              CAN_Init();
              pwr_set_state(PWR_MODE_ACTIVE);
            }
            break;
          }
          default:
          {
            no_can_ticks=0;
            STB_Clear(); //tranceiver normal mode
            pwr_set_idle0();
            pwr_set_state(PWR_MODE_ACTIVE);
            break;
          }
       


             
        }


        
        
        
    }

    
    /* Execution should not come here during normal operation */

    return ( EXIT_FAILURE );
}


/*******************************************************************************
 End of File
*/

