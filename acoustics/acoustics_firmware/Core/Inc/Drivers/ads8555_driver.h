#ifndef ADS8555_DRIVER_H
#define ADS8555_DRIVER_H

#include <stdbool.h>
#include <stdint.h>
#include "stm32h7xx_hal.h"

#ifdef __cplusplus
extern "C" {
#endif  //  __cplusplus

#ifndef ADS8555_FS_GPIO_Port
#define ADS8555_FS_GPIO_Port GPIOE
#endif
#ifndef ADS8555_FS_Pin
#define ADS8555_FS_Pin GPIO_PIN_4
#endif

/* Optional conversion start (CONVST) and BUSY pins */
/* #define ADS8555_CONV_GPIO_Port GPIOE */
/* #define ADS8555_CONV_Pin GPIO_PIN_3 */
/* #define ADS8555_BUSY_GPIO_Port GPIOE */
/* #define ADS8555_BUSY_Pin GPIO_PIN_2 */

#ifndef ADS8555_USE_32BIT_FRAMES
#define ADS8555_USE_32BIT_FRAMES 1
#endif

#ifndef ADS8555_RX_BUF_WORDS
#define ADS8555_RX_BUF_WORDS 4096u
#endif

__attribute__((aligned(32))) uint32_t rx_buf_spi_1[ADS8555_RX_BUF_WORDS];
__attribute__((aligned(32))) uint32_t rx_buf_spi_2[ADS8555_RX_BUF_WORDS];
__attribute__((aligned(32))) uint32_t rx_buf_spi_3[ADS8555_RX_BUF_WORDS];

struct ads8555_ctrl {
    uint8_t range_a; /* per‑pair range/gain */
    uint8_t range_b;
    uint8_t range_c;
    uint8_t sdo_mode; /* 0:1‑SDO, 1:2‑SDO, 2:3‑SDO (we use 2 here) */
    uint8_t pwr_down; /* power‑down bits if used */
    uint8_t other;    /* remaining ctrl bits */
};

struct ADS8555_HandleTag;

typedef void (*ADS8555_OutputReadyCB)(struct ADS8555_HandleTag* dev,
                                      uint8_t half);

typedef struct ADS8555_HandleTag {
    SPI_HandleTypeDef* hspi_master; /* SPI4: master, half‑duplex TX‑only */
    SPI_HandleTypeDef* hspi_sdoA;   /* SPI1: slave full‑duplex (RX DMA used) */
    SPI_HandleTypeDef* hspi_sdoB;   /* SPI2 */
    SPI_HandleTypeDef* hspi_sdoC;   /* SPI3 */

    /* Optional MDMA handle (memory‑to‑memory interleave) */
    MDMA_HandleTypeDef* hmdma_a; /* A→out */
    MDMA_HandleTypeDef* hmdma_b; /* B→out */
    MDMA_HandleTypeDef* hmdma_c; /* C→out */

    /* Control word */
    struct ads8555_ctrl ctrl;
    uint16_t ctrl_word; /* packed 16‑bit control */

    /* RX buffers (AXI SRAM, 32‑byte aligned) */
    volatile uint8_t half_ready[2]; /* bit0=A, bit1=B, bit2=C */
    ADS8555_OutputReadyCB on_ready; /* user callback when out[half] is filled */
} ADS8555_Handle;

/* -------------------- API -------------------- */

/* Initialize: set handles, compute ctrl word, set idle pin states. */
HAL_StatusTypeDef ADS8555_Init(
    ADS8555_Handle* dev,
    SPI_HandleTypeDef* spi_master,
    SPI_HandleTypeDef* spi_sdoA,
    SPI_HandleTypeDef* spi_sdoB,
    SPI_HandleTypeDef* spi_sdoC,
    MDMA_HandleTypeDef* mdma_a, /* can be NULL to skip MDMA and let CPU pack */
    MDMA_HandleTypeDef* mdma_b,
    MDMA_HandleTypeDef* mdma_c,
    const struct ads8555_ctrl* ctrl_init,
    ADS8555_OutputReadyCB cb);

void ADS8555_OnSpiCplt(ADS8555_Handle* dev, SPI_HandleTypeDef* hspi);

#ifdef __cplusplus
}
#endif  //  __cplusplus

#endif  // !ADS_SAMPLING_H
