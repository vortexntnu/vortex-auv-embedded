/*
 * led_facade.c - Simple app-facing API for WS2812 status LEDs (non-blocking)
 */

#include "ws2812_spi_enc.h"
#include "ws2812_port_sercom5_harmony.h"   // Harmony-backed port
#include "led_facade.h"

#ifndef LED_COUNT
#define LED_COUNT 4
#endif

static ws2812_grb_t s_px[LED_COUNT];
static uint8_t      s_enc[LED_COUNT * WS2812_SPI_BYTES_PER_LED];
static volatile bool s_inflight = false;

static void _led_done(void *ctx) { (void)ctx; s_inflight = false; }

void led_init(void)
{
    // SERCOM5 pins/clock and TC3 are configured by SYS_Initialize() (MCC/MHC).
    ws2812enc_init();                 // build LUT
    ws2812_port_bind_sercom5_harmony();   // bind async SPI + TC3 latch
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

bool led_commit_async(void)
{
    if (s_inflight) return false;
    ws2812enc_encode_grb(s_px, LED_COUNT, s_enc);
    s_inflight = true;
    size_t len = ws2812enc_buffer_bytes(LED_COUNT);
    return ws2812_send_encoded_async(s_enc, len, _led_done, NULL);
}
