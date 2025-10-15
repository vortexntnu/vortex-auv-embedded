#include <stddef.h>                     // Defines NULL
#include <stdbool.h>                    // Defines true
#include <stdlib.h>                     // Defines EXIT_FAILURE
#include "definitions.h"                // SYS function prototypes

const uint32_t tcc0_period = 74000U;
const uint32_t tcc1_period = 74000U;
const uint32_t tcc2_period = 18500U;

const uint32_t pwm_period_microseconds = 20000U;


struct Thruster {
    uint8_t tcc_num;
    uint8_t channel;
    uint32_t period;
};

static const struct Thruster thrusters[8] = {
    {0, 0, tcc0_period}, // TCC0_CHANNEL0
    {0, 1, tcc0_period}, // TCC0_CHANNEL1
    {0, 2, tcc0_period}, // TCC0_CHANNEL2
    {0, 3, tcc0_period}, // TCC0_CHANNEL3
    {0, 4, tcc0_period}, // TCC0_CHANNEL4
    {0, 5, tcc0_period}, // TCC0_CHANNEL5
    {1, 0, tcc1_period}, // TCC1_CHANNEL0
    {1, 1, tcc1_period}  // TCC1_CHANNEL1 
};

/*
 * Set thruster PWM dutycycle and reset watchdog timer 
 * 
 * @param pData pointer to array containing dutycycle values
 */
static void set_thruster_pwm(uint8_t *data);

static void stop_thrusters(void);

static void start_thrusters(void);


int main ( void ) {
    /* Initialize all modules */
    SYS_Initialize ( NULL );

    while ( true )
    {
        /* Maintain state machines of all polled MPLAB Harmony modules. */
        SYS_Tasks ( );
    }

    /* Execution should not come here during normal operation */

    return ( EXIT_FAILURE );
}

static void set_thruster_pwm(uint8_t *data) {
    for (size_t thr; thr < 8; thr++) {
        uint16_t duty_cycle = (data[2*thr] << 8) | data[2*thr + 1];
        uint32_t tcc_value = (duty_cycle * (thrusters[thr].period + 1)) / pwm_period_microseconds;
        
        switch (thrusters[thr].tcc_num) {
            case 0:
                TCC0_PWM24bitDutySet(thrusters[thr].channel, tcc_value);
                break;
                
            case 1:
                TCC1_PWM24bitDutySet(thrusters[thr].channel, tcc_value);
                break;
                
            case 2:
                // Shouldn't hit this with current TCC channel configuration
                TCC2_PWM16bitDutySet(thrusters[thr].channel, (uint16_t) tcc_value);
                break;
            default:
                break;
        }
    }
    WDT_Clear();
}

static void stop_thrusters(void) {
    TCC0_PWMStop();
    TCC1_PWMStop();
    TCC2_PWMStop();
};

static void start_thrusters(void) {
    TCC0_PWMStart();
    TCC1_PWMStart();
    TCC2_PWMStart();
};


