/* 
 * File:   port.h
 * Author: heik
 *
 * Created on February 6, 2025, 8:55 PM
 */

#ifndef PORT_H
#define	PORT_H

#define SW0 PORT_PA15
#define LED0 PORT_PA14

#define LED0_Toggle() (PORT_REGS->GROUP[0].PORT_OUTTGL = LED0)

#ifdef	__cplusplus
extern "C" {
#endif

void PORT_Initialize(void);

#ifdef	__cplusplus
}
#endif

#endif	/* PORT_H */

