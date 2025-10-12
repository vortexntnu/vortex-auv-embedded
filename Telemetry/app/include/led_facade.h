/*
 * led_facade.h - Simple app-facing API for WS2812 status LEDs (non-blocking)
 */

#ifndef LED_FACADE_H
#define LED_FACADE_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void led_init(void);                         // init SERCOM5, TC3, encoder, bind async
void led_set(uint8_t i, uint8_t r, uint8_t g, uint8_t b);
bool led_commit_async(void);                 // returns false if a frame is already in-flight
bool led_busy(void);                         // query if TX+latch in progress

#ifdef __cplusplus
}
#endif

#endif /* LED_FACADE_H */
