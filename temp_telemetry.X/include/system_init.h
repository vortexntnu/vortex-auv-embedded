

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

#define CFG_OS_SINGLE 0x8000
#define CFG_MUX_DIFF_0_1 0x0000
#define CFG_MUX_DIFF_2_3 0x3000
#define CFG_PGA_6_144V 0x0000
#define CFG_MODE_SINGLE 0x0100
#define CFG_DR_128SPS 0x0080
#define CFG_COMP_MODE 0x0010
#define CFG_COMP_POL 0x0008
#define CFG_COMP_LAT 0x0004
#define CFG_COMP_QUE_DIS 0x0003

#define PSM_ADDRESS 0x48
#define REG_CONV 0x00
#define REG_CFG 0x01

#ifdef __cplusplus

extern "C"{

#endif // __cplusplus


void system_init(void);


#ifdef __cplusplus

}

#endif // __cplusplus


#endif // !SYSTEM_INIT_H
