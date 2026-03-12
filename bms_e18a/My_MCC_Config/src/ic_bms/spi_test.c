#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include "definitions.h"
#include "spi_test.h"

#define SPI_TEST_WAIT_TIMEOUT  (2000000UL)
#define UART_TEST_WAIT_TIMEOUT (3000000UL)

/* Set to 1U to re-enable old SPI transport framing checks. */
#define SPI_TEST_ENABLE_SPI_TRANSPORT 0U

/* Drives the same CS GPIO used by the production BMS SPI path. */
static inline void spi_test_cs_low(void)
{
    CS_Clear();
}

static inline void spi_test_cs_high(void)
{
    CS_Set();
}

#if SPI_TEST_ENABLE_SPI_TRANSPORT
/* Prevents tests from hanging forever if SPI stays busy. */
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

/* UART helper so we can inspect exact bytes sent/received on the live target. */
static void spi_test_print_bytes(const char *label, const uint8_t *buf, size_t len)
{
    printf("%s:", label);
    for (size_t i = 0; i < len; i++)
    {
        printf(" %02X", buf[i]);
    }
    printf("\n");
}

/* Same CRC-8 polynomial (0x07) used in bms_spi.c framing. */
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

/* Tests write_reg frame construction and one real SPI write transfer. */
/* With loopback only, this validates transport/framing (not real BQ register writes). */
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

/* Tests read_reg frame construction, SPI full-duplex transfer, and CRC parsing. */
/* In loopback mode, rx mirrors tx; this still validates parser/CRC path end-to-end. */
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
#endif

static bool uart_test_write_blocking(const uint8_t *buf, size_t len)
{
    uint32_t timeout = UART_TEST_WAIT_TIMEOUT;

    if ((buf == NULL) || (len == 0U))
    {
        return false;
    }

    if (!SERCOM3_USART_Write((void *)buf, len))
    {
        return false;
    }

    while (SERCOM3_USART_WriteIsBusy())
    {
        if (timeout-- == 0U)
        {
            return false;
        }
    }

    timeout = UART_TEST_WAIT_TIMEOUT;
    while (!SERCOM3_USART_TransmitComplete())
    {
        if (timeout-- == 0U)
        {
            return false;
        }
    }

    return true;
}

static bool uart_test_write_cstr(const char *text)
{
    size_t len = 0U;

    if (text == NULL)
    {
        return false;
    }

    while (text[len] != '\0')
    {
        len++;
    }

    return uart_test_write_blocking((const uint8_t *)text, len);
}

static bool uart_test_read_byte_timeout(uint8_t *value, uint32_t timeout)
{
    if (value == NULL)
    {
        return false;
    }

    if (!SERCOM3_USART_Read(value, 1U))
    {
        return false;
    }

    while (SERCOM3_USART_ReadIsBusy())
    {
        if (timeout-- == 0U)
        {
            SERCOM3_USART_ReadAbort();
            return false;
        }
    }

    return (SERCOM3_USART_ErrorGet() == USART_ERROR_NONE);
}

static void uart_test_format_hex_u8(uint8_t value, char *out)
{
    static const char hex[16] = "0123456789ABCDEF";

    out[0] = hex[(value >> 4) & 0x0FU];
    out[1] = hex[value & 0x0FU];
}

void spi_driver_self_test_run(void)
{
#if SPI_TEST_ENABLE_SPI_TRANSPORT
    uint8_t wr_data[3] = {0x12U, 0x34U, 0x56U};
    uint8_t rd_data[3] = {0U};
    bool ok_write;
    bool ok_read;
#endif
    bool ok_tx;
    bool ok_rx;
    uint8_t rx_byte = 0U;
    char rx_hex_msg[] = "RX byte: 0x00\r\n";
    char echo_hex_msg[] = "Echo byte: 0x00\r\n";

    CS_OutputEnable();
    spi_test_cs_high();

    ok_tx = uart_test_write_cstr("\r\n=== UART test on PA22(TX) / PA23(RX) ===\r\n");
    if (!ok_tx)
    {
        LED_Y_Clear();
        return;
    }

    ok_tx = uart_test_write_cstr("Send 1 byte from terminal now...\r\n");
    if (!ok_tx)
    {
        LED_Y_Clear();
        return;
    }

    ok_rx = uart_test_read_byte_timeout(&rx_byte, UART_TEST_WAIT_TIMEOUT);
    if (!ok_rx)
    {
        (void)uart_test_write_cstr("RX timeout or UART error\r\n");
        LED_Y_Clear();
        return;
    }

    uart_test_format_hex_u8(rx_byte, &rx_hex_msg[11]);
    (void)uart_test_write_cstr(rx_hex_msg);

    (void)uart_test_write_cstr("Echo: ");
    (void)uart_test_write_blocking(&rx_byte, 1U);
    (void)uart_test_write_cstr("\r\n");

    uart_test_format_hex_u8(rx_byte, &echo_hex_msg[13]);
    (void)uart_test_write_cstr(echo_hex_msg);

    LED_Y_Set();

    /*
     * SPI framing/transport self-test intentionally disabled for UART bring-up.
     * Re-enable by setting SPI_TEST_ENABLE_SPI_TRANSPORT to 1U.
     */
#if SPI_TEST_ENABLE_SPI_TRANSPORT

    ok_write = spi_test_write_reg_exact(0x10U, wr_data, sizeof(wr_data));
    ok_read = spi_test_read_reg_exact(0x10U, rd_data, sizeof(rd_data));

    printf("write_reg_exact(0x10, len=3): %s\n", ok_write ? "OK" : "FAIL");
    printf("read_reg_exact(0x10, len=3): %s\n", ok_read ? "OK" : "FAIL");
    spi_test_print_bytes("read_reg data", rd_data, sizeof(rd_data));
    printf("Note: with MISO/MOSI loopback this validates framing only; with BQ attached it validates real bus transactions.\n");
#endif
}
