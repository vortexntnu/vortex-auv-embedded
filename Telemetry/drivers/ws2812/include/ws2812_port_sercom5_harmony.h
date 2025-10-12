#ifndef WS2812_PORT_SERCOM5_HARMONY_H
#define WS2812_PORT_SERCOM5_HARMONY_H

#ifdef __cplusplus
extern "C" {
#endif

/* Bind the Harmony PLIB-backed async port (SERCOM5 SPI + TC3 timer)
 * to the ws2812 encoder. Call once after SYS_Initialize().
 */
void ws2812_port_bind_sercom5_harmony(void);

#ifdef __cplusplus
}
#endif

#endif /* WS2812_PORT_SERCOM5_HARMONY_H */