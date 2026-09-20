/*
 * 
 * Source File
 * 
 * Platform:
 *  ATSAMC21 
 * 
 * Company:
 *  Vortex NTNU.
 * 
 * Author:
 *  Markus Sandvik
 * 
 * File Name:
 *  led_facade.c
 *  
 * Summary:
 *  Simple app-facing API for WS2812 status LEDs (non-blocking)
 * 
 * Description:
 *  Simplifies usage of WS2812 SPI Encoder, combined with ws2812 SERCOMx harmony 
 *  binder. 
 *      Functionality similar to Adafruit NeoPixel library (Arduino).
 *          Neopixel    -    led_facade
 *          begin()     -    led_init()
 *          pixel_set() -    led_set()
 *          clear()     -    led_clear_all()
 *          show()      -    led_commit_async()
 */

#include "ws2812_spi_enc.h"
#include "ws2812_port_sercom0_harmony.h"   // Harmony-backed port
#include "led_facade.h"
#include <stdio.h>                

#ifndef LED_COUNT
#define LED_COUNT 5 //Comment: Might be better to set in main/general config file
#endif

static ws2812_grb_t s_px[LED_COUNT];
static uint8_t      s_enc[LED_COUNT * WS2812_SPI_BYTES_PER_LED];
static volatile bool s_inflight = false;

static void _led_done(void *ctx) { (void)ctx; s_inflight = false; }

void led_init(void) 
{
    // SERCOM pins/clock and TC3 are configured by SYS_Initialize() (MCC/MHC).
    ws2812enc_init();                 // build LUT
    ws_led_bind_port();    // bind async SPI + TC3 latch
    for (unsigned i = 0; i < LED_COUNT; ++i) {
        s_px[i].g = s_px[i].r = s_px[i].b = 0;
    }
}

void led_set(uint8_t i, uint8_t r, uint8_t g, uint8_t b)
{
    if (i >= LED_COUNT) return;
    s_px[i].g = g; s_px[i].r = r; s_px[i].b = b;  // GRB order
}
    
bool led_busy(void) { return s_inflight; }

void led_clear_all(void) { 
    for (unsigned i = 0; i < LED_COUNT; ++i)
        led_set(i, 0, 0, 0);
}

bool led_commit_async(void) //Must be called to send changes to bitstream
{   
    if (s_inflight) return false;
    ws2812enc_encode_grb(s_px, LED_COUNT, s_enc);
    s_inflight = true;
    size_t len = ws2812enc_buffer_bytes(LED_COUNT);
    return ws2812_send_encoded_async(s_enc, len, _led_done, NULL);
}
