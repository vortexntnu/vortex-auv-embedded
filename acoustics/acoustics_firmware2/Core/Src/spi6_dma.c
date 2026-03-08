/**
 * spi6_dma.c
 *
 * SPI6 16-bit full-duplex DMA driver for STM32H753
 *
 * Pin mapping:
 *   SPI6_SCK   PG13  AF5
 *   SPI6_MOSI  PG14  AF5
 *   SPI6_MISO  PA6   AF8
 *   CS         PE9   GPIO output (software controlled)
 *
 * SPI Mode: CPOL=1 (idle high), CPHA=0 (sample on first/rising edge) = Mode 2
 *
 * Buffers placed in SRAM4 (0x38000000) — required for BDMA which
 * only has access to the D3 power domain SRAM.
 *
 * Add to your linker script (.ld) MEMORY block if not already present:
 *   RAM_D3 (xrw) : ORIGIN = 0x38000000, LENGTH = 64K
 *
 * And add this output section:
 *   .sram4 (NOLOAD) :
 *   {
 *     *(.sram4)
 *     *(.sram4*)
 *   } >RAM_D3
 *
 * DMAMUX2 request IDs (H753 RM0433 Table 115):
 *   SPI6_RX = 10
 *   SPI6_TX = 11
 */

#include "stm32h7xx.h"

/* ── Buffers ── must live in SRAM4 for BDMA ─────────────────────────────── */

__attribute__((section(".sram4")))
static volatile uint16_t spi6_tx_buf;

__attribute__((section(".sram4")))
static volatile uint16_t spi6_rx_buf;

/* ── Forward declaration of user callback ────────────────────────────────── */

extern void SPI6_TransferCpltCallback(uint16_t rx_data);

/* ── CS helpers ──────────────────────────────────────────────────────────── */

static inline void CS_Low(void)
{
    GPIOE->BSRR = (uint32_t)GPIO_PIN_9 << 16U;  /* Reset PE9 */
}

static inline void CS_High(void)
{
    GPIOE->BSRR = GPIO_PIN_9;                    /* Set PE9 */
}

/* ── Initialization ──────────────────────────────────────────────────────── */

void SPI6_DMA_Init(void)
{
    /* 1. Enable clocks ---------------------------------------------------- */
	RCC->APB4ENR |= RCC_APB4ENR_SPI6EN;
    RCC->AHB4ENR  |= RCC_AHB4ENR_BDMAEN;
    RCC->AHB4ENR  |= RCC_AHB4ENR_GPIOAEN
                   | RCC_AHB4ENR_GPIOEEN
                   | RCC_AHB4ENR_GPIOGEN;

    /* Small delay for clock to stabilize */
    __DSB();
    __ISB();

    /* 2. GPIO ------------------------------------------------------------ */

    /* PA6 — SPI6_MISO, AF8 */
    GPIOA->MODER   &= ~(0x3U << (6 * 2));
    GPIOA->MODER   |=  (0x2U << (6 * 2));        /* Alternate function */
    GPIOA->OSPEEDR |=  (0x3U << (6 * 2));        /* Very high speed */
    GPIOA->PUPDR   &= ~(0x3U << (6 * 2));        /* No pull */
    GPIOA->AFR[0]  &= ~(0xFU << (6 * 4));
    GPIOA->AFR[0]  |=  (0x8U << (6 * 4));        /* AF8 */

    /* PG13 — SPI6_SCK, AF5 */
    GPIOG->MODER   &= ~(0x3U << (13 * 2));
    GPIOG->MODER   |=  (0x2U << (13 * 2));
    GPIOG->OSPEEDR |=  (0x3U << (13 * 2));
    GPIOG->PUPDR   &= ~(0x3U << (13 * 2));
    GPIOG->AFR[1]  &= ~(0xFU << ((13 - 8) * 4));
    GPIOG->AFR[1]  |=  (0x5U << ((13 - 8) * 4)); /* AF5 */

    /* PG14 — SPI6_MOSI, AF5 */
    GPIOG->MODER   &= ~(0x3U << (14 * 2));
    GPIOG->MODER   |=  (0x2U << (14 * 2));
    GPIOG->OSPEEDR |=  (0x3U << (14 * 2));
    GPIOG->PUPDR   &= ~(0x3U << (14 * 2));
    GPIOG->AFR[1]  &= ~(0xFU << ((14 - 8) * 4));
    GPIOG->AFR[1]  |=  (0x5U << ((14 - 8) * 4)); /* AF5 */

    /* PE9 — CS, plain output, idle HIGH */
    GPIOE->MODER   &= ~(0x3U << (9 * 2));
    GPIOE->MODER   |=  (0x1U << (9 * 2));        /* Output */
    GPIOE->OSPEEDR |=  (0x3U << (9 * 2));        /* Very high speed */
    GPIOE->PUPDR   &= ~(0x3U << (9 * 2));        /* No pull */
    GPIOE->OTYPER  &= ~(0x1U << 9);              /* Push-pull */
    CS_High();                                    /* Deassert at start */

    /* 3. Configure SPI6 (SPE must be 0) ---------------------------------- */

    /*
     * CFG1:
     *   MBR[2:0] = 0b101 → fPCLK/64   (adjust field bits 30:28 for your rate)
     *   DSIZE    = 0xF   → 16-bit frames
     *   TXDMAEN + RXDMAEN enabled
     */
    SPI6->CFG1 = (0x5U << 28)               /* MBR: /64 — tune as needed   */
               | (0xFU <<  0)               /* DSIZE = 16-bit               */
               | SPI_CFG1_TXDMAEN
               | SPI_CFG1_RXDMAEN;

    /*
     * CFG2:
     *   MASTER  = 1   → master mode
     *   CPOL    = 1   → clock idle HIGH
     *   CPHA    = 0   → sample on first edge (rising, since CPOL=1) = Mode 2
     *   SSM     = 1   → software slave management (we drive CS manually)
     *   COMM    = 00  → full-duplex
     *   LSBFRST = 0   → MSB first
     */
    SPI6->CFG2 = SPI_CFG2_MASTER
               | SPI_CFG2_CPOL               /* CPOL = 1                    */
               | SPI_CFG2_SSM;               /* Software NSS                */
               /* CPHA = 0 (first edge) — bit left clear                    */

    /*
     * CR1: SSI must be 1 when SSM=1, otherwise MODF fault fires.
     * SPE is set last, after BDMA is configured.
     */
    SPI6->CR1 = SPI_CR1_SSI;

    /* 4. BDMA ------------------------------------------------------------ */

    /* TX: BDMA_Channel0 ← DMAMUX2_Channel0 ← request 11 (SPI6_TX) */
    BDMA_Channel0->CCR   = 0;                /* Disable while configuring   */
    BDMA_Channel0->CPAR  = (uint32_t)&SPI6->TXDR;
    BDMA_Channel0->CM0AR  = (uint32_t)&spi6_tx_buf;
    BDMA_Channel0->CNDTR = 1;
    BDMA_Channel0->CCR   = BDMA_CCR_MSIZE_0  /* Memory size:     16-bit    */
                         | BDMA_CCR_PSIZE_0  /* Peripheral size: 16-bit    */
                         | BDMA_CCR_DIR      /* Direction: mem → periph    */
                         | BDMA_CCR_TCIE;    /* TC interrupt enable        */

    DMAMUX2_Channel0->CCR = 11U;             /* Bind to SPI6_TX request     */

    /* RX: BDMA_Channel1 ← DMAMUX2_Channel1 ← request 10 (SPI6_RX) */
    BDMA_Channel1->CCR   = 0;
    BDMA_Channel1->CPAR  = (uint32_t)&SPI6->RXDR;
    BDMA_Channel1->CM0AR  = (uint32_t)&spi6_rx_buf;
    BDMA_Channel1->CNDTR = 1;
    BDMA_Channel1->CCR   = BDMA_CCR_MSIZE_0  /* Memory size:     16-bit    */
                         | BDMA_CCR_PSIZE_0  /* Peripheral size: 16-bit    */
                         | 0U               /* Direction: periph → mem     */
                         | BDMA_CCR_TCIE;    /* TC interrupt enable        */

    DMAMUX2_Channel1->CCR = 10U;             /* Bind to SPI6_RX request     */

    /* 5. NVIC ------------------------------------------------------------ */
    NVIC_SetPriority(BDMA_Channel0_IRQn, 6U);
    NVIC_SetPriority(BDMA_Channel1_IRQn, 6U);
    NVIC_EnableIRQ(BDMA_Channel0_IRQn);
    NVIC_EnableIRQ(BDMA_Channel1_IRQn);

    /* 6. Enable SPI ------------------------------------------------------- */
    SPI6->CR1 |= SPI_CR1_SPE;
}

/* ── Start a 16-bit transfer ─────────────────────────────────────────────── */
/*
 * Safe to call from an ISR.
 * Do NOT call while a transfer is already in progress — check
 * SPI6_IsBusy() first if there is any risk of overlap.
 */
void SPI6_Transfer16(uint16_t tx_data)
{
    spi6_tx_buf = tx_data;

    /* Re-arm BDMA channels (CNDTR resets to 0 after each transfer) */
    BDMA_Channel0->CCR   &= ~BDMA_CCR_EN;
    BDMA_Channel1->CCR   &= ~BDMA_CCR_EN;
    BDMA_Channel0->CNDTR  = 1U;
    BDMA_Channel1->CNDTR  = 1U;
    BDMA_Channel0->CCR   |=  BDMA_CCR_EN;
    BDMA_Channel1->CCR   |=  BDMA_CCR_EN;

    CS_Low();

    /* Set frame count and kick off transfer */
    SPI6->CR2  = 1U;                         /* TSIZE = 1 frame             */
    SPI6->CR1 |= SPI_CR1_CSTART;            /* Start                       */
}

/* ── Optional: poll this before calling SPI6_Transfer16 ─────────────────── */

uint8_t SPI6_IsBusy(void)
{
    return (SPI6->SR & SPI_SR_TXC) == 0U;
}

/* ── BDMA IRQ handlers ───────────────────────────────────────────────────── */

/* TX complete — just clear the flag, nothing else needed */
void BDMA_Channel0_IRQHandler(void)
{
    BDMA->IFCR = BDMA_IFCR_CTCIF0;
}

/*
 * RX complete — fires after the full 16-bit word has been shifted in.
 * At this point both TX and RX are done (SPI is synchronous).
 * Deassert CS then invoke the user callback with the received word.
 */
void BDMA_Channel1_IRQHandler(void)
{
    if (BDMA->ISR & BDMA_ISR_TCIF1)
    {
        BDMA->IFCR = BDMA_IFCR_CTCIF1;

        CS_High();

        SPI6_TransferCpltCallback((uint16_t)spi6_rx_buf);
    }
}
