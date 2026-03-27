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

#include <stdbool.h>  // Defines true
#include <stddef.h>   // Defines NULL
#include <stdio.h>
#include <stdlib.h>  // Defines EXIT_FAILURE
#include "app/can_telemetry.h"
#include "app/state_machine.h"
#include "config/default/peripheral/rtc/plib_rtc.h"
#include "config/default/peripheral/systick/plib_systick.h"
#include "definitions.h"  // SYS function prototypes
#include "ic_bms/bms_spi.h"
#include "ic_bms/spi_test.h"
#include "peripheral/port/plib_port.h"

// *****************************************************************************
// *****************************************************************************
// Section: Main Entry Point
// *****************************************************************************
// *****************************************************************************
/*
static void CAN_Wake_EIC_Callback(uintptr_t context) // called when a CAN
wake-up interrupt occurs on EIC pin 14
{
    (void)context;
    sm_on_can_wake();
}


*/
volatile bool rtc_timer = false;
extern volatile bool can_tx_avaliable;

static void TelemetryRtcCb(
    RTC_TIMER32_INT_MASK intCause,
    uintptr_t context)  // called every 100ms by the RTC timer interrupt
{
    (void)intCause;
    (void)context;
    // sm_on_rtc_tick();
    // CAN_telemetry_tickISR();
    // printf("RTC interrupt\r\n");
    rtc_timer = true;
}

int main(void) {
    /* Initialize all modules */
    SYS_Initialize(NULL);
    SYSTICK_TimerStart();
    BOTHOFF_Clear();

    BQ769x2_Init();
    // BOTHOFF_Clear();
    //  bms_init_comm_voltage();
    SYSTICK_DelayMs(100U);
    RTC_Timer32CompareSet(1000);
    RTC_Timer32CallbackRegister(TelemetryRtcCb, 0);
    RTC_Timer32InterruptEnable(RTC_TIMER32_INT_MASK_CMP0);
    RTC_Timer32Start();
    // bms_alert_irq_init();
    CAN_Init();
    // pwr_set_idle0();

    // CommandSubcommands(FET_ENABLE); // FET_ENABLE
    voltage_test_init();

    // can_scope_test_init();

    // printf("CAN scope test running\r\n");

    while (true) {
        SYS_Tasks();
        if (rtc_timer) {
            voltage_test_step();
            rtc_timer = false;
                    CAN_voltage_send();
            if (can_tx_avaliable) {
                CAN_alert_pfa_send();
                CAN_alert_ssa_send();
                CAN_current_send();
                CAN_voltage_send();
            }
        }

        // bms_alert_step();
        // spi_write_probe_step();

        // can_sope_test_step();
        state_machine_simple();
    }

    /* Execution should not come here during normal operation */

    return (EXIT_FAILURE);
}

/*******************************************************************************
 End of File
*/
