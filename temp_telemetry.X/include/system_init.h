

#ifndef SYSTEM_INIT_H
#define SYSTEM_INIT_H


#ifdef __cplusplus

extern "C"{

#endif // __cplusplus

#include "same51j20a.h"


void pin_init();


void nvic_init();


void nvm_ctrl_init();


#ifdef __cplusplus

}

#endif // __cplusplus


#endif // !SYSTEM_INIT_H
