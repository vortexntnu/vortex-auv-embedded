/* 
 * File:   clock.h
 * Author: nathaniel
 *
 * Created on November 4, 2024, 4:09 PM
 */

#ifndef CLOCK_H
#define	CLOCK_H

#ifdef	__cplusplus
extern "C" {
#endif


#include "sam.h"



void OSCCTRL_Initialize(void);

void OSC32KCTRL_Initialize(void);


void FDPLL0_Initialize(void);


void DFLL_Initialize(void);



void GCLK0_Initialize(void);


void GCLK1_Initialize(void);


void GCLK2_Initialize(void);


void CLOCK_Initialize (void);



#ifdef	__cplusplus
}
#endif

#endif	/* CLOCK_H */

