/*
 * ws2812_port_sercom5_harmony.c
 * Harmony-backed async port for WS2812 on SAMC21
 *
 * - Uses SERCOM5 SPI Master PLIB non-blocking write + callback
 * - Uses TC3 Timer PLIB one-shot for >=80 us latch
 * Assumes SERCOM5 and TC3 are configured & pins muxed in MCC/MHC (SYS_Initialize).
 */

#include "definitions.h"          // Harmony PLIBs
#include "ws2812_spi_enc.h"
#include "ws2812_port_sercom5_harmony.h"

static ws_on_tx_done_t    s_tx_done_cb;
static void*              s_tx_done_ctx;
static ws_on_delay_done_t s_delay_cb;
static void*              s_delay_ctx;

/* ---------- SERCOM5 SPI non-blocking TX ---------- */
static void sercom5_spi_cb(uintptr_t context)
{
    (void)context;
    if (s_tx_done_cb) s_tx_done_cb(s_tx_done_ctx);
}

static int start_spi5_write(const uint8_t* data, size_t len,
                            ws_on_tx_done_t on_tx_done, void* user)
{
    s_tx_done_cb  = on_tx_done;
    s_tx_done_ctx = user;

    SERCOM5_SPI_CallbackRegister(sercom5_spi_cb, 0);
    return SERCOM5_SPI_Write((void*)data, len) ? 1 : 0;   // returns immediately
}

static bool spi5_busy(void)
{
    return SERCOM5_SPI_IsBusy();
}

/* ------------------ Latch via TC3 (one-shot >=80 us) ------------------ */
/* PLIB exposes 16-bit Timer APIs and TC_TIMER_CALLBACK. */

static ws_on_delay_done_t s_delay_cb;
static void*              s_delay_ctx;

/* Harmony?s TC_TIMER_CALLBACK is typically: void (*cb)(uint32_t status, uintptr_t ctx) */
static void tc3_cb(uint32_t status, uintptr_t context)
{
    (void)status; (void)context;
    TC3_TimerStop();
    if (s_delay_cb) s_delay_cb(s_delay_ctx);
}

/* Utility wrappers for the 16-bit API names you have */
static inline void tc3_counter_set(uint32_t v)
{
    TC3_Timer16bitCounterSet((uint16_t)v);
}

static inline void tc3_period_set(uint32_t v)
{
    TC3_Timer16bitPeriodSet((uint16_t)v);
}

static int delay_us_tc3(uint32_t us, ws_on_delay_done_t on_delay_done, void* user)
{
    s_delay_cb  = on_delay_done;
    s_delay_ctx = user;

    /* Convert microseconds to TC ticks using the actual TC3 frequency. */
    uint32_t freq  = TC3_TimerFrequencyGet();              // Hz
    uint32_t ticks = (uint32_t)(((uint64_t)us * freq) / 1000000ULL);
    if (ticks == 0) ticks = 1;
    if (ticks > 0xFFFFu) ticks = 0xFFFFu;                  // clamp to 16-bit

    TC3_TimerCallbackRegister((TC_TIMER_CALLBACK)tc3_cb, 0);
    TC3_TimerStop();
    tc3_counter_set(0);
    tc3_period_set(ticks);

    /* Some MCC versions require a command to load period; if you see no callback,
       you can add: TC3_TimerCommandSet(TC_COMMAND_START); instead of TimerStart. */
    TC3_TimerStart();
    return 1;
}

/* -------- Bind Harmony-backed port to encoder -------- */
void ws2812_port_bind_sercom5_harmony(void)
{
    // SERCOM5 & TC3 must be initialized by SYS_Initialize() (MCC/MHC).
    ws2812enc_set_async_port(start_spi5_write, spi5_busy, delay_us_tc3);
}
