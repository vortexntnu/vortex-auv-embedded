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


// Bind this SERCOM0/DMAC/TC3 port to the ws2812 encoder core.
// Call once during init (after SYS_Initialize / DMAC_Initialize / SERCOM0 init).
void ws_led_bind_port(void);


// Optional: override at compile time if not using MCC defaults
// -DWS2812_PORT_MAX_LEDS=64
// -DWS2812_DMAC_CH=DMAC_CHANNEL_1


#ifdef __cplusplus
}
#endif