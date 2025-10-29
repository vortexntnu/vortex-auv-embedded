#include <stdlib.h>         // EXIT_FAILURE
#include "definitions.h"    // Harmony drivers
#include "app/app.h"


int main ( void ) {
    /* Initialize all modules */
    SYS_Initialize ( NULL );

    /* Initialize application logic */
    App_Init();
    
    while ( true )
    {
        printf("\r\n Inside while true loop in main.c\r\n\r\n");
        /* Sleep until an interrupt occurs*/
        PM_IdleModeEnter();
        
        printf("\r\n Beyond PM_IdleModeEnter() \r\n\r\n");
        /* Run application logic */
        App_Task();
        
        /* Maintain state machines of all polled MPLAB Harmony modules. */
        SYS_Tasks ( );
    }

    /* Execution should not come here during normal operation */

    return ( EXIT_FAILURE );
}