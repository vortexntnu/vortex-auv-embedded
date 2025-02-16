#include <same51j20a.h>
#include "port.h"

void PORT_Initialize(void) {
    
    /************************** GROUP 0 Initialization *************************/
   PORT_REGS->GROUP[0].PORT_OUT |= SW0;
   PORT_REGS->GROUP[0].PORT_PINCFG[15] |= 0X7U;

   PORT_REGS->GROUP[0].PORT_DIR |= LED0;
   PORT_REGS->GROUP[0].PORT_PINCFG[14] |= 0x0U;

   PORT_REGS->GROUP[0].PORT_PMUX[7] |= 0x0U; //Peripheral function A
   
}
