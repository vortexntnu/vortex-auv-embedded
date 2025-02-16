/* 
 * File:   interrupts.h
 * Author: heik
 *
 * Created on February 6, 2025, 9:19 PM
 */

#ifndef INTERRUPTS_H
#define	INTERRUPTS_H

#ifdef	__cplusplus
extern "C" {
#endif

void Interrupts_Initialize(void);
void NVIC_Initialize(void);
void EIC_Initialize(void);


#ifdef	__cplusplus
}
#endif

#endif	/* NVIC_H */

