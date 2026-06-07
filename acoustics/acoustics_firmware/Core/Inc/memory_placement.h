/**
 * @file memory_placement.h
 * @brief Section placement macros for STM32H753 memory regions.
 *
 * Usage:
 *   PLACE_IN_AXI_SRAM  uint8_t myBuffer[1024];   // D1: 512K AXI SRAM
 *   PLACE_IN_D2_SRAM   uint8_t dmaBuf[256];       // D2: 288K, DMA-accessible
 *   PLACE_IN_D3_SRAM   uint8_t bdmaBuf[64];       // D3: 64K SRAM4, BDMA-accessible
 *   PLACE_IN_DTCM      uint32_t fastVar;           // DTCM: 128K, CPU fast path
 *   PLACE_IN_ITCM      void fastFunc(void);        // ITCM: 64K, CPU instruction cache
 */

#ifndef MEMORY_PLACEMENT_H
#define MEMORY_PLACEMENT_H

#ifdef __GNUC__
/** D1 domain — 512K AXI SRAM (0x24000000)
 *  Accessible by: CPU, DMA1, DMA2, ETH, USB */
#define PLACE_IN_AXI_SRAM  __attribute__((section(".axi_sram")))

/** D2 domain — 288K SRAM1+2+3 (0x30000000)
 *  Accessible by: CPU, DMA1, DMA2, ETH, USB */
#define PLACE_IN_D2_SRAM   __attribute__((section(".d2_sram")))

/** D3 domain — 64K SRAM4 (0x38000000)
 *  Accessible by: CPU, BDMA, LPUART1 */
#define PLACE_IN_D3_SRAM   __attribute__((section(".d3_sram")))

/** DTCM — 128K Data Tightly Coupled Memory (0x20000000)
 *  * Accessible by: CPU, MDMA */
#define PLACE_IN_DTCM      __attribute__((section(".dtcm")))

/** ITCM — 64K Instruction Tightly Coupled Memory (0x00000000)
 * Accessible by: CPU, MDMA */
#define PLACE_IN_ITCM      __attribute__((section(".itcm")))

/* ================================================================
 * Alignment constants
 * ================================================================ */

/* --- Byte width transfers --- */
#define ALIGN_DMA_BYTE              1   /* Single byte, no burst     */
#define ALIGN_DMA_BURST_4_BYTE      4   /* 1 byte  × 4 beats         */
#define ALIGN_DMA_BURST_8_BYTE      8   /* 1 byte  × 8 beats         */
#define ALIGN_DMA_BURST_16_BYTE    16   /* 1 byte  × 16 beats        */

/* --- Half-word (16-bit) transfers --- */
#define ALIGN_DMA_HWORD             2   /* Single half-word           */
#define ALIGN_DMA_BURST_4_HWORD     8   /* 2 bytes × 4 beats         */
#define ALIGN_DMA_BURST_8_HWORD    16   /* 2 bytes × 8 beats         */
#define ALIGN_DMA_BURST_16_HWORD   32   /* 2 bytes × 16 beats        */

/* --- Word (32-bit) transfers --- */
#define ALIGN_DMA_WORD              4   /* Single word                */
#define ALIGN_DMA_BURST_4_WORD     16   /* 4 bytes × 4 beats         */
#define ALIGN_DMA_BURST_8_WORD     32   /* 4 bytes × 8 beats         */
#define ALIGN_DMA_BURST_16_WORD    64   /* 4 bytes × 16 beats        */

/* --- Double-word (64-bit) transfers (MDMA only) --- */
#define ALIGN_DMA_DWORD             8   /* Single double-word         */
#define ALIGN_DMA_BURST_4_DWORD    32   /* 8 bytes × 4 beats         */
#define ALIGN_DMA_BURST_8_DWORD    64   /* 8 bytes × 8 beats         */
#define ALIGN_DMA_BURST_16_DWORD  128   /* 8 bytes × 16 beats        */

/* --- Cache line --- */
#define ALIGN_CACHE_LINE           32   /* Cortex-M7 cache line size  */

/* ================================================================
 * DMA buffer placement + alignment macros
 * format: <DMA_CONTROLLER>_BUF_<REGION>(alignment)
 * ================================================================ */

/* BDMA — only D3 SRAM accessible */
#define BDMA_BUF_D3(burst_align)    __attribute__((section(".d3_sram"),  aligned(burst_align)))

/* DMA1 / DMA2 — AXI SRAM or D2 SRAM */
#define DMA_BUF_AXI(burst_align)    __attribute__((section(".axi_sram"),     aligned(burst_align)))
#define DMA_BUF_D2(burst_align)     __attribute__((section(".d2_sram"),  aligned(burst_align)))

/* MDMA — can reach DTCM, AXI, D2, D3 */
#define MDMA_BUF_DTCM(burst_align)  __attribute__((section(".dtcm"),     aligned(burst_align)))
#define MDMA_BUF_AXI(burst_align)   __attribute__((section(".axi_sram"),     aligned(burst_align)))
#define MDMA_BUF_D2(burst_align)    __attribute__((section(".d2_sram"),  aligned(burst_align)))
#define MDMA_BUF_D3(burst_align)    __attribute__((section(".d3_sram"),  aligned(burst_align)))
#else
#define PLACE_IN_AXI_SRAM
#define PLACE_IN_D2_SRAM 
#define PLACE_IN_D3_SRAM
#define PLACE_IN_DTCM
#define PLACE_IN_ITCM
#define ALIGN_DMA_BURST_8_WORD
#define ALIGN_DMA_BURST_4_WORD 
#define ALIGN_DMA_BURST_8_HWORD 
#define ALIGN_DMA_BURST_4_HWORD  
#define MDMA_BUF_DTCM(burst_align)
#define DMA_BUF_D2(burst_align)
#define DMA_BUF_AXI(burst_align)
#define DMA_BUF_D3(burst_align)
#endif

#endif /* MEMORY_PLACEMENT_H */
