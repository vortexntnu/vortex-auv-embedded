/*
 Platform:
    ATSAMC21 

 Company:
    Vortex NTNU

 Author:
    Markus Sandvik

 File Name:
    led_facade.h
 
 - Simple app-facing API for WS2812 status LEDs (non-blocking)
 */

#ifndef LED_FACADE_H
#define LED_FACADE_H

#include <stdbool.h>
#include <stdint.h>
#include "ws2812_spi_enc.h"

#ifdef __cplusplus
extern "C" {
#endif

void led_init(void);                         // init SERCOM0, TC3, encoder, bind async
void led_set(uint8_t i, uint8_t r, uint8_t g, uint8_t b);
bool led_commit_async(void);                 // returns false if a frame is already in-flight
bool led_busy(void);                         // query if TX+latch in progress
void led_clear_all(void);
uint8_t led_count(void);
void led_fill_all(uint8_t r, uint8_t g, uint8_t b);

const ws2812_grb_t* led_facade_pixels(void);

#ifdef __cplusplus
}
#endif

#endif /* LED_FACADE_H */
