
/*
 * File:   tcc.h
 * Author: nathaniel
 *
 * Created on January 19, 2025, 6:34 PM
 */

#ifndef TCC_COMMON_H
#define TCC_COMMON_H

#include "samc21e17a.h"
#include "stdbool.h"
#include "stddef.h"
#ifdef __cplusplus
extern "C" {
#endif

#define PWM_MIN 4200
#define PWM_IDLE 9000
#define PWM_MAX 13800
#define PWM_PERIOD_MICROSECONDS 20000
#define TCC_PERIOD 119999

typedef void (*TCC_CALLBACK)(uint32_t status, uintptr_t context);
// *****************************************************************************

typedef struct {
    TCC_CALLBACK callback_fn;
    uintptr_t context;
} TCC_CALLBACK_OBJECT;

#ifdef __cplusplus
}
#endif

#endif /* TCC_H */
