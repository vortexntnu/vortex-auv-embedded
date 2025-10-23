/*******************************************************************************
  Main Source File

  Company:
    Vortex NTNU.

 Author:
    Markus Sandvik

  File Name:
    main.c

  Summary:
    This file contains the "main" function for a project.

  Description:
    This file contains the "main" function for a project.  The
    "main" function calls the "SYS_Initialize" function to initialize the state
    machines of all generated modules in the system. It also needs to call
    led_init() to initialize the LED facade before using any LED functions.
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
#include "ws2812_spi_enc.h"      // gives WS2812_SPI_BYTES_PER_LED + encode API
#include "led_facade.h"

// *****************************************************************************
// *****************************************************************************
// Section: Main Entry Point
// *****************************************************************************
// *****************************************************************************


/* ---------------Moved to led_facade.c----------------
void led_clear_all(void)
{
    for (unsigned i = 0; i < 2; ++i)
        led_set(i, 0, 0, 0);
}*/
/*
void usart_clock_sanity(void)
{
    uint32_t f_periph = SERCOM4_USART_FrequencyGet();   // clock driving the baud generator
    printf("SERCOM4 clock: %lu Hz\r\n", (unsigned long)f_periph);
}*/

int main(void)
{
    SYS_Initialize(NULL);
    
    led_init();
    

    while (1) {
        SYS_Tasks();
        if(!led_busy()){
            led_clear_all();
            led_set(2,255,255,255);
            led_commit_async();
        }
    }
    return (EXIT_FAILURE);
}



/*******************************************************************************
 End of File
*/


