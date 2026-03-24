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
    //printf("--- Testing Thrusters ---\n");
    
    
    //test_can_tx();
    
    uint8_t to_test[] = {1}; // Indices
    uint8_t count = sizeof(to_test) / sizeof(to_test[0]);
    uint16_t max_us = 1750;
    uint16_t min_us = 1250;
    uint16_t step_us = 50;
    uint32_t step_delay_ms = 10;
    
    test_thrusters(to_test, count, max_us, min_us, step_us, step_delay_ms);
    
        
    while ( true )
    {
        
        /* Sleep until an interrupt occurs*/
        //PM_IdleModeEnter();
                
        //generate_pwm_signals();
        
        //;
        
        /* Run application logic */
        //app_task();
        
        
    }

    /* Execution should not come here during normal operation */

    return ( EXIT_FAILURE );
}