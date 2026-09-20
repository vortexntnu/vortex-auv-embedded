/*
 Platform:
    ATSAMC21 

 Company:
    Vortex NTNU.

 Author:
    Markus Sandvik

 File Name:
    ws2812_port_sercom0_harmony.h
 */

#ifndef WS2812_PORT_SERCOM0_HARMONY_H
#define WS2812_PORT_SERCOM0_HARMONY_H

#ifdef __cplusplus
extern "C" {
#endif
    
#define WS2812_RESET_PAD_BYTES 80

/* Optional overrides:
   #define WS2812_DMAC_CH                  DMAC_CHANNEL_0
   #define WS2812_SERCOM_SPI_BYTES_MAX     4096u
   #define WS2812_RESET_PAD_BYTES          24u
*/
uint32_t ws_led_dbg_get_isr_count(void);
void ws_led_bind_port(void);   /* call once after SPI/DMAC init */

#ifdef __cplusplus
}
#endif
#endif
