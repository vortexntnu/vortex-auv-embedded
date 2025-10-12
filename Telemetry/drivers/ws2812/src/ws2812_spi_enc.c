/*
 * ws2812_spi_enc.c  (non-blocking core)
 */

#include "ws2812_spi_enc.h"

/* Internal LUT: each input byte expands to 24 bits = 3 output bytes */
static uint8_t s_lut[256][3];

/* Async port callbacks */
static ws_async_start_fn_t g_start_fn = 0;
static ws_async_busy_fn_t  g_busy_fn  = 0;
static ws_async_delay_fn_t g_delay_fn = 0;

/* Forward internal callbacks */
static void _on_tx_done(void *user);
static void _on_latch_done(void *user);

/* Small state for sequencing TX -> latch -> user done */
typedef struct {
    ws_user_done_cb_t user_done;
    void *user_ctx;
} ws_state_t;

static void build_lut_3x(void)
{
    #define SYM0 0x4u /* 0b100 */
    #define SYM1 0x6u /* 0b110 */
    for (unsigned v = 0; v < 256; ++v) {
        uint32_t stream = 0;
        int bitpos = 23;
        for (int b = 7; b >= 0; --b) {
            unsigned in_bit = (v >> b) & 1u;
            unsigned sym = in_bit ? SYM1 : SYM0;
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

void ws2812enc_init(void) { build_lut_3x(); }

size_t ws2812enc_buffer_bytes(size_t num_leds) { return num_leds * (size_t)WS2812_SPI_BYTES_PER_LED; }

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
    size_t w = 0, in_idx = 0;
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

/* --- Async port binding --- */
void ws2812enc_set_async_port(ws_async_start_fn_t start_fn,
                              ws_async_busy_fn_t  busy_fn,
                              ws_async_delay_fn_t delay_fn)
{
    g_start_fn = start_fn;
    g_busy_fn  = busy_fn;
    g_delay_fn = delay_fn;
}

/* User-facing async send: already-encoded buffer */
int ws2812_send_encoded_async(const uint8_t *encoded, size_t len, ws_user_done_cb_t user_done_cb, void *user)
{
    if (!g_start_fn || !g_delay_fn) return 0;
    static ws_state_t s;                 /* single in-flight; extend if multi-buffer */
    s.user_done = user_done_cb;
    s.user_ctx  = user;
    /* Start async TX; when hardware finishes last bit, _on_tx_done will run */
    return g_start_fn(encoded, len, _on_tx_done, &s);
}

/* Convenience: encode then send. Caller provides the encoded buffer memory. */
int ws2812_encode_and_send_async(const ws2812_grb_t *pixels, size_t num_leds,
                                 uint8_t *encoded_out, size_t encoded_out_len,
                                 ws_user_done_cb_t user_done_cb, void *user)
{
    const size_t need = ws2812enc_buffer_bytes(num_leds);
    if (encoded_out_len < need) return 0;
    ws2812enc_encode_grb(pixels, num_leds, encoded_out);
    return ws2812_send_encoded_async(encoded_out, need, user_done_cb, user);
}

/* --- Internal async chain: TX done -> start latch timer -> latch done -> user callback --- */
static void _on_tx_done(void *user)
{
    (void)user;
    if (g_delay_fn) {
        /* schedule >= 80us latch delay; then call _on_latch_done */
        g_delay_fn(WS2812_RESET_US, _on_latch_done, user);
    }
}

static void _on_latch_done(void *user)
{
    ws_state_t *s = (ws_state_t *)user;
    if (s && s->user_done) s->user_done(s->user_ctx);
}
