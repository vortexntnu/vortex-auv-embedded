#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "definitions.h"
#include "app/can_facade.h"
#include "spi_test.h"
#include "bms_spi.h"

#define UART_TIMEOUT_LOOPS        (3000000UL)
#define VOLTAGE_TEST_DELAY_CYCLES (24000000UL)
#define CAN_SCOPE_TEST_PERIOD_MS  (100U)

static void delay_cycles(uint32_t cycles)
{
    volatile uint32_t i;

    for (i = 0U; i < cycles; i++)
    {
        ;
    }
}

static bool uart_write_blocking(const uint8_t *data, size_t len)
{
    uint32_t timeout = UART_TIMEOUT_LOOPS;

    if ((data == NULL) || (len == 0U))
    {
        return false;
    }

    if (!SERCOM3_USART_Write((void *)data, len))
    {
        return false;
    }

    timeout = UART_TIMEOUT_LOOPS;
    while (!SERCOM3_USART_TransmitComplete())
    {
        if (timeout-- == 0U)
        {
            return false;
        }
    }

    return true;
}

static void uart_write_text(const char *text)
{
    size_t len;

    if (text == NULL)
    {
        return;
    }

    len = strlen(text);
    if (len > 0U)
    {
        (void)uart_write_blocking((const uint8_t *)text, len);
    }
}

static void uart_write_voltages(const uint16_t cell_mV[6])
{
    char line[96];
    int len = snprintf(
        line,
        sizeof(line),
        "%u,%u,%u,%u,%u,%u\r\n",
        (unsigned int)cell_mV[0],
        (unsigned int)cell_mV[1],
        (unsigned int)cell_mV[2],
        (unsigned int)cell_mV[3],
        (unsigned int)cell_mV[4],
        (unsigned int)cell_mV[5]
    );

    if (len <= 0)
    {
        return;
    }

    if ((size_t)len >= sizeof(line))
    {
        len = (int)(sizeof(line) - 1U);
    }

    (void)uart_write_blocking((const uint8_t *)line, (size_t)len);
}

static void uart_write_alert_ssa(void)
{
    uint16_t alarm = 0U;
    uint16_t ssa = 0U;
    char line[48];
    int len;

    if (!bq_direct_command(AlarmStatus, &alarm, R))
    {
        return;
    }

    if (!bq_direct_command(SafetyStatusA, &ssa, R))
    {
        return;
    }

    len = snprintf(
        line,
        sizeof(line),
        "alert=0x%04X,ssa=0x%04X\r\n",
        (unsigned int)alarm,
        (unsigned int)ssa);

    if (len <= 0)
    {
        return;
    }

    if ((size_t)len >= sizeof(line))
    {
        len = (int)(sizeof(line) - 1U);
    }

    (void)uart_write_blocking((const uint8_t *)line, (size_t)len);
}

void spi_write_probe_step(void)
{
    static uint8_t tx = 0x5AU;
    bool ok;
    char line[32];
    int len;

    ok = (write_reg(BATTERY_STATUS, &tx, 1U) == BQ_OK);

    len = snprintf(
        line,
        sizeof(line),
        "wr,0x%02X,%s\r\n",
        (unsigned int)tx,
        ok ? "ok" : "fail");

    if (len > 0)
    {
        if ((size_t)len >= sizeof(line))
        {
            len = (int)(sizeof(line) - 1U);
        }
        (void)uart_write_blocking((const uint8_t *)line, (size_t)len);
    }

    if (ok)
    {
        LED_R_Clear();
    }
    else
    {
        LED_R_Set();
    }

    LED_Y_Toggle();
    delay_cycles(VOLTAGE_TEST_DELAY_CYCLES);

    tx ^= 0xFFU;
}

void voltage_test_init(void)
{
    LED_R_Clear();
    LED_Y_Clear();

    uart_write_text("BMS voltage test started\r\n");
    uart_write_text("voltage_order,c1,c2,c3,c4,c5,c10\r\n");
}

void voltage_test_step(void)
{
    uint16_t cell_mV[6] = {0U};
    bool ok = read_cells_1to6(cell_mV);

    if (ok)
    {
        uart_write_voltages(cell_mV);
        uart_write_alert_ssa();
        LED_R_Clear();
    }
    else
    {
        uart_write_text("read fail\r\n");
        LED_R_Set();
    }

    LED_Y_Toggle();
    delay_cycles(VOLTAGE_TEST_DELAY_CYCLES);
}

void can_scope_test_init(void)
{
    LED_R_Clear();
    LED_Y_Clear();

    CAN_Init();
}

void can_scope_test_step(void)
{
    uint8_t rx_data[8];
    uint32_t rx_id = 0U;
    uint8_t rx_len = 0U;
    char line[64];
    int len;

    if (CAN_TryRead(&rx_id, &rx_len, rx_data))
    {
        len = snprintf(
            line,
            sizeof(line),
            "rx,0x%03X,%u,0x%02X,0x%02X\r\n",
            (unsigned int)rx_id,
            (unsigned int)rx_len,
            (unsigned int)((rx_len > 0U) ? rx_data[0] : 0U),
            (unsigned int)((rx_len > 1U) ? rx_data[1] : 0U));

        if (len > 0)
        {
            if ((size_t)len >= sizeof(line))
            {
                len = (int)(sizeof(line) - 1U);
            }
            (void)uart_write_blocking((const uint8_t *)line, (size_t)len);
        }
    }

    LED_Y_Toggle();
    SYSTICK_DelayMs(CAN_SCOPE_TEST_PERIOD_MS);
}
