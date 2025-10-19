#include "ads8555_driver.h"
#include <string.h>

/* GPIO helpers */
static inline void fs_low(void) {
    HAL_GPIO_WritePin(ADS8555_FS_GPIO_Port, ADS8555_FS_Pin, GPIO_PIN_RESET);
}
static inline void fs_high(void) {
    HAL_GPIO_WritePin(ADS8555_FS_GPIO_Port, ADS8555_FS_Pin, GPIO_PIN_SET);
}

#ifdef ADS8555_CONV_Pin
static inline void conv_low(void) {
    HAL_GPIO_WritePin(ADS8555_CONV_GPIO_Port, ADS8555_CONV_Pin, GPIO_PIN_RESET);
}
static inline void conv_high(void) {
    HAL_GPIO_WritePin(ADS8555_CONV_GPIO_Port, ADS8555_CONV_Pin, GPIO_PIN_SET);
}
#endif

#ifdef ADS8555_BUSY_Pin
static inline GPIO_PinState busy_read(void) {
    return HAL_GPIO_ReadPin(ADS8555_BUSY_GPIO_Port, ADS8555_BUSY_Pin);
}
#endif

/* Map fields → 16‑bit control (update to your chosen bit map per datasheet) */
static uint16_t pack_ctrl(const ADS8555_CtrlFields* c) {
    uint16_t w = 0;
    w |= ((c->sdo_mode & 0x3) << 14); /* 3‑SDO => 2 */
    w |= ((c->pwr_down & 0x1) << 13);
    w |= ((c->range_a & 0x3) << 8);
    w |= ((c->range_b & 0x3) << 6);
    w |= ((c->range_c & 0x3) << 4);
    w |= (c->other & 0xF);
    return w;
}

static inline uint32_t half_index(const ADS8555_Handle* dev,
                                  SPI_HandleTypeDef* hspi) {
    (void)dev;
    if (hspi == dev->hspi_sdoA)
        return 0;
    if (hspi == dev->hspi_sdoB)
        return 1;
    return 2; /* sdoC */
}

void ADS8555_SetControl(ADS8555_Handle* dev, const ADS8555_CtrlFields* ctrl) {
    dev->ctrl = *ctrl;
    dev->ctrl_word = pack_ctrl(ctrl);
}

HAL_StatusTypeDef ADS8555_Init(ADS8555_Handle* dev,
                               SPI_HandleTypeDef* spi_master,
                               SPI_HandleTypeDef* spi_sdoA,
                               SPI_HandleTypeDef* spi_sdoB,
                               SPI_HandleTypeDef* spi_sdoC,
                               MDMA_HandleTypeDef* mdma_a,
                               MDMA_HandleTypeDef* mdma_b,
                               MDMA_HandleTypeDef* mdma_c,
                               const ADS8555_CtrlFields* ctrl_init,
                               ADS8555_OutputReadyCB cb) {
    if (!dev || !spi_master || !spi_sdoA || !spi_sdoB || !spi_sdoC)
        return HAL_ERROR;
    memset(dev, 0, sizeof(*dev));
    dev->hspi_master = spi_master;
    dev->hspi_sdoA = spi_sdoA;
    dev->hspi_sdoB = spi_sdoB;
    dev->hspi_sdoC = spi_sdoC;
    dev->hmdma_a = mdma_a;
    dev->hmdma_b = mdma_b;
    dev->hmdma_c = mdma_c;
    dev->on_ready = cb;

    ADS8555_CtrlFields c = {0};
    if (ctrl_init)
        c = *ctrl_init;
    else {
        c.sdo_mode = 2; /* 3‑SDO */
        c.range_a = c.range_b = c.range_c = 0;
        c.pwr_down = 0;
        c.other = 0;
    }
    ADS8555_SetControl(dev, &c);

    fs_high();
#ifdef ADS8555_CONV_Pin
    conv_high();
#endif

    return HAL_OK;
}

/* Start continuous: kick 3× RX DMAs (circular). Master SPI4 will be used
 * per‑frame when you toggle FS and send control/SCLK. */
HAL_StatusTypeDef ADS8555_Start(ADS8555_Handle* dev) {
#if ADS8555_USE_32BIT_FRAMES
    HAL_StatusTypeDef sa = HAL_SPI_Receive_DMA(
        dev->hspi_sdoA, (uint8_t*)dev->rxA, ADS8555_RX_BUF_WORDS);
    HAL_StatusTypeDef sb = HAL_SPI_Receive_DMA(
        dev->hspi_sdoB, (uint8_t*)dev->rxB, ADS8555_RX_BUF_WORDS);
    HAL_StatusTypeDef sc = HAL_SPI_Receive_DMA(
        dev->hspi_sdoC, (uint8_t*)dev->rxC, ADS8555_RX_BUF_WORDS);
#else
    /* 16‑bit frames */
    HAL_StatusTypeDef sa = HAL_SPI_Receive_DMA(
        dev->hspi_sdoA, (uint8_t*)dev->rxA, ADS8555_RX_BUF_WORDS * 2);
    HAL_StatusTypeDef sb = HAL_SPI_Receive_DMA(
        dev->hspi_sdoB, (uint8_t*)dev->rxB, ADS8555_RX_BUF_WORDS * 2);
    HAL_StatusTypeDef sc = HAL_SPI_Receive_DMA(
        dev->hspi_sdoC, (uint8_t*)dev->rxC, ADS8555_RX_BUF_WORDS * 2);
#endif
    if (sa || sb || sc)
        return HAL_ERROR;
    return HAL_OK;
}

void ADS8555_Stop(ADS8555_Handle* dev) {
    (void)HAL_SPI_DMAStop(dev->hspi_sdoA);
    (void)HAL_SPI_DMAStop(dev->hspi_sdoB);
    (void)HAL_SPI_DMAStop(dev->hspi_sdoC);
}

void ADS8555_TriggerConversion(void) {
#ifdef ADS8555_CONV_Pin
    conv_low();
    for (volatile int i = 0; i < 10; ++i)
        __NOP();
    conv_high();
#endif
}

bool ADS8555_WaitBusy(uint32_t timeout_us) {
#ifdef ADS8555_BUSY_Pin
    uint32_t start = DWT->CYCCNT;
    uint32_t ticks = (SystemCoreClock / 1000000u) * timeout_us;
    while (busy_read() == GPIO_PIN_SET) {
        if (timeout_us && ((DWT->CYCCNT - start) > ticks))
            return false;
    }
    return true;
#else
    (void)timeout_us;
    return true;
#endif
}

/* ---------------- DMA rendezvous + MDMA interleave ---------------- */
static void try_kick_mdma(ADS8555_Handle* dev, uint8_t half) {
    if (dev->half_ready[half] != 0b00000111)
        return;  // need A,B,C
    dev->half_ready[half] = 0;

    /* Source pointers: each half is ADS8555_RX_BUF_WORDS/2 elements */
    const uint32_t n = ADS8555_RX_BUF_WORDS / 2;
    uint32_t* srcA = &dev->rxA[half * n];
    uint32_t* srcB = &dev->rxB[half * n];
    uint32_t* srcC = &dev->rxC[half * n];
    ADS8555_Sample3* dst = dev->out[half];

    /* MDMA interleave: three channels into AoS: [A,B,C] repeating. Each sample
     * is 16‑bit signed per channel in the 32‑bit word: A0|A1 ⇒ we only need the
     * low 16 for CHx0 and next 16 for CHx1 per frame; however the ADC provides
     * 32 bits per SDO per frame (two channels). Here, each SPI element
     * corresponds to one frame already, mapping is application‑specific. For
     * simplicity, we store pairs A0,A1 into two consecutive outputs. Adjust as
     * needed. */
    /* Minimal, portable approach: do it with CPU if no MDMA provided. */
    if (!dev->hmdma_a || !dev->hmdma_b || !dev->hmdma_c) {
        for (uint32_t i = 0; i < n; ++i) {
            uint32_t wa = srcA[i];
            uint32_t wb = srcB[i];
            uint32_t wc = srcC[i];
            /* Unpack MSB‑first 16‑bit words from each 32‑bit */
            int16_t a0 = (int16_t)(wa >> 16);
            int16_t a1 = (int16_t)(wa & 0xFFFF);
            int16_t b0 = (int16_t)(wb >> 16);
            int16_t b1 = (int16_t)(wb & 0xFFFF);
            int16_t c0 = (int16_t)(wc >> 16);
            int16_t c1 = (int16_t)(wc & 0xFFFF);
            /* Write two AoS samples (A0,B0,C0) then (A1,B1,C1) */
            dst[2 * i + 0] = (ADS8555_Sample3){a0, b0, c0};
            dst[2 * i + 1] = (ADS8555_Sample3){a1, b1, c1};
        }
        if (dev->on_ready)
            dev->on_ready(dev, half);
        return;
    }

    /* If MDMA is provided, you can set up three MDMA transfers with
     * DstInc=sizeof(ADS8555_Sample3), starting at &dst[0].a, &dst[0].b,
     * &dst[0].c respectively, and SrcInc=4 bytes. Because each 32‑bit source
     * contains two channels (e.g., A0|A1), you may want a two‑pass MDMA or
     * linked list to place A0/B0/C0 then A1/B1/C1. For brevity here we call the
     * CPU path above; implement MDMA in your project if needed for peak
     * throughput. */
    for (uint32_t i = 0; i < n; ++i) {
        uint32_t wa = srcA[i];
        uint32_t wb = srcB[i];
        uint32_t wc = srcC[i];
        int16_t a0 = (int16_t)(wa >> 16);
        int16_t a1 = (int16_t)(wa & 0xFFFF);
        int16_t b0 = (int16_t)(wb >> 16);
        int16_t b1 = (int16_t)(wb & 0xFFFF);
        int16_t c0 = (int16_t)(wc >> 16);
        int16_t c1 = (int16_t)(wc & 0xFFFF);
        dst[2 * i + 0] = (ADS8555_Sample3){a0, b0, c0};
        dst[2 * i + 1] = (ADS8555_Sample3){a1, b1, c1};
    }
    if (dev->on_ready)
        dev->on_ready(dev, half);
}

void ADS8555_OnSpiHalfCplt(ADS8555_Handle* dev, SPI_HandleTypeDef* hspi) {
    uint8_t b = 1u << half_index(dev, hspi);
    dev->half_ready[0] |= b;
    try_kick_mdma(dev, 0);
}

void ADS8555_OnSpiCplt(ADS8555_Handle* dev, SPI_HandleTypeDef* hspi) {
    uint8_t b = 1u << half_index(dev, hspi);
    dev->half_ready[1] |= b;
    try_kick_mdma(dev, 1);
}
