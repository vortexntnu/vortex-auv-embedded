#include <stdlib.h>         // EXIT_FAILURE
#include "definitions.h"    // Harmony driv
#include "app/app.h"

static bool wait_for(volatile bool *flag, uint32_t ticks) {
    while (ticks--) {
        if (*flag) return true;
    }
    return false;
}

static bool run_test(const char *name, uint8_t msg_id, const uint8_t *payload, uint8_t length) {
    uart_message_ready = false;

    bool sent = uart_send_frame(msg_id, payload, length);
    if (!sent) {
        printf("[FAIL] %s: uart_send_frame() returned false\r\n", name);
        return false;
    }

    /* Wait for the loopback frame to be received and validated */
    if (!wait_for(&uart_message_ready, 500000UL)) {
        printf("[FAIL] %s: timed out waiting for loopback\r\n", name);
        return false;
    }

    /* Verify message ID */
     if (uart_msg_id != msg_id) {
        printf("[FAIL] %s: expected MSG_ID 0x%02X, got 0x%02X\r\n",
           name, msg_id, uart_msg_id);
        return false;
    }

    /* For frames with payload, verify the payload round-tripped correctly */
    if (length > 0U && payload != NULL) {
        if (memcmp(uart_payload, payload, length) != 0) {
            printf("[FAIL] %s: payload mismatch\r\n", name);
            return false;
        }
    }

    printf("[PASS] %s\r\n", name);
    return true;

}

int main ( void ) {
    /* Initialize all modules */
    SYS_Initialize ( NULL );
    
    RESET_TH_Clear();

    /* Initialize application logic */
    app_init();
    
    printf("\r\n=== UART Framing Loopback Test ===\r\n");
    
    run_test("TURN_THRUSTERS_OFF (no payload)", 0x01U, NULL, 0U);

    
    //generate_pwm_signals();
    //printf("--- Testing Thrusters ---\n");
    
    
    //test_can_tx();
    
    uint8_t to_test[] = {0, 1, 2, 3, 4, 5, 6, 7}; // Indices
    uint8_t count = sizeof(to_test) / sizeof(to_test[0]);
    uint16_t max_us = 1950;
    uint16_t min_us = 1050;
    uint16_t step_us = 25;
    uint32_t step_delay_ms = 25;
    
    //test_thrusters(to_test, count, max_us, min_us, step_us, step_delay_ms);
    //test_thrusters_seq(to_test, count, max_us, min_us, step_us, step_delay_ms);
    //test_thrusters_split(to_test, count, max_us, min_us, step_us, step_delay_ms);
    
    uint32_t hold_ms = 5000;
    //test_neutral_to_max(to_test, count, max_us, hold_ms);
    
    
        
    while ( true )
    {
        
        /* Sleep until an interrupt occurs*/
        //PM_IdleModeEnter();
                
        //generate_pwm_signals();
        
        //printf("Hello world!\n");
        
        //test_can_tx();
        //;
        
        /* Run application logic */
        //app_task();
        
        
    }

    /* Execution should not come here during normal operation */

    return ( EXIT_FAILURE );
}