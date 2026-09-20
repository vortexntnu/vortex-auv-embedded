/*
 Source File
 
 Platform:
    ATSAMC21 

 Company:
    Vortex NTNU.

 Author:
    Markus Sandvik

 File Name:
    ws2812_spi_enc.c

 * ws2812_spi_enc.c: hardware-agnostic encoder + async orchestration
 */

#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "ws2812_spi_enc.h"

/* ---------------- LUT: 1 byte -> 3 SPI bytes (MSB-first) ---------------- */
static uint8_t s_lut[256][3];

static void build_lut_3x(void)
{
    for (uint32_t v = 0; v < 256u; v++) {
        uint32_t stream = 0;
        for (int bit = 7; bit >= 0; bit--) {
            const uint32_t sym = ((v >> bit) & 1u) ? 0b110u : 0b100u; // 1->110, 0->100
            stream = (stream << 3) | sym;
        }
        s_lut[v][0] = (uint8_t)((stream >> 16) & 0xFFu);
        s_lut[v][1] = (uint8_t)((stream >>  8) & 0xFFu);
        s_lut[v][2] = (uint8_t)( stream        & 0xFFu);
    }
}

void ws2812enc_init(void) { build_lut_3x(); }

size_t ws2812enc_buffer_bytes(size_t num_leds)
{
    return num_leds * (size_t)WS2812_SPI_BYTES_PER_LED; // 9 bytes/LED
}

void ws2812enc_byte_encode(uint8_t value, uint8_t out3[3])
{
    out3[0] = s_lut[value][0];
    out3[1] = s_lut[value][1];
    out3[2] = s_lut[value][2];
}

/* --------- Encoding (struct array GRB) matches header prototype -------- */
void ws2812enc_encode_grb(const ws2812_grb_t *in, size_t num_leds, uint8_t *out)
{
    for (size_t i = 0; i < num_leds; i++) {
        const uint8_t g = in[i].g;
        const uint8_t r = in[i].r;
        const uint8_t b = in[i].b;

        uint8_t *p = &out[i * WS2812_SPI_BYTES_PER_LED];

        /*p[0] = s_lut[g][0]; p[1] = s_lut[g][1]; p[2] = s_lut[g][2];
        p[3] = s_lut[r][0]; p[4] = s_lut[r][1]; p[5] = s_lut[r][2];
        p[6] = s_lut[b][0]; p[7] = s_lut[b][1]; p[8] = s_lut[b][2];*/
        
        // TEST: send RGB instead of GRB
        p[0] = s_lut[r][0]; p[1] = s_lut[r][1]; p[2] = s_lut[r][2];
        p[3] = s_lut[g][0]; p[4] = s_lut[g][1]; p[5] = s_lut[g][2];
        p[6] = s_lut[b][0]; p[7] = s_lut[b][1]; p[8] = s_lut[b][2];
    }
}

/* ---- Encoding (flat bytes [G,R,B] per LED) ? matches header prototype ---- */
void ws2812enc_encode_bytes_grb(const uint8_t *grb_bytes, size_t num_leds, uint8_t *out)
{
    for (size_t i = 0; i < num_leds; i++) {
        const uint8_t g = grb_bytes[i*3 + 0];
        const uint8_t r = grb_bytes[i*3 + 1];
        const uint8_t b = grb_bytes[i*3 + 2];

        uint8_t *p = &out[i * WS2812_SPI_BYTES_PER_LED];

        /*p[0] = s_lut[g][0]; p[1] = s_lut[g][1]; p[2] = s_lut[g][2];
        p[3] = s_lut[r][0]; p[4] = s_lut[r][1]; p[5] = s_lut[r][2];
        p[6] = s_lut[b][0]; p[7] = s_lut[b][1]; p[8] = s_lut[b][2];*/
        
        // TEST: send RGB instead of GRB
        p[0] = s_lut[r][0]; p[1] = s_lut[r][1]; p[2] = s_lut[r][2];
        p[3] = s_lut[g][0]; p[4] = s_lut[g][1]; p[5] = s_lut[g][2];
        p[6] = s_lut[b][0]; p[7] = s_lut[b][1]; p[8] = s_lut[b][2];
    }
}

/* ---------------- Async orchestration using the port hooks ---------------- */
static ws_async_start_fn_t g_start_fn = NULL;
static ws_async_busy_fn_t  g_busy_fn  = NULL;
static ws_async_delay_fn_t g_delay_fn = NULL;

typedef struct {
    ws_user_done_cb_t user_done;
    void *            user_ctx;
} ws_state_t;

static void _on_tx_done(void *user);
static void _on_latch_done(void *user);

int ws2812_send_encoded_async(const uint8_t *encoded, size_t len,
                              ws_user_done_cb_t user_done_cb, void *user)
{
    if (!encoded || (len == 0u))    return 0;
    if (!g_start_fn || !g_busy_fn)  return 0;
    if (g_busy_fn())                return 0;

    static ws_state_t s;
    s.user_done = user_done_cb;
    s.user_ctx  = user;

    /* Start async write. Port appends zero-pad and calls _on_tx_done. */
    return g_start_fn(encoded, len, _on_tx_done, &s);
}

int ws2812_encode_and_send_async(const ws2812_grb_t *pixels, size_t num_leds,
                                 uint8_t *encoded_out, size_t encoded_out_len,
                                 ws_user_done_cb_t user_done_cb, void *user)
{
    const size_t need = ws2812enc_buffer_bytes(num_leds);
    if (!pixels || !encoded_out || encoded_out_len < need) return 0;

    ws2812enc_encode_grb(pixels, num_leds, encoded_out);
    return ws2812_send_encoded_async(encoded_out, need, user_done_cb, user);
}

static void _on_tx_done(void *user)
{
    /* If port exposes a non-blocking delay, use it; else zero-pad already handled latch. */
    if (g_delay_fn) {
        g_delay_fn(WS2812_RESET_US, _on_latch_done, user);
    } else {
        _on_latch_done(user);
    }
}

static void _on_latch_done(void *user)
{
    ws_state_t *s = (ws_state_t *)user;
    if (s && s->user_done) s->user_done(s->user_ctx);
}

void ws2812enc_set_async_port(ws_async_start_fn_t start_fn,
                              ws_async_busy_fn_t  busy_fn,
                              ws_async_delay_fn_t delay_fn)
{
    g_start_fn = start_fn;
    g_busy_fn  = busy_fn;
    g_delay_fn = delay_fn;
}
