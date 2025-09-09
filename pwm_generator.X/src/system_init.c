/*
 * File:   system_init.c
 * Author: nathaniel
 *
 * Created on January 19, 2025, 4:28 PM
 */

#include "sam.h"
#include "samc21e17a.h"
#include "system_init.h"

static void gpio_init(void) {

    /* LED Pins */
    // PA2: Led_enable
    // PA17: LED Red
    // PA18: LED Yellow
    // PA19: LED Green
    PORT_REGS->GROUP[0].PORT_DIR |=
        (1 << 2) | (1 << 19);

    // PWM Pins

    PORT_REGS->GROUP[0].PORT_PINCFG[4] = 0x1;  // TCC0/C0
    PORT_REGS->GROUP[0].PORT_PINCFG[5] = 0x1;  // TCC0/C1
    PORT_REGS->GROUP[0].PORT_PMUX[2] = 0x44;

    PORT_REGS->GROUP[0].PORT_PINCFG[6] = 0x1;  // TCC1/C0
    PORT_REGS->GROUP[0].PORT_PINCFG[7] = 0x1;  // TCC1/C1
    PORT_REGS->GROUP[0].PORT_PMUX[3] = 0x44;
  

    /*Since we want independent duty cycle on each thruster
     * we cannot use TCC1 channel 1 and 2
     */
    PORT_REGS->GROUP[0].PORT_PINCFG[8] = 0x1;  // TCC1/C2
    PORT_REGS->GROUP[0].PORT_PINCFG[9] = 0x1;  // TCC1/C3
    PORT_REGS->GROUP[0].PORT_PMUX[4] = 0x55;

    PORT_REGS->GROUP[0].PORT_PINCFG[10] = 0x1;  // TCC0/C2
    PORT_REGS->GROUP[0].PORT_PINCFG[11] = 0x1;  // TCC0/C3
    PORT_REGS->GROUP[0].PORT_PMUX[5] = 0x55;

    // TCC2
    PORT_REGS->GROUP[0].PORT_PINCFG[16] = 0x1;
    PORT_REGS->GROUP[0].PORT_PINCFG[17] = 0x1;
    PORT_REGS->GROUP[0].PORT_PMUX[8] = 0x44;
    


    // TCC can only have 4 compare values
    // Therefore we use TC4 for led PWM
    // To use TCC0 instead change port_pmux to 0x55
    PORT_REGS->GROUP[0].PORT_PINCFG[14] = 0x1;  // TC4/C0 (TCC0/C4)
    PORT_REGS->GROUP[0].PORT_PINCFG[15] = 0x1;  // TC4/C1 (TCC0/C5)
    PORT_REGS->GROUP[0].PORT_PMUX[7] = 0x44;

    // I2C SERCOM channels

    // SERCOM3 I2C 1
    // CAN also be used as USART
    PORT_REGS->GROUP[0].PORT_PINCFG[22] = 0x1;
    PORT_REGS->GROUP[0].PORT_PINCFG[23] = 0x1;

    PORT_REGS->GROUP[0].PORT_PMUX[11] = 0x22;

    // CAN0
    PORT_REGS->GROUP[0].PORT_PINCFG[24] = 0x1;
    PORT_REGS->GROUP[0].PORT_PINCFG[25] = 0x1;

    PORT_REGS->GROUP[0].PORT_PMUX[12] = 0x66;
}

static void NVIC_Initialize(void) {
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

    /*NVIC_SetPriority(DMAC_IRQn, 3);*/
    /*NVIC_EnableIRQ(DMAC_IRQn);*/

    /* Enable NVIC Controller */
    __DMB();
    __enable_irq();
}

static void NVMCTRL_Initialize(void) {
    NVMCTRL_REGS->NVMCTRL_CTRLB = NVMCTRL_CTRLB_READMODE_NO_MISS_PENALTY |
                                  NVMCTRL_CTRLB_SLEEPPRM_WAKEONACCESS |
                                  NVMCTRL_CTRLB_RWS(2) | NVMCTRL_CTRLB_MANW_Msk;
}



void system_init(void){
    NVMCTRL_REGS->NVMCTRL_CTRLB = NVMCTRL_CTRLB_RWS(3);
    PM_Initialize();
    gpio_init();
    CLOCK_Initialize();
    NVMCTRL_Initialize();
    TCC0_PWMInitialize();
    TCC1_PWMInitialize();
    TCC2_PWMInitialize();
    TC4_CompareInitialize();
    CAN0_Initialize();

    SERCOM3_USART_Initialize();  // USART for Debugging

    // SERCOM3_SLAVE_I2C_Initialize();

    NVIC_Initialize();
}
