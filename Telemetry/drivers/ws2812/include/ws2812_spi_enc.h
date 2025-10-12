/*
 * ws2812_spi_enc.h
 *
 * Non-blocking 3-bit SPI encoder for WS2812/WS2812B.
 * - Encoding: WS bit (1.25us) -> 3 SPI bits at ~2.4 MHz (0->100, 1->110)
 * - SPI: Mode 0, MSB-first, continuous, NO gaps
 * - Latch: keep DIN low for >= 80us after transfer
 *
 * This core is hardware-agnostic. You provide async TX callbacks
 * (typically SPI+IRQ or SPI+DMAC on SAMC21) via ws2812enc_set_async_port().
 */

#ifndef WS2812_SPI_ENC_H
#define WS2812_SPI_ENC_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* --- Constants --- */
#define WS2812_SPI_BYTES_PER_COLOR   (3u)   /* 1 data byte -> 3 SPI bytes */
#define WS2812_SPI_BYTES_PER_LED     (9u)   /* GRB -> 3 * 3 bytes */
#define WS2812_RESET_US              (80u)  /* latch/reset low time (>= 80us) */

/* Pixel format (GRB order) */
typedef struct {
    uint8_t g;
    uint8_t r;
    uint8_t b;
} ws2812_grb_t;

/* --- Core API (encoding only) --- */
void   ws2812enc_init(void);                          /* build LUT */
size_t ws2812enc_buffer_bytes(size_t num_leds);       /* n * 9 */
void   ws2812enc_byte_encode(uint8_t value, uint8_t out3[3]);
void   ws2812enc_encode_grb(const ws2812_grb_t *in, size_t num_leds, uint8_t *out);
void   ws2812enc_encode_bytes_grb(const uint8_t *grb_bytes, size_t num_leds, uint8_t *out);

/* --- Async transmit port (to be implemented by platform) ---
 * start: begin streaming 'data' (len bytes) out of SPI with NO gaps and return immediately.
 *        The port must call 'on_tx_done(user)' when the LAST bit has left the shifter.
 * busy:  true while the SPI/IRQ/DMA is still transmitting the current frame (optional).
 * delay_us: schedule a non-blocking >=us delay and invoke 'on_delay_done(user)' for the latch.
 */
typedef void (*ws_on_tx_done_t)(void *user);
typedef void (*ws_on_delay_done_t)(void *user);

typedef bool (*ws_async_busy_fn_t)(void);
typedef int  (*ws_async_start_fn_t)(const uint8_t *data, size_t len, ws_on_tx_done_t on_tx_done, void *user);
typedef int  (*ws_async_delay_fn_t)(uint32_t us, ws_on_delay_done_t on_delay_done, void *user);

void ws2812enc_set_async_port(ws_async_start_fn_t start_fn,
                              ws_async_busy_fn_t  busy_fn,
                              ws_async_delay_fn_t delay_fn);

/* --- High-level non-blocking send orchestration ---
 * Manages send + latch sequencing using the async port.
 */

/* Start sending an already-encoded buffer (returns immediately). 
 * When complete (including latch), your 'user_done_cb(user)' is called.
 */
typedef void (*ws_user_done_cb_t)(void *user);

int  ws2812_send_encoded_async(const uint8_t *encoded, size_t len, ws_user_done_cb_t user_done_cb, void *user);

/* Convenience: encode then send. Caller provides encoded_out buffer memory. */
int  ws2812_encode_and_send_async(const ws2812_grb_t *pixels, size_t num_leds,
                                  uint8_t *encoded_out, size_t encoded_out_len,
                                  ws_user_done_cb_t user_done_cb, void *user);

#ifdef __cplusplus
}
#endif

#endif /* WS2812_SPI_ENC_H */
