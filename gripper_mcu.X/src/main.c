
/*
 * Define the Configuration Bits.
 */

#pragma config NVMCTRL_BOOTPROT = SIZE_0BYTES
#pragma config NVMCTRL_EEPROM_SIZE = SIZE_0BYTES
#pragma config BODVDDUSERLEVEL = 0x8 //VDD level 44. should trigger at around 4.5V
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

#include <samc21j18a.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h> 

#include "clock.h"
#include "system_init.h"
#include "supc.h"

void delay_ms(uint32_t ms) {
    uint32_t CPU_CLK = 120000000;
    uint32_t cycles = ((ms * CPU_CLK) / 10000);
    
    for(uint32_t i = 0; i < cycles; i++) {
        __asm__("NOP");
    }
    return;
}

int main(void) {
    PORT_REGS->GROUP[0].PORT_DIRSET = PORT_PA15;
    for(uint8_t i = 0; i < 100; i++) {
        PORT_REGS->GROUP[0].PORT_OUTTGL = PORT_PA15;
        delay_ms(40);
    }
    
    NVIC_Initialize();
    PIN_Initialize();
    NVMCTRL_Initialize();
    CLOCK_Initialize(); 
    //SUPC_Initialize();
        
    PORT_REGS->GROUP[0].PORT_OUTSET = PORT_PA15;
    
    
    while (1) {} 
    return 0;
}
