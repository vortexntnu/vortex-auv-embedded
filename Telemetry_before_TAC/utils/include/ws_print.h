#pragma once
/*
 * ws_print.h — tiny UART/printf helpers for inspecting WS2812 encoded data.
 * Usage:
 *   #include "ws_print.h"
 *   ws_dump_encoded(encbuf, led_count);        // prints hex + 3-bit groups
 *
 * If you want to disable all prints globally, #define WS_PRINT_ENABLE 0 before including.
 */

#ifndef WS_PRINT_ENABLE
#define WS_PRINT_ENABLE 1
#endif

#if WS_PRINT_ENABLE

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

/* Print one byte as hex (e.g., "DB") */
static inline void ws_print_byte_hex(uint8_t b)
{
    printf("%02X", b);
}

/* Print one byte as bits, grouping every 3 (e.g., "110 110 11") */
static inline void ws_print_bits8_group3(uint8_t b)
{
    for (int i = 7; i >= 0; --i) {
        putchar((b & (1u << i)) ? '1' : '0');
        if (i % 3 == 0 && i != 0) putchar(' ');
    }
}

/* Dump 9 encoded bytes (G,R,B) for a single LED: hex then bits */
static inline void ws_dump_encoded_led(const uint8_t enc9[9])
{
    // HEX
    printf("HEX:  ");
    for (int i = 0; i < 9; ++i) {
        ws_print_byte_hex(enc9[i]);
        if (i == 2 || i == 5) putchar('|'); else putchar(' ');
    }
    printf("\r\n");

    // BITS (groups of 3 should look like 100/110)
    printf("BITS: ");
    for (int i = 0; i < 9; ++i) {
        ws_print_bits8_group3(enc9[i]);
        if (i == 2 || i == 5) printf(" |"); else putchar(' ');
    }
    printf("\r\n");
}

/* Dump an entire encoded buffer (num_leds * 9 bytes) */
static inline void ws_dump_encoded(const uint8_t *buf, size_t num_leds)
{
    for (size_t i = 0; i < num_leds; ++i) {
        printf("LED[%u]\r\n", (unsigned)i);
        ws_dump_encoded_led(&buf[i * 9u]);
    }
}

/* Generic helpers (if you want quick hex/raw dumps) */
static inline void ws_dump_hex(const uint8_t *buf, size_t len)
{
    for (size_t i = 0; i < len; ++i) {
        ws_print_byte_hex(buf[i]);
        putchar((i + 1) % 16 == 0 ? '\n' : ' ');
    }
    printf("\r\n");
}

static inline void ws_dump_bits_group3(const uint8_t *buf, size_t len)
{
    for (size_t i = 0; i < len; ++i) {
        ws_print_bits8_group3(buf[i]);
        putchar(' ');
        if ((i + 1) % 6 == 0) printf("\r\n");
    }
    printf("\r\n");
}

#else  /* WS_PRINT_ENABLE == 0 */

/* No-op stubs when printing is disabled */
static inline void ws_dump_encoded_led(const uint8_t enc9[9])                { (void)enc9; }
static inline void ws_dump_encoded(const uint8_t *buf, size_t num_leds)      { (void)buf; (void)num_leds; }
static inline void ws_dump_hex(const uint8_t *buf, size_t len)               { (void)buf; (void)len; }
static inline void ws_dump_bits_group3(const uint8_t *buf, size_t len)       { (void)buf; (void)len; }

#endif /* WS_PRINT_ENABLE */
