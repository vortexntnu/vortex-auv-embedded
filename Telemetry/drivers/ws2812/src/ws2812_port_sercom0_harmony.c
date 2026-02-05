
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
#include "ws2812_spi_enc.h"            
#include "ws2812_port_sercom0_harmony.h"

// -----------------------------
//  Configuration / sizing
// -----------------------------
#ifndef WS2812_DMAC_CH
#define WS2812_DMAC_CH         DMAC_CHANNEL_0   // change if you wired a different channel in MCC/MHC
#endif

#ifndef WS2812_SERCOM_SPI_BYTES_MAX
#  define WS2812_SERCOM_SPI_BYTES_MAX   (4096u)   // encoded+pad capacity
#endif

// Number of extra 0x00 bytes appended before and after the encoded frame to ensure the
// WS2812 reset (low) time.

#ifndef WS2812_RESET_PAD_BYTES
#define WS2812_RESET_PAD_BYTES 24u //24bytes = 79,92us
#endif

#ifndef WS2812_PRE_PAD_BYTES
#  define WS2812_PRE_PAD_BYTES 24u //24bytes = 79,92us
#endif

/* ---------------- Private state ---------------- */
static volatile bool   s_dma_busy    = false;
static uint8_t         tx_shadow[WS2812_SERCOM_SPI_BYTES_MAX];
static ws_on_tx_done_t s_tx_done_cb  = NULL;
static void*           s_tx_done_ctx = NULL;

/* ---------------- Helpers ---------------- */
static inline void sercom0_tx_reset(void)
{
    // Drain any stale RX to avoid immediate BUFOVF
    while (SERCOM0_REGS->SPIM.SERCOM_INTFLAG & SERCOM_SPIM_INTFLAG_RXC_Msk) {
        (void)SERCOM0_REGS->SPIM.SERCOM_DATA;
    }
    // Clear overflow if it was set (write-1-to-clear)
    SERCOM0_REGS->SPIM.SERCOM_STATUS = SERCOM_SPIM_STATUS_BUFOVF_Msk;

    // Clear DRE/TXC so next DMA beats are accepted
    SERCOM0_REGS->SPIM.SERCOM_INTFLAG = SERCOM_SPIM_INTFLAG_DRE_Msk
                                      | SERCOM_SPIM_INTFLAG_TXC_Msk;

    // Ensure transmitter is enabled
    SERCOM0_REGS->SPIM.SERCOM_CTRLA |= SERCOM_SPIM_CTRLA_ENABLE_Msk;
    (void)SERCOM0_REGS->SPIM.SERCOM_SYNCBUSY;
}

static inline void dmac_ch_reset(void)
{
    // Disable via PLIB, then clear per-channel flags via device registers
    DMAC_ChannelDisable(WS2812_DMAC_CH);

    // Select channel, then clear TERR/TCMPL flags
    DMAC_REGS->DMAC_CHID = DMAC_CHID_ID(WS2812_DMAC_CH);
    DMAC_REGS->DMAC_CHINTFLAG = DMAC_CHINTFLAG_TERR_Msk | DMAC_CHINTFLAG_TCMPL_Msk;
}

/* ---------------- DMAC completion callback ---------------- */
static void dmac_tx_cb(DMAC_TRANSFER_EVENT event, uintptr_t context)
{
    (void)event; (void)context;

    // Wait for true transmit complete: shift register empty
    while ((SERCOM0_REGS->SPIM.SERCOM_INTFLAG & SERCOM_SPIM_INTFLAG_TXC_Msk) == 0) {
    }

    // Drain RX and clear overflow (defensive)
    while (SERCOM0_REGS->SPIM.SERCOM_INTFLAG & SERCOM_SPIM_INTFLAG_RXC_Msk) {
        (void)SERCOM0_REGS->SPIM.SERCOM_DATA;
    }
    SERCOM0_REGS->SPIM.SERCOM_STATUS = SERCOM_SPIM_STATUS_BUFOVF_Msk;

    // Clear TXC now that we've observed it
    SERCOM0_REGS->SPIM.SERCOM_INTFLAG = SERCOM_SPIM_INTFLAG_TXC_Msk;

    // Be explicit: disable the channel after completion
    DMAC_ChannelDisable(WS2812_DMAC_CH);

    s_dma_busy = false;

    if (s_tx_done_cb) {
        ws_on_tx_done_t cb = s_tx_done_cb;
        void*           uc = s_tx_done_ctx;
        s_tx_done_cb = NULL;
        s_tx_done_ctx = NULL;
        cb(uc);
    }
}


/* ---------------- Async port hooks (match ws_async_* typedefs) ----------- */
static int start_spi5_write(const uint8_t *data, size_t len,
                            ws_on_tx_done_t on_tx_done, void *user)
{
    if (!data || (len == 0u)) return 0;
    if (s_dma_busy)           return 0;

    const size_t total =
        (size_t)WS2812_PRE_PAD_BYTES +
        (size_t)len +
        (size_t)WS2812_RESET_PAD_BYTES;

    if (total > sizeof(tx_shadow)) {
        return 0; // too large
    }

    // ---- PRE pad: force DIN low before first symbol (reset/idle) ----
    memset(tx_shadow, 0x00, WS2812_PRE_PAD_BYTES);

    // ---- DATA ----
    memcpy(&tx_shadow[WS2812_PRE_PAD_BYTES], data, len);

    // ---- POST pad: keep DIN low long enough to latch/reset ----
    memset(&tx_shadow[WS2812_PRE_PAD_BYTES + len], 0x00, WS2812_RESET_PAD_BYTES);

    s_tx_done_cb  = on_tx_done;
    s_tx_done_ctx = user;

    sercom0_tx_reset();
    dmac_ch_reset();

    // Make sure our channel will raise TCMPL interrupts
    DMAC_REGS->DMAC_CHID = DMAC_CHID_ID(WS2812_DMAC_CH);
    DMAC_REGS->DMAC_CHINTENSET = DMAC_CHINTENSET_TCMPL_Msk;

    s_dma_busy = true;
    bool ok = DMAC_ChannelTransfer(
        WS2812_DMAC_CH,
        (const void *)tx_shadow,
        (const void *)&SERCOM0_REGS->SPIM.SERCOM_DATA,
        total
    );

    if (!ok) {
        s_dma_busy = false;
        s_tx_done_cb = NULL;
        s_tx_done_ctx = NULL;
        return 0;
    }
    return 1;
}


static bool spi5_busy(void)
{
    if (s_dma_busy)           return true;
    if (SERCOM0_SPI_IsBusy()) return true;
    return false;
}

static int port_delay_us(uint32_t us, ws_on_delay_done_t on_delay_done, void *user)
{
    (void)us;
    if (on_delay_done) on_delay_done(user);
    return 1;
}

/* ---------------- Public glue -------------------------------------------- */
void ws_led_bind_port(void)
{
    // Disable RX to prevent RXC/BUFOVF from ever setting while we TX-only
    SERCOM0_REGS->SPIM.SERCOM_CTRLB &= ~SERCOM_SPIM_CTRLB_RXEN_Msk;
    while (SERCOM0_REGS->SPIM.SERCOM_SYNCBUSY) { }

    // DMAC -> our completion callback
    DMAC_ChannelCallbackRegister(WS2812_DMAC_CH, dmac_tx_cb, 0);

    // Enable DMAC global IRQ and our channel's TCMPL interrupt
    NVIC_EnableIRQ(DMAC_IRQn);
    DMAC_REGS->DMAC_CHID = DMAC_CHID_ID(WS2812_DMAC_CH);
    DMAC_REGS->DMAC_CHINTENSET = DMAC_CHINTENSET_TCMPL_Msk;

    // Hand hooks to the encoder core
    ws2812enc_set_async_port(start_spi5_write, spi5_busy, port_delay_us);
}
