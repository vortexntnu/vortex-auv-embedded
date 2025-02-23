/* 
 * File:   supc.h
 * Author: heik
 *
 * Created on February 23, 2025, 12:30 AM
 */

#ifndef SUPC_H
#define	SUPC_H

#ifdef	__cplusplus
extern "C" {
#endif
    
/* 
 * 0: bod33 threshold not crossed 
 * 1: bod33 threshold crossed
 * 
 * Is used by the interrupt handler to signal when bod33 threshold value is 
 * crossed. To detect multiple crosses it needs to be reset in the method which 
 * checks it.
 */
extern volatile int8_t bod33_crossed;

/* Enables BOD33 interrupts */
void SUPC_Initialize(void);


#ifdef	__cplusplus
}
#endif

#endif	/* SUPC_H */

