/*
 * File:   other.c
 * Author: nathaniel
 *
 * Created on January 19, 2025, 4:28 PM
 */

#include "sam.h"
#include "samc21e17a.h"

void PIN_Initialize(void) {
    // 16MHz clock
    PORT_REGS->GROUP[0].PORT_PINCFG[14] = 0x1;  // XIN
    PORT_REGS->GROUP[0].PORT_PINCFG[15] = 0x1;  // XOUT

    PORT_REGS->GROUP[0].PORT_PMUX[7] = 0x77;

    // ADC Pins
    PORT_REGS->GROUP[0].PORT_PINCFG[2] = 0x1;  // CS1
    PORT_REGS->GROUP[0].PORT_PINCFG[3] = 0x1;  // CS2
    PORT_REGS->GROUP[0].PORT_PINCFG[4] = 0x1;  // CS3

    PORT_REGS->GROUP[0].PORT_PMUX[1] = 0x11;
    PORT_REGS->GROUP[0].PORT_PMUX[2] = 0x01;

    // PWM Pins
    PORT_REGS->GROUP[0].PORT_PINCFG[7] = 0x1;   // PWM3 TCC1[1]
    PORT_REGS->GROUP[0].PORT_PINCFG[10] = 0x1;  // PWM2 TCC1[0]
    PORT_REGS->GROUP[0].PORT_PINCFG[19] = 0x1;  // PWM1 TCC0[3]

    PORT_REGS->GROUP[0].PORT_PMUX[3] = 0x40;
    PORT_REGS->GROUP[0].PORT_PMUX[5] = 0x4;
    PORT_REGS->GROUP[0].PORT_PMUX[9] = 0x50;

    // SERVO Enable pins
    PORT_REGS->GROUP[0].PORT_DIR |= (1 << 27) | (1 << 28) | (1 << 0);

    // I2C SERCOM channels
    // SERCOM0 I2C 3
    PORT_REGS->GROUP[0].PORT_PINCFG[8] = 0x1;
    PORT_REGS->GROUP[0].PORT_PINCFG[9] = 0x1;

    PORT_REGS->GROUP[0].PORT_PMUX[4] = 0x22;  // SERCOM0 (encoder) OR USART
    // PORT_REGS->GROUP[0].PORT_PMUX[4] = 0x33; // SERCOM2 (backup i2c)

    // SERCOM1 I2C 2
    PORT_REGS->GROUP[0].PORT_PINCFG[16] = 0x1;
    PORT_REGS->GROUP[0].PORT_PINCFG[17] = 0x1;

    PORT_REGS->GROUP[0].PORT_PMUX[8] = 0x22;

    // SERCOM 2 I2C pins
    PORT_REGS->GROUP[0].PORT_PINCFG[12] = 0x1;
    PORT_REGS->GROUP[0].PORT_PINCFG[13] = 0x1;

    PORT_REGS->GROUP[0].PORT_PMUX[6] = 0x22;

    // SERCOM3 I2C 1
    PORT_REGS->GROUP[0].PORT_PINCFG[22] = 0x1;
    PORT_REGS->GROUP[0].PORT_PINCFG[23] = 0x1;

    PORT_REGS->GROUP[0].PORT_PMUX[11] = 0x22;

    // CAN0
    PORT_REGS->GROUP[0].PORT_PINCFG[24] = 0x1;
    PORT_REGS->GROUP[0].PORT_PINCFG[25] = 0x1;

    PORT_REGS->GROUP[0].PORT_PMUX[12] = 0x66;
}

void NVIC_Initialize(void) {
    // NVIC_SetPriority(TCC1_IRQn, 3);
    // NVIC_EnableIRQ(TCC1_IRQn);

    // NVIC_SetPriority(TCC0_IRQn, 3);
    // NVIC_EnableIRQ(TCC0_IRQn);

    // I2C 3
    NVIC_SetPriority(SERCOM0_IRQn, 3);
    NVIC_EnableIRQ(SERCOM0_IRQn);

    // I2C 2
    /* For nested I2C interrupts inside CAN Callback to work
     * priority must be set higher than CAN interrupt
     * */
    NVIC_SetPriority(SERCOM1_IRQn, 2);
    NVIC_EnableIRQ(SERCOM1_IRQn);

    // I2C 3
    NVIC_SetPriority(SERCOM3_IRQn, 3);
    NVIC_EnableIRQ(SERCOM3_IRQn);

    NVIC_SetPriority(SERCOM2_IRQn, 3);
    NVIC_EnableIRQ(SERCOM2_IRQn);

    NVIC_SetPriority(CAN0_IRQn, 3);
    NVIC_EnableIRQ(CAN0_IRQn);

    NVIC_SetPriority(DMAC_IRQn, 3);
    NVIC_EnableIRQ(DMAC_IRQn);

    /* Enable NVIC Controller */
    __DMB();
    __enable_irq();
}

void NVMCTRL_Initialize(void) {
    NVMCTRL_REGS->NVMCTRL_CTRLB = NVMCTRL_CTRLB_READMODE_NO_MISS_PENALTY |
                                  NVMCTRL_CTRLB_SLEEPPRM_WAKEONACCESS |
                                  NVMCTRL_CTRLB_RWS(2) | NVMCTRL_CTRLB_MANW_Msk;
}

void EVSYS_Initialize(void) { /*Event Channel User Configuration*/
    EVSYS_REGS->EVSYS_USER[28] = EVSYS_USER_CHANNEL(0x1);

    /* Event Channel 0 Configuration */
    EVSYS_REGS->EVSYS_CHANNEL[0] =
        EVSYS_CHANNEL_EVGEN(3) | EVSYS_CHANNEL_PATH(2) |
        EVSYS_CHANNEL_EDGSEL(0) | EVSYS_CHANNEL_RUNSTDBY_Msk;
}
