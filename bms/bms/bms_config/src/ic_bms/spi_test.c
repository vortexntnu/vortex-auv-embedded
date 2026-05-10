#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include "definitions.h"
#include "spi_test.h"

#define SPI_TEST_CS_GROUP      (0U)
#define SPI_TEST_CS_MASK       (1UL << 7)   // PA07
#define SPI_TEST_WAIT_TIMEOUT  (2000000UL)

// Drives the same CS GPIO used by the production BMS SPI path.
static inline void spi_test_cs_low(void)
{
    PORT_REGS->GROUP[SPI_TEST_CS_GROUP].PORT_OUTCLR = SPI_TEST_CS_MASK;
}

static inline void spi_test_cs_high(void)
{
    PORT_REGS->GROUP[SPI_TEST_CS_GROUP].PORT_OUTSET = SPI_TEST_CS_MASK;
}

// Prevents tests from hanging forever if SPI stays busy.
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

// UART helper so we can inspect exact bytes sent/received on the live target.
static void spi_test_print_bytes(const char *label, const uint8_t *buf, size_t len)
{
    printf("%s:", label);
    for (size_t i = 0; i < len; i++)
    {
        printf(" %02X", buf[i]);
    }
    printf("\n");
}

// Same CRC-8 polynomial (0x07) used in bms_spi.c framing.
static uint8_t spi_test_crc8_calc(const uint8_t *data, uint8_t len)
{
    uint8_t crc = 0x00U;

    for (uint8_t i = 0; i < len; i++)
    {
        crc ^= data[i];
        for (uint8_t b = 0; b < 8U; b++)
        {
            crc = (crc & 0x80U) ? (uint8_t)((crc << 1U) ^ 0x07U) : (uint8_t)(crc << 1U);
        }
    }
    return crc;
}

// Tests write_reg frame construction and one real SPI write transfer.
// With loopback only, this validates transport/framing (not real BQ register writes).
static bool spi_test_write_reg_exact(uint8_t reg_addr, const uint8_t *data, uint8_t length)
{
    uint8_t tx_bytes[3U * 32U];

    if ((length == 0U) || (length > 32U) || SERCOM0_SPI_IsBusy())
    {
        return false;
    }

    for (uint8_t i = 0U; i < length; i++)
    {
        uint8_t cmd = (uint8_t)(0x80U | (uint8_t)((reg_addr + i) & 0x7FU));
        uint8_t pair[2] = {cmd, data[i]};
        tx_bytes[3U * i + 0U] = cmd;
        tx_bytes[3U * i + 1U] = data[i];
        tx_bytes[3U * i + 2U] = spi_test_crc8_calc(pair, 2U);
    }

    spi_test_print_bytes("write_reg tx", tx_bytes, (size_t)(3U * length));

    spi_test_cs_low();
    bool ok = SERCOM0_SPI_Write(tx_bytes, (size_t)(3U * length));
    if (ok)
    {
        ok = spi_test_wait_idle();
    }
    spi_test_cs_high();
    return ok;
}

// Tests read_reg frame construction, SPI full-duplex transfer, and CRC parsing.
// In loopback mode, rx mirrors tx; this still validates parser/CRC path end-to-end.
static bool spi_test_read_reg_exact(uint8_t reg_addr, uint8_t *data, uint8_t length)
{
    uint8_t tx[3U * 33U];
    uint8_t rx[3U * 33U];
    uint8_t frames;

    if ((length == 0U) || (length > 32U) || SERCOM0_SPI_IsBusy())
    {
        return false;
    }

    frames = (uint8_t)(length + 1U);

    {
        uint8_t pair0[2] = {(uint8_t)(reg_addr & 0x7FU), 0x00U};
        tx[0] = pair0[0];
        tx[1] = pair0[1];
        tx[2] = spi_test_crc8_calc(pair0, 2U);
    }

    for (uint8_t i = 1U; i < frames; i++)
    {
        uint8_t cmd = (i < (uint8_t)(frames - 1U)) ? (uint8_t)((reg_addr + i) & 0x7FU) : (uint8_t)0x00U;
        uint8_t pair[2] = {cmd, 0x00U};
        tx[3U * i + 0U] = pair[0];
        tx[3U * i + 1U] = pair[1];
        tx[3U * i + 2U] = spi_test_crc8_calc(pair, 2U);
    }

    spi_test_print_bytes("read_reg tx", tx, (size_t)(3U * frames));

    spi_test_cs_low();
    bool ok = SERCOM0_SPI_WriteRead(tx, (size_t)(3U * frames), rx, (size_t)(3U * frames));
    if (ok)
    {
        ok = spi_test_wait_idle();
    }
    spi_test_cs_high();
    if (!ok)
    {
        return false;
    }

    spi_test_print_bytes("read_reg rx", rx, (size_t)(3U * frames));

    for (uint8_t j = 1U; j <= length; j++)
    {
        uint8_t *chunk = &rx[3U * j];
        if (spi_test_crc8_calc(chunk, 2U) != chunk[2])
        {
            return false;
        }
        data[j - 1U] = chunk[1];
    }
    return true;
}

void spi_driver_self_test_run(void)
{
    uint8_t wr_data[3] = {0x12U, 0x34U, 0x56U};
    uint8_t rd_data[3] = {0U};
    bool ok_write;
    bool ok_read;

    PORT_REGS->GROUP[SPI_TEST_CS_GROUP].PORT_DIRSET = SPI_TEST_CS_MASK;
    spi_test_cs_high();

    printf("=== SPI read/write reg transport test (PA08->PA09 loopback) ===\n");

    ok_write = spi_test_write_reg_exact(0x10U, wr_data, sizeof(wr_data));
    ok_read = spi_test_read_reg_exact(0x10U, rd_data, sizeof(rd_data));

    printf("write_reg_exact(0x10, len=3): %s\n", ok_write ? "OK" : "FAIL");
    printf("read_reg_exact(0x10, len=3): %s\n", ok_read ? "OK" : "FAIL");
    spi_test_print_bytes("read_reg data", rd_data, sizeof(rd_data));
    printf("Note: this confirms MCU SPI framing/path only; real BQ register behavior needs the BQ IC connected.\n");
}
