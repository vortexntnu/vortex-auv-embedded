#include "spi_link_test.h"
#include "bms_spi.h"
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

static void PRINT_BUF(const char *tag, const uint8_t *b, uint8_t n)
{
    printf("%s:", tag);
    for (uint8_t i = 0; i < n; i++)
        printf(" %02X", b[i]);
    printf("\r\n");
}

void SPI_LINK_TEST_RUN_ONCE(void)
{
    const uint8_t BASE = 0x10;
    uint8_t tx[4] = { 0xA5, 0x5A, 0xC3, 0x3C };
    uint8_t rx[4] = { 0 };

    printf("\r\n=== SPI LINK TEST (SAMC21 master) ===\r\n");

    /* This uses YOUR actual write_reg implementation */
    if (!write_reg(BASE, tx, (uint8_t)sizeof(tx)))
    {
        printf("write_reg FAILED\r\n");
        return;
    }

    /* This uses YOUR actual read_reg implementation (pipelined read + CRC check) */
    if (!read_reg(BASE, rx, (uint8_t)sizeof(rx)))
    {
        printf("read_reg FAILED\r\n");
        return;
    }

    PRINT_BUF("TX", tx, (uint8_t)sizeof(tx));
    PRINT_BUF("RX", rx, (uint8_t)sizeof(rx));

    bool ok = true;
    for (uint8_t i = 0; i < (uint8_t)sizeof(tx); i++)
        if (tx[i] != rx[i]) ok = false;

    printf("RESULT: %s\r\n", ok ? "OK" : "FAIL");
}
