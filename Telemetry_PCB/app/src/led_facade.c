/*
  Platform:
    ATSAMC21 

 Company:
    Vortex NTNU

 Author:
    Markus Sandvik

 File Name:
    led_facade.c

 * led_facade.c - Simple app-facing API for WS2812 status LEDs (non-blocking)
 * Matches ws2812_spi_enc.h and the SERCOM0+DMAC port binder.
 *
 * Neopixel -> led_facade
 *   begin()     -> led_init()
 *   pixel_set() -> led_set()
 *   clear()     -> led_clear_all()
 *   show()      -> led_commit_async()
 */

#include "ws2812_spi_enc.h"
#include "ws2812_port_sercom0_harmony.h"
#include "led_facade.h"
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#ifndef LED_COUNT
#define LED_COUNT 10
#endif

static ws2812_grb_t s_px[LED_COUNT];
static uint8_t      s_enc[LED_COUNT * WS2812_SPI_BYTES_PER_LED];
static volatile bool s_inflight = false;

static void _led_done(void *ctx)
{
    (void)ctx;
    s_inflight = false;
}

void led_init(void)
{
    ws2812enc_init();      // build LUT
    ws_led_bind_port();    // bind async SPI/DMAC hooks

    // Clear local framebuffer
    for (unsigned i = 0; i < LED_COUNT; ++i) {
        s_px[i].g = 0; s_px[i].r = 0; s_px[i].b = 0;
    }
}

void led_set(uint8_t i, uint8_t r, uint8_t g, uint8_t b)
{
    if (i >= LED_COUNT) return;
    s_px[i].g = g; s_px[i].r = r; s_px[i].b = b;  // GRB order

    // (Optional) debug print remove for production
    // printf("led_set i=%u  GRB=(%u,%u,%u)\r\n", i, g, r, b);
}

bool led_busy(void)
{
    return s_inflight;
}

void led_clear_all(void)
{
    for (unsigned i = 0; i < LED_COUNT; ++i) {
        s_px[i].g = 0; s_px[i].r = 0; s_px[i].b = 0;
    }
}

bool led_commit_async(void)  // must be called to send framebuffer to the strip
{
    if (s_inflight) return false;   // still sending a previous frame

    const size_t len = ws2812enc_buffer_bytes(LED_COUNT);
    ws2812enc_encode_grb(s_px, LED_COUNT, s_enc);

    // Try to start the async transfer; only mark inflight if it actually started
    int ok = ws2812_send_encoded_async(s_enc, len, _led_done, NULL);
    if (ok) {
        s_inflight = true;
        return true;
    }
    // If it didn't start (port busy), make sure we don't block future attempts
    s_inflight = false;
    return false;
}

uint8_t led_count(void)
{
    return (uint8_t)LED_COUNT;
}

void led_fill_all(uint8_t r, uint8_t g, uint8_t b)
{
    for (uint8_t i = 0; i < LED_COUNT; ++i)
        led_set(i, r, g, b);
}
