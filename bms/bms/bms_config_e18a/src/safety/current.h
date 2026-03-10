
#include "definitions.h"
#include "peripheral/adc/plib_adc0.h"
#include <stdint.h>


#define CURRENT_THRESHOLD_A     25 
#define SENSOR_SENSITIVITY_V_A  0.04
#define SENSOR_OFFSET_V         1.65



void current_init(void);
void current_check(void);
void current_raise_flags(void);

