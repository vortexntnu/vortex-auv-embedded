
/*
 * Define the Configuration Bits.
 */
#pragma config BOD33_DIS = CLEAR
#pragma config BOD33USERLEVEL = 0x19U
#pragma config BOD33_ACTION = INT
#pragma config BOD33_HYST = 0x2U
#pragma config NVMCTRL_BOOTPROT = 0
#pragma config NVMCTRL_SEESBLK = 0x0U                          
#pragma config NVMCTRL_SEEPSZ = 0x0U                                  
#pragma config RAMECC_ECCDIS = SET                                    
#pragma config WDT_ENABLE = CLEAR
#pragma config WDT_ALWAYSON = CLEAR
#pragma config WDT_PER = CYC8192
#pragma config WDT_WINDOW = CYC8192
#pragma config WDT_EWOFFSET = CYC8192
#pragma config WDT_WEN = CLEAR
#pragma config NVMCTRL_REGION_LOCKS = 0xffffffffU


#define CPU_CLOCK_FREQUENCY 120000000U

#include <same51j20a.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h> 

#include "clock/clock.h"
#include "port/port.h"
#include "interrupts/interrupts.h"
#include "supc/supc.h"


int main(void) {
    SUPC_Initialize();
    PORT_Initialize(); 
    CLOCK_Initialize(); 
    Interrupts_Initialize();
    
    while (1) {} 
    return 0;
}
