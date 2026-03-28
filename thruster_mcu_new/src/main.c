#include <stdlib.h>         // EXIT_FAILURE
#include "definitions.h"    // Harmony driv
#include "app/app.h"

int main ( void ) {
    /* Initialize all modules */
    SYS_Initialize ( NULL );
    
    RESET_TH_Clear();

    /* Initialize application logic */
    app_init();
    
    //generate_pwm_signals();
    printf("--- Testing can ---\r\n");
    
    // char message[] = "TEST\r\n";
    // SERCOM2_USART_Write(message, sizeof(message) - 1);
    
    test_can_tx();
        
    while ( true )
    {
        
        /* Sleep until an interrupt occurs*/
        //PM_IdleModeEnter();
                
        //generate_pwm_signals();
        
        /* Run application logic */
        app_task();
        
        
    }

    /* Execution should not come here during normal operation */

    return ( EXIT_FAILURE );
}
