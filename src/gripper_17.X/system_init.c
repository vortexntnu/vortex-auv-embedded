/*
 * File:   other.c
 * Author: nathaniel
 *
 * Created on January 19, 2025, 4:28 PM
 */

#include "sam.h"
#include "samc21j18a.h"
void PIN_Initialize(void) {
    // PORT_REGS->GROUP[0].PORT_DIRSET = (1 << 15);

    // SERCOM4 USART Pins
    PORT_REGS->GROUP[1].PORT_PINCFG[10] = 0x1;
    PORT_REGS->GROUP[1].PORT_PINCFG[11] = 0x1;

    PORT_REGS->GROUP[1].PORT_PMUX[5] = 0x33;

    // PWM Pins
    PORT_REGS->GROUP[0].PORT_PINCFG[7] = 0x1;   // PWM3 TCC1[1]
    PORT_REGS->GROUP[0].PORT_PINCFG[10] = 0x1;  // PWM2 TCC1[0]
    PORT_REGS->GROUP[0].PORT_PINCFG[19] = 0x1;  // PWM1 TCC0[3]

    PORT_REGS->GROUP[0].PORT_PMUX[3] = 0x4;
    PORT_REGS->GROUP[0].PORT_PMUX[5] = 0x40;
    PORT_REGS->GROUP[0].PORT_PMUX[9] = 0x5;

    // TEST PWM PINS
    // PORT_REGS->GROUP[1].PORT_PINCFG[30] = 0x1;
    // PORT_REGS->GROUP[1].PORT_PINCFG[31] = 0x1;  // TCC0[1]
    // PORT_REGS->GROUP[0].PORT_PINCFG[10] = 0x1;  // TCC1[0]
    // PORT_REGS->GROUP[0].PORT_PINCFG[11] = 0x1;  // TCC1[1]
    //
    // PORT_REGS->GROUP[0].PORT_PMUX[15] = 0x44;
    // PORT_REGS->GROUP[0].PORT_PMUX[5] = 0x44;
    //
    // PORT_REGS->GROUP[1].PORT_PINCFG[12] = 0x1;
    // PORT_REGS->GROUP[1].PORT_PINCFG[13] = 0x1;
    //
    // PORT_REGS->GROUP[1].PORT_PMUX[6] = 0x55;

    // SERCOM 2 I2C pins
    PORT_REGS->GROUP[0].PORT_PINCFG[12] = 0x1;
    PORT_REGS->GROUP[0].PORT_PINCFG[13] = 0x1;

    PORT_REGS->GROUP[0].PORT_PMUX[6] = 0x22;

    // SERCOM 5 I2C pins
    PORT_REGS->GROUP[1].PORT_PINCFG[16] = 0x1;
    PORT_REGS->GROUP[1].PORT_PINCFG[17] = 0x1;

    PORT_REGS->GROUP[1].PORT_PMUX[8] = 0x22;

    // I2C SERCOM channels
    // SERCOM0 I2C 3
    PORT_REGS->GROUP[0].PORT_PINCFG[8] = 0x1;
    PORT_REGS->GROUP[0].PORT_PINCFG[9] = 0x1;

    PORT_REGS->GROUP[0].PORT_PMUX[4] = 0x22;

    // SERCOM1 I2C 2
    PORT_REGS->GROUP[0].PORT_PINCFG[16] = 0x1;
    PORT_REGS->GROUP[0].PORT_PINCFG[17] = 0x1;

    PORT_REGS->GROUP[0].PORT_PMUX[8] = 0x22;

    // SERCOM3 I2C 1
    PORT_REGS->GROUP[0].PORT_PINCFG[22] = 0x1;
    PORT_REGS->GROUP[0].PORT_PINCFG[23] = 0x1;

    PORT_REGS->GROUP[0].PORT_PMUX[11] = 0x22;

    // CAN 0
    PORT_REGS->GROUP[0].PORT_PINCFG[24] = 0x1;
    PORT_REGS->GROUP[0].PORT_PINCFG[25] = 0x1;

    PORT_REGS->GROUP[0].PORT_PMUX[12] = 0x66;
}

void NVIC_Initialize(void) {
    // NVIC_SetPriority(TCC1_IRQn, 3);
    // NVIC_EnableIRQ(TCC1_IRQn);
    //
    // NVIC_SetPriority(TCC0_IRQn, 3);
    // NVIC_EnableIRQ(TCC0_IRQn);

    NVIC_SetPriority(SERCOM2_IRQn, 3);
    NVIC_EnableIRQ(SERCOM2_IRQn);

    NVIC_SetPriority(SERCOM5_IRQn, 3);
    NVIC_EnableIRQ(SERCOM5_IRQn);

    NVIC_SetPriority(CAN0_IRQn, 3);
    NVIC_EnableIRQ(CAN0_IRQn);

    /* Enable NVIC Controller */
    __DMB();
    __enable_irq();
}

void NVMCTRL_Initialize(void) {
    NVMCTRL_REGS->NVMCTRL_CTRLB = NVMCTRL_CTRLB_READMODE_NO_MISS_PENALTY |
                                  NVMCTRL_CTRLB_SLEEPPRM_WAKEONACCESS |
                                  NVMCTRL_CTRLB_RWS(2) | NVMCTRL_CTRLB_MANW_Msk;
}
