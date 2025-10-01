#include "sam.h"

void delay(volatile uint32_t count) {
    while (count--) {
        __NOP(); // No Operation Instruction
    }
}

int main(void) {
    // 1. Set PC18 as output
    PORT_REGS->GROUP[2].PORT_DIR |= (1 << 18);

    while (1) {
        // 2. Turn LED on
        PORT_REGS->GROUP[2].PORT_OUT |= (1 << 18);
        delay(1000000);

        // 3. Turn LED off
        PORT_REGS->GROUP[2].PORT_OUT &= ~(1 << 18);
        delay(1000000);
    }
}
