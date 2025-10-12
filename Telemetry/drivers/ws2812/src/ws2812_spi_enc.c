/*
 * ws2812_spi_enc.c
 *
 * Implementation of a 3-bit SPI encoder for WS2812/WS2812B.
 * See ws2812_spi_enc.h for usage notes.
 */

#include "ws2812_spi_enc.h"

/* Internal LUT: each input byte expands to 24 bits = 3 output bytes */
static uint8_t s_lut[256][3];
static ws2812enc_tx_fn_t s_tx_cb = 0;

/* 3-bit symbols (MSB-first inside the 3-bit group)
 * '0' => 1 0 0  (0b100)
 * '1' => 1 1 0  (0b110)
 */
#define SYM0 0x4u
#define SYM1 0x6u

static void build_lut_3x(void)
{
    for (unsigned v = 0; v < 256; ++v) {
        uint32_t stream = 0;    /* 24-bit packed stream (MSB-first) */
        int bitpos = 23;        /* next bit position to fill */

        for (int b = 7; b >= 0; --b) {
            unsigned in_bit = (v >> b) & 1u;
            unsigned sym = in_bit ? SYM1 : SYM0; /* 3-bit symbol */

            /* write 3 bits MSB-first: s2, s1, s0 */
            for (int s = 2; s >= 0; --s) {
                unsigned sb = (sym >> s) & 1u;
                if (sb) stream |= (1u << bitpos);
                --bitpos;
            }
        }
        s_lut[v][0] = (uint8_t)((stream >> 16) & 0xFF);
        s_lut[v][1] = (uint8_t)((stream >>  8) & 0xFF);
        s_lut[v][2] = (uint8_t)( stream        & 0xFF);
    }
}

void ws2812enc_init(void)
{
    build_lut_3x();
}

size_t ws2812enc_buffer_bytes(size_t num_leds)
{
    return num_leds * (size_t)WS2812_SPI_BYTES_PER_LED;
}

void ws2812enc_byte_encode(uint8_t value, uint8_t out3[3])
{
    out3[0] = s_lut[value][0];
    out3[1] = s_lut[value][1];
    out3[2] = s_lut[value][2];
}

void ws2812enc_encode_grb(const ws2812_grb_t *in, size_t num_leds, uint8_t *out)
{
    size_t w = 0;
    for (size_t i = 0; i < num_leds; ++i) {
        const ws2812_grb_t *px = &in[i];
        const uint8_t *lg = s_lut[px->g];
        const uint8_t *lr = s_lut[px->r];
        const uint8_t *lb = s_lut[px->b];
        out[w+0] = lg[0]; out[w+1] = lg[1]; out[w+2] = lg[2];
        out[w+3] = lr[0]; out[w+4] = lr[1]; out[w+5] = lr[2];
        out[w+6] = lb[0]; out[w+7] = lb[1]; out[w+8] = lb[2];
        w += WS2812_SPI_BYTES_PER_LED;
    }
}

void ws2812enc_encode_bytes_grb(const uint8_t *grb_bytes, size_t num_leds, uint8_t *out)
{
    size_t w = 0;
    size_t in_idx = 0;
    for (size_t i = 0; i < num_leds; ++i) {
        const uint8_t *lg = s_lut[grb_bytes[in_idx + 0]]; /* G */
        const uint8_t *lr = s_lut[grb_bytes[in_idx + 1]]; /* R */
        const uint8_t *lb = s_lut[grb_bytes[in_idx + 2]]; /* B */
        out[w+0] = lg[0]; out[w+1] = lg[1]; out[w+2] = lg[2];
        out[w+3] = lr[0]; out[w+4] = lr[1]; out[w+5] = lr[2];
        out[w+6] = lb[0]; out[w+7] = lb[1]; out[w+8] = lb[2];
        w     += WS2812_SPI_BYTES_PER_LED;
        in_idx += 3;
    }
}

void ws2812enc_set_tx_callback(ws2812enc_tx_fn_t fn)
{
    s_tx_cb = fn;
}

/* Weak default; override in your platform layer */
__attribute__((weak)) void ws2812_delay_us(uint32_t us)
{
    (void)us;
}

int ws2812_send_blocking(const ws2812_grb_t *pixels, size_t num_leds)
{
    if (!s_tx_cb) return 0;

    const size_t enc_len = ws2812enc_buffer_bytes(num_leds);

    /* For large LED counts, avoid large VLAs; manage a persistent buffer in app code.
       Here we demonstrate a simple stack allocation for modest strip sizes. */
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wvla"
#endif
    uint8_t *tmp = (enc_len > 0) ? (uint8_t*)__builtin_alloca(enc_len) : 0;
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
    if (!tmp && enc_len) return 0;

    ws2812enc_encode_grb(pixels, num_leds, tmp);
    int ok = s_tx_cb(tmp, enc_len);

    /* Latch/reset: hold line low for >= 80us */
    ws2812_delay_us(WS2812_RESET_US);
    return ok;
}

/* ---------------------------- SAMC21 Notes ----------------------------
 * Configure SERCOMx as SPI master, Mode 0, MSB-first, 8-bit frames.
 * Recommended clock: 2.4 MHz (GCLK_SERCOMx = 48 MHz, BAUD = 9).
 *
 * Pseudocode for init:
 *   - Enable APB clock to SERCOMx; route GCLK 48MHz to SERCOMx core.
 *   - MUX SCK/MOSI pins to SERCOMx pads; choose DOPO accordingly.
 *   - CTRLA: MODE=SPI_MASTER, DOPO=..., DIPO(any), CPOL=0, CPHA=0.
 *   - CTRLB: RX disabled (optional), TX enabled.
 *   - BAUD = 9  => fSPI = 48MHz / (2*(9+1)) = 2.4MHz.
 *   - ENABLE.
 *
 * Blocking TX callback example signature you can implement:
 *   int samc21_sercom_spi_tx_blocking(const uint8_t *data, size_t len) {
 *       for (size_t i = 0; i < len; ++i) {
 *           while (!(SERCOMx->SPI.INTFLAG.bit.DRE)) { }
 *           SERCOMx->SPI.DATA.reg = data[i];
 *       }
 *       while (!SERCOMx->SPI.INTFLAG.bit.TXC) { }
 *       return 1;
 *   }
 *
 * After setting the callback with ws2812enc_set_tx_callback(),
 * call ws2812_send_blocking(pixels, count) whenever you want to update the LEDs.
 * --------------------------------------------------------------------- */
