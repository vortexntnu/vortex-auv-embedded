/*
******************************************************************************
**
**  File        : STM32H753ZITX_FLASH.ld
**
**  Author      : STM32CubeIDE
**
**  Abstract    : Linker script for STM32H753ZITx
**                2048K FLASH
**                  64K ITCM
**                 128K DTCM      (D1, 0x20000000)
**                 512K AXI SRAM  (D1, 0x24000000)
**                 288K SRAM1-3   (D2, 0x30000000)
**                  64K SRAM4     (D3, 0x38000000)
**
**  Distribution: The file is distributed as is, without any warranty
**                of any kind.
**
******************************************************************************
** @attention
**
** Copyright (c) 2025 STMicroelectronics.
** All rights reserved.
**
** This software is licensed under terms that can be found in the LICENSE file
** in the root directory of this software component.
** If no LICENSE file comes with this software, it is provided AS-IS.
**
******************************************************************************
*/

/* Entry Point */
ENTRY(Reset_Handler)

/* Highest address of the user mode stack */
_estack = ORIGIN(RAM_D1) + LENGTH(RAM_D1);

/* Generate a link error if heap and stack don't fit into RAM */
_Min_Heap_Size = 0x200;
_Min_Stack_Size = 0x400;  /* 16KB — gives plenty of headroom */

/* Specify the memory areas */
MEMORY
{
  FLASH    (rx)  : ORIGIN = 0x08000000, LENGTH = 2048K
  ITCMRAM  (xrw) : ORIGIN = 0x00000000, LENGTH = 64K
  DTCMRAM  (xrw) : ORIGIN = 0x20000000, LENGTH = 128K
  RAM_D1   (xrw) : ORIGIN = 0x24000000, LENGTH = 512K   /* AXI SRAM */
  RAM_D2   (xrw) : ORIGIN = 0x30000000, LENGTH = 288K   /* SRAM1+2+3 */
  RAM_D3   (xrw) : ORIGIN = 0x38000000, LENGTH = 64K    /* SRAM4 */
}

/* Define output sections */
SECTIONS
{
  /* Startup / interrupt vector table — must be first in FLASH */
  .isr_vector :
  {
    . = ALIGN(4);
    KEEP(*(.isr_vector))
    . = ALIGN(4);
  } >FLASH

  /* Code */
  .text :
  {
    . = ALIGN(4);
    *(.text)
    *(.text*)
    *(.glue_7)
    *(.glue_7t)
    *(.eh_frame)
    KEEP(*(.init))
    KEEP(*(.fini))
    . = ALIGN(4);
    _etext = .;
  } >FLASH

  /* Read-only data */
  .rodata :
  {
    . = ALIGN(4);
    *(.rodata)
    *(.rodata*)
    . = ALIGN(4);
  } >FLASH

  /* Exception handling — GCC11+ supports READONLY; remove if using GCC10 or earlier */
  .ARM.extab (READONLY) :
  {
    *(.ARM.extab* .gnu.linkonce.armextab.*)
  } >FLASH

  .ARM (READONLY) :
  {
    __exidx_start = .;
    *(.ARM.exidx*)
    __exidx_end = .;
  } >FLASH

  .preinit_array (READONLY) :
  {
    PROVIDE_HIDDEN(__preinit_array_start = .);
    KEEP(*(.preinit_array*))
    PROVIDE_HIDDEN(__preinit_array_end = .);
  } >FLASH

  .init_array (READONLY) :
  {
    PROVIDE_HIDDEN(__init_array_start = .);
    KEEP(*(SORT(.init_array.*)))
    KEEP(*(.init_array*))
    PROVIDE_HIDDEN(__init_array_end = .);
  } >FLASH

  .fini_array (READONLY) :
  {
    PROVIDE_HIDDEN(__fini_array_start = .);
    KEEP(*(SORT(.fini_array.*)))
    KEEP(*(.fini_array*))
    PROVIDE_HIDDEN(__fini_array_end = .);
  } >FLASH

  /* Initialised data: LMA in FLASH, VMA in D1 AXI SRAM */
  _sidata = LOADADDR(.data);

  .data :
  {
    . = ALIGN(4);
    _sdata = .;
    *(.data)
    *(.data*)
    *(.RamFunc)
    *(.RamFunc*)
    . = ALIGN(4);
    _edata = .;
  } >RAM_D1 AT> FLASH

  /* Zero-initialised data — D1 AXI SRAM */
  . = ALIGN(4);
  .bss :
  {
    _sbss = .;
    __bss_start__ = _sbss;
    *(.bss)
    *(.bss*)
    *(COMMON)
    . = ALIGN(4);
    _ebss = .;
    __bss_end__ = _ebss;
  } >RAM_D1

  /* Heap + stack size check */
  ._user_heap_stack :
  {
    . = ALIGN(8);
    PROVIDE(end = .);
    PROVIDE(_end = .);
    . = . + _Min_Heap_Size;
    . = . + _Min_Stack_Size;
    . = ALIGN(8);
  } >RAM_D1

  /* ------------------------------------------------------------------ */
  /* Explicit placement sections — use macros from memory_placement.h   */
  /* ------------------------------------------------------------------ */

  /* PLACE_IN_ITCM — 64K Instruction TCM, zero-wait instruction fetch */
  .itcm (NOLOAD) :
  {
    . = ALIGN(4);
    *(.itcm)
    . = ALIGN(4);
  } >ITCMRAM

  /* PLACE_IN_DTCM — 128K Data TCM, zero-wait CPU data access */
  .dtcm (NOLOAD) :
  {
    . = ALIGN(4);
    *(.dtcm)
    . = ALIGN(4);
  } >DTCMRAM

  /* PLACE_IN_D2_SRAM — 288K SRAM1+2+3, accessible by DMA1/DMA2 */
  .d2_sram (NOLOAD) :
  {
    . = ALIGN(4);
    *(.d2_sram)
    . = ALIGN(4);
  } >RAM_D2

  /* PLACE_IN_D3_SRAM — 64K SRAM4, accessible by BDMA */
  .d3_sram (NOLOAD) :
  {
    . = ALIGN(4);
    *(.d3_sram)
    . = ALIGN(4);
  } >RAM_D3

  /* PLACE_IN_AXI_SRAM — 512K AXI SRAM, general purpose D1 RAM
   * (default .data/.bss destination; only add here if you need
   *  explicit placement of specific objects) */
  .axi_sram (NOLOAD) :
  {
    . = ALIGN(4);
    *(.axi_sram)
    . = ALIGN(4);
  } >RAM_D1

  /* ------------------------------------------------------------------ */

  /DISCARD/ :
  {
    libc.a(*)
    libm.a(*)
    libgcc.a(*)
  }

  .ARM.attributes 0 : { *(.ARM.attributes) }
}
