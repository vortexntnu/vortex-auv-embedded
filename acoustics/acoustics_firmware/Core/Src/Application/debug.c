/*
 * debug.c
 *
 *  Created on: 24. apr. 2026
 *      Author: vikin
 */

#include "debug.h"
#include "main.h"
#include <stdio.h>

#include "ad7606_driver.h"

int _write(int file, char *ptr, int len)
{
    // Only send if ITM is enabled and debugger connected
    if ((CoreDebug->DEMCR & CoreDebug_DEMCR_TRCENA_Msk) &&
        (ITM->TCR & ITM_TCR_ITMENA_Msk))
    {
        for (int i = 0; i < len; i++)
            ITM_SendChar((uint8_t)ptr[i]);
    }
    return len;
}


// claude generated
static int32_t f32_whole(float32_t x) {
    return (int32_t)x;
}

static int32_t f32_frac(float32_t x, uint8_t decimals) {
    int32_t whole = (int32_t)x;
    float32_t frac = x - (float32_t)whole;
    if (frac < 0) frac = -frac;
    uint32_t scale = 1;
    for (uint8_t i = 0; i < decimals; i++) scale *= 10;
    return (int32_t)(frac * scale);
}

void debug_dump_python_array_q15(q15_t* arr, int len) {
    if (arr == NULL || len <= 0) return;

    printf("[");
    for (int i = 0; i < len - 1; i++) {
        printf("%d,", arr[i]);
    }
    printf("%d]", arr[len - 1]);
}

void debug_dump_python_array_f32(float32_t* arr, int len){
    if (arr == NULL || len <= 0) return;

    printf("[");
    for (int i = 0; i < len; i++) {
        if (arr[i] < 0.0f && f32_whole(arr[i]) == 0)
            printf("-");
        printf("%ld.%06ld", f32_whole(arr[i]), f32_frac(arr[i], 6));
        if (i < len - 1) printf(",");
    }
    printf("]");
}

void debug_print_binary(uint16_t value, uint8_t bits) {
    for (int i = bits - 1; i >= 0; i--) {
        printf("%c", (value >> i) & 1 ? '1' : '0');
    }
}

void debug_read_all_registers_binary(struct ad7606_device* my_ADC){
	for(int i = 0; i < 44; i++){
		printf("Register %#04x:\t",my_ADC->registers->all[i].address);
		uint8_t data = ad7606_read_register(my_ADC, my_ADC->registers->all[i]);
		debug_print_binary(data,8);
		printf("\r\n");
	}
}

void debug_apply_threshold(float32_t* signal, uint32_t signal_len, float32_t threshold){
	for(int i = 0; i < signal_len; i++){
		signal[i] = 100*(signal[i] > threshold);
	}
}

void debug_print_direction(float32_t* direction_of_arrival, float32_t snr){
	printf("{");
	debug_dump_python_array_f32(direction_of_arrival, 3);
	printf(",");
	printf("%ld.%06ld", f32_whole(snr), f32_frac(snr, 6));
	printf("},\r\n\t");
}
