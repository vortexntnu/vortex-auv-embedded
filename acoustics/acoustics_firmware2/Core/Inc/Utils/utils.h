/*
 * utils.h
 *
 *  Created on: 7. mar. 2026
 *      Author: vikin
 */

#ifndef INC_UTILS_UTILS_H_
#define INC_UTILS_UTILS_H_

#include <stdint.h>

void utils_delay(volatile uint32_t count);
void utils_DWT_init(void);
void utils_DWT_delay_ms(uint32_t ms);
void utils_DWT_delay_us(uint32_t us);

double reading_to_voltage(int reading);
double voltage_to_temp(double voltage);

#endif /* INC_UTILS_UTILS_H_ */
