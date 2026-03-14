#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "definitions.h"
#include "spi_test.h"
#include "bms_spi.h"

#define UART_TIMEOUT_LOOPS        (3000000UL)
#define VOLTAGE_TEST_DELAY_CYCLES (24000000UL)

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

    while (SERCOM3_USART_WriteIsBusy())
    {
        if (timeout-- == 0U)
        {
            return false;
        }
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

static const char *status_to_text(bms_state_t state)
{
    switch (state)
    {
        case BMS_STATE_PRECHARGE:
            return "precharge";
        case BMS_STATE_CHARGING:
            return "charging";
        case BMS_STATE_DISCHARGING:
            return "discharging";
        case BMS_STATE_IDLE:
            return "idle";
        case BMS_STATE_TRANSITION:
            return "transition";
        default:
            return "status_fail";
    }
}

static void uart_write_voltages(const uint16_t cell_mV[6], const char *status_text, uint8_t fet_reg)
{
    char line[96]; 
    int len = snprintf( 
        line,
        sizeof(line),
        "%u,%u,%u,%u,%u,%u,%s,0x%02X\r\n",
        (unsigned int)cell_mV[0],
        (unsigned int)cell_mV[1],
        (unsigned int)cell_mV[2],
        (unsigned int)cell_mV[3],
        (unsigned int)cell_mV[4],
        (unsigned int)cell_mV[5],
        status_text,
        (unsigned int)fet_reg);

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

    ok = write_reg(BATTERY_STATUS, &tx, 1U);

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
        LED_G_Set();
        LED_R_Clear();
    }
    else
    {
        LED_G_Clear();
        LED_R_Set();
    }

    LED_Y_Toggle();
    delay_cycles(VOLTAGE_TEST_DELAY_CYCLES);

    tx ^= 0xFFU; /* alternate 0x5A/0xA5 for easy scope verification */
}

void voltage_test_init(void)
{
    LED_R_Clear();
    LED_Y_Clear();
    LED_G_Clear();

    bq76942_init();
    uart_write_text("BMS voltage test started\r\n");
}


void voltage_test_step(void)
{
    uint16_t cell_mV[6] = {0U};
    uint8_t fet_reg = 0U;
    bms_state_t state = BMS_STATE_READ_FAIL;
    bool status_ok = bms_battery_status_get(&fet_reg, &state);
    bool ok = read_cells_1to6(cell_mV);
    const char *status_text = status_ok ? status_to_text(state) : "status_fail";

    if (ok)
    {
        uart_write_voltages(cell_mV, status_text, fet_reg);
        LED_G_Set();
        LED_R_Clear();
    }
    else
    {
        uart_write_text("read fail\r\n");
        LED_G_Clear();
        LED_R_Set();
        
    }

    LED_Y_Toggle();
    delay_cycles(VOLTAGE_TEST_DELAY_CYCLES);
}
