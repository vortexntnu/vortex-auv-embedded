/*
 * ws2812_spi_enc.h
 *
 * 3-bit SPI encoder for WS2812/WS2812B on Microchip SAMC21 (or any MCU with SPI).
 * Each WS2812 bit (~1.25 us) is encoded into 3 SPI bits at ~2.4 MHz:
 *   WS '0' => 100  (T_high ? 0.417us, T_low ? 0.833us)
 *   WS '1' => 110  (T_high ? 0.833us, T_low ? 0.417us)
 *
 * Use SPI Mode 0, MSB-first, continuous stream with NO gaps between bytes.
 * Recommended SAMC21 SPI clock: 2.4 MHz (SERCOM GCLK = 48 MHz, BAUD = 9).
 *
 * Public API is hardware-agnostic; you provide a TX callback that pushes bytes
 * to the SPI (blocking or via DMA). A weak ws2812_delay_us() is provided; implement
 * it in your platform layer to ensure ?80us latch after each frame.
 */

#ifndef WS2812_SPI_ENC_H
#define WS2812_SPI_ENC_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>

/* ----- Configuration & constants ----- */
#define WS2812_SPI_BYTES_PER_COLOR   (3u)   /* 1 data byte -> 3 SPI bytes */
#define WS2812_SPI_BYTES_PER_LED     (9u)   /* GRB -> 3 * 3 bytes */
#define WS2812_RESET_US              (80u)  /* latch/reset low time (>= 80us) */

/* WS2812 pixel in GRB order */
typedef struct {
    uint8_t g;
    uint8_t r;
    uint8_t b;
} ws2812_grb_t;

/* Initialize the encoder (builds the 256x3 LUT). Does not touch SPI HW. */
void ws2812enc_init(void);

/* Number of encoded SPI bytes required for N LEDs. */
size_t ws2812enc_buffer_bytes(size_t num_leds);

/* Encode an array of GRB pixels into an SPI byte stream.
 * out must have at least ws2812enc_buffer_bytes(num_leds) bytes.
 */
void ws2812enc_encode_grb(const ws2812_grb_t *in, size_t num_leds, uint8_t *out);

/* Encode flat GRB bytes (len = 3*num_leds) into SPI byte stream. */
void ws2812enc_encode_bytes_grb(const uint8_t *grb_bytes, size_t num_leds, uint8_t *out);

/* Encode a single data byte (one GRB component) into 3 SPI bytes. */
void ws2812enc_byte_encode(uint8_t value, uint8_t out3[3]);

/* ----- Transmit integration ----- */
/* Provide a TX callback that transmits bytes over SPI without inter-byte gaps.
 * Return nonzero on success.
 */
typedef int (*ws2812enc_tx_fn_t)(const uint8_t *data, size_t len);
void ws2812enc_set_tx_callback(ws2812enc_tx_fn_t fn);

/* Convenience: encode and transmit in one call using the TX callback.
 * Returns 0 on failure (no callback set) or nonzero on success.
 */
int ws2812_send_blocking(const ws2812_grb_t *pixels, size_t num_leds);

/* Weak delay hook used by ws2812_send_blocking() for the latch period. 
 * Implement this in your platform layer with ~1us accuracy.
 */
__attribute__((weak)) void ws2812_delay_us(uint32_t us);

/* ----- Optional SAMC21 helpers (declarations only) -----
 * You can define these in your project if you want ready-made TX and init.
 * They are not required by the encoder itself.
 */
#ifdef __SAMD21__  /* some projects define this; adapt if desired */
#endif

/* Example signatures you may implement in your platform code:
 * void samc21_sercom_spi_init_2p4mhz(void);
 * int  samc21_sercom_spi_tx_blocking(const uint8_t *data, size_t len);
 */

#ifdef __cplusplus
}
#endif

#endif /* WS2812_SPI_ENC_H */
