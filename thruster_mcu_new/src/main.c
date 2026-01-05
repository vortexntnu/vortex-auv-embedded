#include <stdlib.h>         // EXIT_FAILURE
#include "definitions.h"    // Harmony driv


int main ( void ) {
    /* Initialize all modules */
    SYS_Initialize ( NULL );

    /* Initialize application logic */
    app_init();
    
    while ( true )
    {
        /* Sleep until an interrupt occurs*/
        PM_IdleModeEnter();
        
        /* Run application logic */
        app_task();
    }

    /* Execution should not come here during normal operation */

    return ( EXIT_FAILURE );
}