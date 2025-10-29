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

#include <stdbool.h>      // Defines true
#include <stddef.h>       // Defines NULL
#include <stdlib.h>       // Defines EXIT_FAILURE
#include "definitions.h"  // SYS function prototypes

#include "led_facade.h"
#include "wsen_pads_port_sercom3.h"

// *****************************************************************************
// *****************************************************************************
// Section: Main Entry Point
// *****************************************************************************
// *****************************************************************************

int main(void) {
    // system init...
    led_init();  // calls ws_led_sercom5_init_2p4mhz(),
                 // ws2812enc_init(), and bind()

    led_set(0, 0x00, 0x10, 0x00);
    led_commit_async();

    i2c_init();
    wsen_init();
    drdy_init();

    float pressure = 0.0f;
    float temp = 0.0f;

    while (1) {
        if (!led_busy()) {
            // update again if needed
        }

        wsen_cycle_tick();
        if (wsen_cycle_done_ok(&pressure, &temp)) {
            wsen_reset();
            // TODO: do something with pressure and temp
        } else {
            SERCOM_I2C_ERROR err;
            if (wsen_cycle_failed(&err)) {
                wsen_reset();
            }
        }
    }
    /* Execution should not come here during normal operation */

    return (EXIT_FAILURE);
}

/*******************************************************************************
 End of File
*/
