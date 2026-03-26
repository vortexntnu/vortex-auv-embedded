#include <stdlib.h>         // EXIT_FAILURE
#include "definitions.h"    // Harmony driv
#include "app/app.h"
#include <stdio.h>

int main ( void ) {
    /* Initialize all modules */
    SYS_Initialize ( NULL );
    
    RESET_TH_Clear();
    
    // uint8_t byte;
    
    //printf("--- ADC Reading test--- \r\n\r\n");

    
    /* Initialize application logic */
    app_init();
    
    //printf("\r\n=== UART Framing Loopback Test ===\r\n");
    
    //run_test("TURN_THRUSTERS_OFF (no payload)", 0x01U, NULL, 0U);

    
    //generate_pwm_signals();
    //printf("--- Testing Thrusters ---\n");
    
    
    //test_can_tx();
    
//    uint8_t to_test[] = {0, 1, 2, 3, 4, 5, 6, 7}; // Indices
//    uint8_t count = sizeof(to_test) / sizeof(to_test[0]);
//    uint16_t max_us = 1950;
//    uint16_t min_us = 1050;
//    uint16_t step_us = 25;
//    uint32_t step_delay_ms = 25;
    
    //test_thrusters(to_test, count, max_us, min_us, step_us, step_delay_ms);
    //test_thrusters_seq(to_test, count, max_us, min_us, step_us, step_delay_ms);
    //test_thrusters_split(to_test, count, max_us, min_us, step_us, step_delay_ms);
    
//    uint32_t hold_ms = 5000;
    //test_neutral_to_max(to_test, count, max_us, hold_ms);
    
        
    while ( true )
    {
        //while (!SERCOM2_USART_Read(&byte, 5)) {}
        
        //while (!SERCOM2_USART_Write(&byte, 5)) {}
        
        /* Sleep until an interrupt occurs*/
        //PM_IdleModeEnter();
                
        //generate_pwm_signals();
        
        //SYSTICK_DelayMs(100);
        
        //test_can_tx();
        //;
        
        
        
        /* Run application logic */
        app_task();
        
        
    }

    /* Execution should not come here during normal operation */

    return ( EXIT_FAILURE );
}