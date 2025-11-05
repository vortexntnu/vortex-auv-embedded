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
        /* Sleep until an interrupt occurs*/
        PM_IdleModeEnter();
        
        /* Run application logic */
        App_Task();
    }

    /* Execution should not come here during normal operation */

    return ( EXIT_FAILURE );
}