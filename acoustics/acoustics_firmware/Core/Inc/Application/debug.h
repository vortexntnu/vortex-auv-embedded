/*
 * debug.h
 *
 *  Created on: 24. apr. 2026
 *      Author: vikin
 */

#ifndef INC_DEBUG_H_
#define INC_DEBUG_H_

#include "main.h"
#include "ad7606_driver.h"

int _write(int file, char *ptr, int len);

typedef enum{
	DEBUG_OK = 0,
	DEBUG_ERROR,
	DEBUG_TIMED_OUT,
} debug_return_status;

void debug_dump_python_array_q15(q15_t* arr, int len);

void debug_dump_python_array_f32(float32_t* arr, int len);

void debug_print_binary(uint16_t value, uint8_t bits);

void debug_read_all_registers_binary(struct ad7606_device* my_ADC);

void debug_apply_threshold(float32_t* signal, uint32_t signal_len, float32_t threshold);


#define DEBUG_DUMP_ARRAY_NAMED_DICT_Q15(name, arr, len) do { \
    printf("\t\"" name "\" : ");                        \
    fflush(stdout);                                     \
    debug_dump_python_array_q15((q15_t*)(arr), (len));        \
    printf("\r\n");                                     \
} while(0)

#define DEBUG_DUMP_ARRAY_NAMED_DICT_F32(name, arr, len) do { \
    printf("\t\"" name "\" : ");                        \
    fflush(stdout);                                     \
    dump_python_array_f32((float32_t*)(arr), (len));    \
    printf("\r\n");                                     \
} while(0)

#define DEBUG_DUMP_ARRAY_NAMED_Q15(name, arr, len) do { \
    printf(name " = ");                            \
    fflush(stdout);                                \
    debug_dump_python_array_q15((q15_t*)(arr), (len));   \
    printf("\r\n");                                \
} while(0)

#define DEBUG_DUMP_ARRAY_NAMED_F32(name, arr, len) do { \
    printf(name " = ");                            \
    fflush(stdout);                                \
    debug_dump_python_array_f32((float32_t*)(arr), (len)); \
    printf("\r\n");                                \
} while(0)


#endif /* INC_DEBUG_H_ */
