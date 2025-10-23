
/*
 Source File
 
 Platform:
    ATSAMC21 

 Company:
    Vortex NTNU.

 Author:
    Markus Sandvik

 File Name:
    ws2812_port_sercom0_harmony.c
 
 Description:
    Harmony SERCOM0 + DMAC glue for WS2812 encoder (non-blocking, single transfer)
    appending a block of 0x00 bytes to the SPI stream via DMAC. This keeps MOSI low
    long enough (>= 80us) after the last data bit to mark end of data transfer.
 */

#include <string.h>
#include <stdbool.h>
#include <stdint.h>

#include "definitions.h"               // SERCOM0_SPI_*, DMAC PLIB, device headers
#include "ws2812_spi_enc.h"            // ws2812enc_set_async_port(..), WS2812_* macros
#include "ws2812_port_sercom0_harmony.h"

// -----------------------------
//  Configuration / sizing
// -----------------------------
#ifndef WS2812_PORT_MAX_LEDS
#define WS2812_PORT_MAX_LEDS   32   // override at compile time if you need more
#endif

#ifndef WS2812_DMAC_CH
#define WS2812_DMAC_CH         DMAC_CHANNEL_0   // change if you wired a different channel in MCC/MHC
#endif

// Number of extra 0x00 bytes appended after the encoded frame to ensure the
// WS2812 reset (low) time. At 2.4 MHz, 1 byte = 3.33 �s.  32 bytes = 106 �s.
#ifndef WS2812_RESET_PAD_BYTES
#define WS2812_RESET_PAD_BYTES 24u //24bytes = 79,92us
#endif

// Max buffer = encoded frame for N LEDs + reset pad bytes
#define WS_SPI_TX_MAX_BYTES    ((WS2812_PORT_MAX_LEDS * WS2812_SPI_BYTES_PER_LED) + WS2812_RESET_PAD_BYTES)

// Shadow buffer copied into by the caller (encoded frame) + our 0x00 pad block.
static uint8_t tx_shadow[WS_SPI_TX_MAX_BYTES];

//  Forward declarations
static int  start_spi5_write(const uint8_t *data, size_t len,
                             ws_on_tx_done_t on_tx_done, void *user);
static bool spi5_busy(void);
static int  port_delay_us(uint32_t us, ws_user_done_cb_t on_done, void* user);

// Completion callback (provided by encoder/user) invoked after the whole
// SPI+pad stream has been clocked out.
static ws_on_tx_done_t s_tx_done_cb = NULL;
static void *          s_tx_done_ctx = NULL;

static volatile bool s_dma_busy = false;   // true while DMAC is moving bytes

// -----------------------------
//  Minimal delay hook (not used for latch in Option B)
// -----------------------------
static int port_delay_us(uint32_t us, ws_user_done_cb_t on_done, void* user)
{
    (void)us;
    if (on_done) on_done(user);     // immediate completion; no timer used here
    return 1;
}

// -----------------------------
//  DMAC completion callback
// -----------------------------
static void dmac_tx_cb(DMAC_TRANSFER_EVENT event, uintptr_t context)
{
    (void)context;

    // Wait until SPI has shifted the very last byte
    while (SERCOM0_SPI_IsBusy()) { /* spin a few cycles */ }

    s_dma_busy = false;

    // With Option B, the reset time has already been satisfied by the zero pad.
    if (s_tx_done_cb) {
        ws_on_tx_done_t cb = s_tx_done_cb;
        void* ctx = s_tx_done_ctx;
        s_tx_done_cb = NULL; s_tx_done_ctx = NULL;
        cb(ctx);
    }
}

// -----------------------------
//  Encoder <-> Port binding
// -----------------------------
void ws_led_bind_port(void)
{
    // Register our DMAC completion callback once
    DMAC_ChannelCallbackRegister(WS2812_DMAC_CH, dmac_tx_cb, 0);

    // Provide the encoder with our async SPI start, busy probe, and (noop) delay hook
    ws2812enc_set_async_port(start_spi5_write, spi5_busy, port_delay_us);
}

// -----------------------------
//  Start a single contiguous SPI transfer via DMAC
// -----------------------------
static int start_spi5_write(const uint8_t *data, size_t len,
                            ws_on_tx_done_t on_tx_done, void *user)
{
    if (!data || (len == 0u)) {
        return 0;
    }

    if ((len + WS2812_RESET_PAD_BYTES) > sizeof(tx_shadow)) {
        // Encoded frame too large for our configured buffer
        return 0;
    }

    // Copy the encoded frame
    memcpy(tx_shadow, data, len);

    // Append the 0x00 reset pad so MOSI stays low long enough after the frame
    memset(&tx_shadow[len], 0x00, WS2812_RESET_PAD_BYTES);

    // Stash completion callback/ctx so we can notify after the full stream
    s_tx_done_cb  = on_tx_done;
    s_tx_done_ctx = user;

    // Mark busy *before* we kick the channel so spi5_busy() is accurate
    s_dma_busy = true;

    // One contiguous DMAC block, beat-triggered by SERCOM0 TX.
    //  - Source increments through tx_shadow
    //  - Destination is fixed at SERCOM0_REGS->SPIM.SERCOM_DATA
    //  - Beat size is BYTE; trigger action is BEAT (set in MCC)
    bool ok = DMAC_ChannelTransfer(
        WS2812_DMAC_CH,
        (const void*)tx_shadow,
        (const void*)&SERCOM0_REGS->SPIM.SERCOM_DATA,
        (len + WS2812_RESET_PAD_BYTES)
    );

    if (!ok) {
        s_dma_busy = false;
        s_tx_done_cb = NULL;
        s_tx_done_ctx = NULL;
        return 0;
    }

    return 1; // started
}

// -----------------------------
//  Busy probe used by the encoder
// -----------------------------
static bool spi5_busy(void)
{
    if (s_dma_busy)                return true;   // DMAC still feeding
    if (SERCOM0_SPI_IsBusy())      return true;   // last byte still shifting
    return false;
}
