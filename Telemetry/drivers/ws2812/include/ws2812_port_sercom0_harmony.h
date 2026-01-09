/*
 * Header File
 * 
 * Platform:
 *  ATSAMC21 
 * 
 * Company:
 *  Vortex NTNU.
 * 
 * Author:
 *  Markus Sandvik
 * 
 * File Name:
 *  ws2812_port_sercom0_harmony.h
 */

#pragma once
#ifdef __cplusplus
extern "C" {
#endif


#define WS2812_RESET_PAD_BYTES 80

uint32_t ws_led_dbg_get_isr_count(void);
void ws_led_bind_port(void);   /* call once after SPI/DMAC init */

#ifdef __cplusplus
}
#endif
