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

#include "leak_sensor_eic.h"
// #include "leak_update_tc2.h"
#include "led_facade.h"
#include "pressure_calc.h"
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

    spi_init();
    wsen_init();
    drdy_init();
    leak_sensor_init();

    struct leak_det leak_detector;
    leakdet_init(&leak_detector, NULL);

    timing_tc2_init_5hz();  // TODO: don't start timer until pressure+temp
                            // measurements have started

    float pressure = 0.0f;  // kPa
    float temp = 0.0f;      // °C
    float pressure_sum = 0.0f;
    float temp_sum = 0.0f;
    uint32_t samples = 0;

    while (1) {
        if (!led_busy()) {
            // update again if needed
        }

        wsen_cycle_tick();
        if (wsen_cycle_done_ok(&pressure, &temp)) {
            wsen_reset();

            pressure_sum += pressure;
            temp_sum += temp;
            samples++;
        } else {
            wsen_reset();
        }
        if (leakdet_tick) {
            leakdet_tick = false;

            float pressure_avg = pressure, temp_avg = temp;
            if (samples > 0) {
                pressure_avg = pressure_sum / samples;
                temp_avg = temp_sum / samples;
            }
            pressure_sum = 0.0f;
            temp_sum = 0.0f;
            samples = 0;

            bool fast = false, slow = false;
            leakdet_update(&leak_detector, pressure_avg, temp_avg, &fast,
                           &slow);

            if (fast) {
                // TODO: handle fast leak
            }
            if (slow) {
                // TODO: handle slow leak
            }
        }
        /* Execution should not come here during normal operation */

        return (EXIT_FAILURE);
    }
}

/*******************************************************************************
 End of File
*/
