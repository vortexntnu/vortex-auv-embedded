
/*
 * Define the Configuration Bits.
 */

#pragma config NVMCTRL_BOOTPROT = SIZE_0BYTES
#pragma config NVMCTRL_EEPROM_SIZE = SIZE_0BYTES
#pragma config BODVDDUSERLEVEL = 0x8
#pragma config BODVDD_DIS = ENABLED
#pragma config BODVDD_ACTION = INT

#pragma config BODVDD_HYST = DISABLED
#pragma config NVMCTRL_REGION_LOCKS = 0xffff

#pragma config WDT_ENABLE = 0x0
#pragma config WDT_ALWAYSON = 0x0
#pragma config WDT_PER = 0xB

#pragma config WDT_WINDOW = 0xB
#pragma config WDT_EWOFFSET = 0xB
#pragma config WDT_WEN = 0x0


#define CPU_CLOCK_FREQUENCY 120000000U

#include <samc21e17a.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h> 

#include "clock.h"
#include "system_init.h"
#include "supc.h"


int main(void) {
    SUPC_Initialize();
    PIN_Initialize();
    NVMCTRL_Initialize();
    CLOCK_Initialize(); 
    
    while (1) {} 
    return 0;
}
