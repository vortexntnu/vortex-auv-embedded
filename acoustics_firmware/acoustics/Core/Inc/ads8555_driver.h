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

/* Use 32‑bit frames on SPI receivers (recommended) */
#ifndef ADS8555_USE_32BIT_FRAMES
#define ADS8555_USE_32BIT_FRAMES 1
#endif

/* Buffer sizing: per‑SPI circular RX buffer length in words (16‑bit if 16‑bit
 * frames, 32‑bit if 32‑bit frames). Must be even (ping‑pong). Choose big enough
 * for timing slack. */
#ifndef ADS8555_RX_BUF_WORDS
#define ADS8555_RX_BUF_WORDS 4096u
#endif

/* Output format: AoS (A0,B0,C0,A1,B1,C1,...) as 16‑bit samples. */
typedef struct {
    int16_t a, b, c;
} ADS8555_Sample3;

/* Per‑frame control fields (mapped to ADS8555 control register). Update mapping
 * as per datasheet. */
typedef struct {
    uint8_t range_a; /* per‑pair range/gain */
    uint8_t range_b;
    uint8_t range_c;
    uint8_t sdo_mode; /* 0:1‑SDO, 1:2‑SDO, 2:3‑SDO (we use 2 here) */
    uint8_t pwr_down; /* power‑down bits if used */
    uint8_t other;    /* remaining ctrl bits */
} ADS8555_CtrlFields;

/* Forward decl for user callback */
struct ADS8555_HandleTag;
typedef void (*ADS8555_OutputReadyCB)(struct ADS8555_HandleTag* dev,
                                      uint8_t half);

/* Driver context */
typedef struct ADS8555_HandleTag {
    /* HAL handles (provided by user) */
    SPI_HandleTypeDef* hspi_master; /* SPI4: master, half‑duplex TX‑only */
    SPI_HandleTypeDef* hspi_sdoA;   /* SPI1: slave full‑duplex (RX DMA used) */
    SPI_HandleTypeDef* hspi_sdoB;   /* SPI2 */
    SPI_HandleTypeDef* hspi_sdoC;   /* SPI3 */

    /* Optional MDMA handle (memory‑to‑memory interleave) */
    MDMA_HandleTypeDef* hmdma_a; /* A→out */
    MDMA_HandleTypeDef* hmdma_b; /* B→out */
    MDMA_HandleTypeDef* hmdma_c; /* C→out */

    /* Control word */
    ADS8555_CtrlFields ctrl;
    uint16_t ctrl_word; /* packed 16‑bit control */

    /* RX buffers (AXI SRAM, 32‑byte aligned) */
    __attribute__((aligned(32))) uint32_t rxA[ADS8555_RX_BUF_WORDS];
    __attribute__((aligned(32))) uint32_t rxB[ADS8555_RX_BUF_WORDS];
    __attribute__((aligned(32))) uint32_t rxC[ADS8555_RX_BUF_WORDS];

    /* Interleaved output buffer (two halves of N/2 samples) */
    __attribute__((aligned(32))) ADS8555_Sample3 out[2][ADS8555_RX_BUF_WORDS];

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
    const ADS8555_CtrlFields* ctrl_init,
    ADS8555_OutputReadyCB cb);


void ADS8555_OnSpiCplt(ADS8555_Handle* dev, SPI_HandleTypeDef* hspi);

#ifdef __cplusplus
}
#endif  //  __cplusplus

#endif  // !ADS_SAMPLING_H
