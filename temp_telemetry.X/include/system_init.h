

#ifndef SYSTEM_INIT_H
#define SYSTEM_INIT_H

#include "same51j20a.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/types.h>
#include "clock.h"
#include "sercom0_i2c.h"
#include "sercom3_i2c.h"
#include "systick.h"

#ifdef __cplusplus

extern "C"{

#endif // __cplusplus


void system_init(void);


#ifdef __cplusplus

}

#endif // __cplusplus


#endif // !SYSTEM_INIT_H
