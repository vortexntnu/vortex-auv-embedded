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

static bool s_cb_baseline_valid = false;
static uint32_t s_cb_cell3_baseline_s = 0U;
static uint16_t s_cb_last_active = 0xFFFFU;
static uint16_t s_cb_last_present = 0xFFFFU;
static uint32_t s_cb_last_cell3 = 0xFFFFFFFFUL;
static bool s_cb_last_ok = false;
static bool s_cb_occurred_reported = false;

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

static void uart_write_voltages(const uint16_t cell_mV[10])
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
        (unsigned int)cell_mV[9]
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

static void uart_write_balance_status(
    bool ok,
    uint16_t active_mask,
    uint16_t present_s,
    uint32_t cell3_total_s,
    bool occurred,
    uint32_t delta_s)
{
    char line[128];
    int len;

    if (!ok)
    {
        uart_write_text("cb,read_fail\r\n");
        return;
    }

    len = snprintf(
        line,
        sizeof(line),
        "cb,active=0x%04X,present_s=%u,cell3_total_s=%lu,delta_s=%lu,occurred=%u\r\n",
        (unsigned int)active_mask,
        (unsigned int)present_s,
        (unsigned long)cell3_total_s,
        (unsigned long)delta_s,
        (unsigned int)(occurred ? 1U : 0U));

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
    char line[64];
    int len;

    LED_R_Clear();
    LED_Y_Clear();

    s_cb_baseline_valid = bms_read_cb_cell3_total_time(&s_cb_cell3_baseline_s);
    s_cb_last_active = 0xFFFFU;
    s_cb_last_present = 0xFFFFU;
    s_cb_last_cell3 = 0xFFFFFFFFUL;
    s_cb_last_ok = false;
    s_cb_occurred_reported = false;

    uart_write_text("BMS voltage test started\r\n");
    uart_write_text("voltage_order,c1,c2,c3,c4,c5,c10\r\n");

    len = snprintf(
        line,
        sizeof(line),
        "cb_baseline_cell3_s=%lu,%s\r\n",
        (unsigned long)s_cb_cell3_baseline_s,
        s_cb_baseline_valid ? "ok" : "fail");

    if (len > 0)
    {
        if ((size_t)len >= sizeof(line))
        {
            len = (int)(sizeof(line) - 1U);
        }
        (void)uart_write_blocking((const uint8_t *)line, (size_t)len);
    }
}

void voltage_test_step(void)
{
    uint16_t cell_mV[10] = {0U};
    uint16_t cb_active_mask = 0U;
    uint16_t cb_present_s = 0U;
    uint32_t cb_cell3_total_s = 0U;
    bool cb_active_ok;
    bool cb_present_ok;
    bool cb_cell3_ok;
    bool cb_ok;
    bool occurred;
    uint32_t delta_s = 0U;
    bool ok = read_cells_1to10(cell_mV);

    if (ok)
    {
        uart_write_voltages(cell_mV);
        LED_R_Clear();
    }
    else
    {
        uart_write_text("read fail\r\n");
        LED_R_Set();
    }

    cb_active_ok = bms_read_cb_active_cells(&cb_active_mask);
    cb_present_ok = bms_read_cb_present_time(&cb_present_s);
    cb_cell3_ok = bms_read_cb_cell3_total_time(&cb_cell3_total_s);
    cb_ok = (cb_active_ok && cb_present_ok && cb_cell3_ok);

    if (cb_ok && s_cb_baseline_valid && (cb_cell3_total_s >= s_cb_cell3_baseline_s))
    {
        delta_s = cb_cell3_total_s - s_cb_cell3_baseline_s;
    }

    occurred = (cb_ok && s_cb_baseline_valid && (delta_s > 0U));

    if ((cb_ok != s_cb_last_ok) ||
        (cb_active_mask != s_cb_last_active) ||
        (cb_present_s != s_cb_last_present) ||
        (cb_cell3_total_s != s_cb_last_cell3))
    {
        uart_write_balance_status(cb_ok, cb_active_mask, cb_present_s, cb_cell3_total_s, occurred, delta_s);

        s_cb_last_ok = cb_ok;
        s_cb_last_active = cb_active_mask;
        s_cb_last_present = cb_present_s;
        s_cb_last_cell3 = cb_cell3_total_s;
    }

    if (occurred && !s_cb_occurred_reported)
    {
        uart_write_text("cb_event,cell3_balancing_observed\r\n");
        s_cb_occurred_reported = true;
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
