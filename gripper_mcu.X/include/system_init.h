/* 
 * File:   other.h
 * Author: nathaniel
 *
 * Created on January 19, 2025, 4:29 PM
 */

#ifndef SYSTEM_INIT_H
#define	SYSTEM_INIT_H

#define LED0_Toggle() (PORT_REGS->GROUP[0].PORT_OUTTGL = PORT_PA15)

#ifdef	__cplusplus
extern "C" {
#endif
    
void NVIC_Initialize(void); 


void NVMCTRL_Initialize(void);

void PIN_Initialize(void);

void EVSYS_Initialize(void);


#ifdef	__cplusplus
}
#endif

#endif	/* SYSTEM_INIT_H */