#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include "definitions.h"
#include "spi_test.h"

#define SPI_TEST_CS_GROUP      (0U)
#define SPI_TEST_CS_MASK       (1UL << 7)   // PA07
#define SPI_TEST_WAIT_TIMEOUT  (2000000UL)

static inline void spi_test_cs_low(void)
{
    PORT_REGS->GROUP[SPI_TEST_CS_GROUP].PORT_OUTCLR = SPI_TEST_CS_MASK;
}

static inline void spi_test_cs_high(void)
{
    PORT_REGS->GROUP[SPI_TEST_CS_GROUP].PORT_OUTSET = SPI_TEST_CS_MASK;
}

static bool spi_test_wait_idle(void)
{
    uint32_t timeout = SPI_TEST_WAIT_TIMEOUT;

    while (SERCOM0_SPI_IsBusy())
    {
        if (timeout-- == 0U)
        {
            return false;
        }
    }

    return true;
}

static void spi_test_print_bytes(const char *label, const uint8_t *buf, size_t len)
{
    printf("%s:", label);
    for (size_t i = 0; i < len; i++)
    {
        printf(" %02X", buf[i]);
    }
    printf("\n");
}

bool spi_test_loopback(void)
{
    uint8_t tx[] = {0xA5U, 0x5AU, 0x3CU, 0xC3U, 0x00U, 0xFFU};
    uint8_t rx[sizeof(tx)];
    bool ok = false;

    PORT_REGS->GROUP[SPI_TEST_CS_GROUP].PORT_DIRSET = SPI_TEST_CS_MASK;
    spi_test_cs_high();

    if (SERCOM0_SPI_IsBusy())
    {
        return false;
    }

    memset(rx, 0, sizeof(rx));

    spi_test_cs_low();
    ok = SERCOM0_SPI_WriteRead(tx, sizeof(tx), rx, sizeof(rx));
    if (ok)
    {
        ok = spi_test_wait_idle();
    }
    spi_test_cs_high();

    if (!ok)
    {
        printf("SPI transfer failed\n");
        return false;
    }

    spi_test_print_bytes("SPI TX", tx, sizeof(tx));
    spi_test_print_bytes("SPI RX", rx, sizeof(rx));

    return (memcmp(tx, rx, sizeof(tx)) == 0);
}

void spi_test_run_and_print(void)
{
    bool pass = spi_test_loopback();
    printf("SPI loopback (PA08->PA09 jumper): %s\n", pass ? "PASS" : "FAIL");
}