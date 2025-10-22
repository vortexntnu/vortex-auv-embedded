#include "ads8555_driver.h"
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "cmsis_gcc.h"

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

static uint16_t pack_ctrl(const struct ads8555_ctrl* c) {
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
    if (hspi == dev->hspi_sdoA)
        return 0;
    if (hspi == dev->hspi_sdoB)
        return 1;
    return 2;
}

void ADS8555_SetControl(ADS8555_Handle* dev, const struct ads8555_ctrl* ctrl) {
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
                               const struct ads8555_ctrl* ctrl_init,
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

    struct ads8555_ctrl c = {0};
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
        dev->hspi_sdoA, (uint8_t*)rx_buf_spi_1, ADS8555_RX_BUF_WORDS);
    HAL_StatusTypeDef sb = HAL_SPI_Receive_DMA(
        dev->hspi_sdoB, (uint8_t*)rx_buf_spi_2, ADS8555_RX_BUF_WORDS);
    HAL_StatusTypeDef sc = HAL_SPI_Receive_DMA(
        dev->hspi_sdoC, (uint8_t*)rx_buf_spi_3, ADS8555_RX_BUF_WORDS);
#else
    /* 16‑bit frames */
    HAL_StatusTypeDef sa = HAL_SPI_Receive_DMA(
        dev->hspi_sdoA, (uint8_t*)rx_buf_spi_1, ADS8555_RX_BUF_WORDS * 2);
    HAL_StatusTypeDef sb = HAL_SPI_Receive_DMA(
        dev->hspi_sdoB, (uint8_t*)rx_buf_spi_2, ADS8555_RX_BUF_WORDS * 2);
    HAL_StatusTypeDef sc = HAL_SPI_Receive_DMA(
        dev->hspi_sdoC, (uint8_t*)rx_buf_spi_3, ADS8555_RX_BUF_WORDS * 2);
#endif
    if (sa || sb || sc)
        return HAL_ERROR;
    return HAL_OK;
}

void ADS8555_Stop(ADS8555_Handle* dev) {
    HAL_SPI_DMAStop(dev->hspi_sdoA);
    HAL_SPI_DMAStop(dev->hspi_sdoB);
    HAL_SPI_DMAStop(dev->hspi_sdoC);
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

static inline void deinterleave_cpu(const uint16_t* restrict src,
                                    uint16_t* restrict out1,
                                    uint16_t* restrict out2,
                                    size_t size) {
    for (size_t i = 0; i < size; i++) {
        *out1++ = *src++;
        *out2++ = *src++;
    }
}

static inline void deinterleave_3x2_to_5(const uint32_t* __restrict srcA,
                                         const uint32_t* __restrict srcB,
                                         const uint32_t* __restrict srcC,
                                         uint32_t n_words,
                                         int16_t* __restrict outA0,
                                         int16_t* __restrict outA1,
                                         int16_t* __restrict outB0,
                                         int16_t* __restrict outB1,
                                         int16_t* __restrict outC0) {
    uint32_t i = 0;

    for (; i + 1u < n_words; i += 2u) {
        uint32_t wa0 = *srcA++;
        uint32_t wb0 = *srcB++;
        uint32_t wc0 = *srcC++;

        uint32_t wa1 = *srcA++;
        uint32_t wb1 = *srcB++;
        uint32_t wc1 = *srcC++;

        // Iter 0
        *outA0++ = (int16_t)(wa0 >> 16);
        *outA1++ = (int16_t)(wa0);
        *outB0++ = (int16_t)(wb0 >> 16);
        *outB1++ = (int16_t)(wb0);
        *outC0++ = (int16_t)(wc0 >> 16);

        // Iter 1
        *outA0++ = (int16_t)(wa1 >> 16);
        *outA1++ = (int16_t)(wa1);
        *outB0++ = (int16_t)(wb1 >> 16);
        *outB1++ = (int16_t)(wb1);
        *outC0++ = (int16_t)(wc1 >> 16);
    }

    // Tail
    for (; i < n_words; ++i) {
        uint32_t wa = *srcA++;
        uint32_t wb = *srcB++;
        uint32_t wc = *srcC++;

        *outA0++ = (int16_t)(wa >> 16);
        *outA1++ = (int16_t)(wa);
        *outB0++ = (int16_t)(wb >> 16);
        *outB1++ = (int16_t)(wb);
        *outC0++ = (int16_t)(wc >> 16);
    }
}

void ADS8555_OnSpiHalfCplt(ADS8555_Handle* dev, SPI_HandleTypeDef* hspi) {
    uint8_t b = 1u << half_index(dev, hspi);
    dev->half_ready[0] |= b;
    // try_kick_mdma(dev, 0);
}

void ADS8555_OnSpiCplt(ADS8555_Handle* dev, SPI_HandleTypeDef* hspi) {
    uint8_t b = 1u << half_index(dev, hspi);
    dev->half_ready[1] |= b;
    // try_kick_mdma(dev, 1);
}
