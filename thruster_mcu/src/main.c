#include <stddef.h>                     // Defines NULL
#include <stdbool.h>                    // Defines true
#include <stdlib.h>                     // Defines EXIT_FAILURE
#include "definitions.h"                // SYS function prototypes

const uint32_t tcc0_period = 74000U;
const uint32_t tcc1_period = 74000U;
const uint32_t tcc2_period = 18500U;


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



int main ( void )
{
    printf(thrusters[0].tcc_num);
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

