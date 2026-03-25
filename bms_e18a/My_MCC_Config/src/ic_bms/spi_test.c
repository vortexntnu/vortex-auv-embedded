#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "definitions.h"
#include "app/can_facade.h"
#include "spi_test.h"
#include "bms_spi.h"

#define UART_TIMEOUT_LOOPS        (3000000UL)
#define VOLTAGE_TEST_DELAY_CYCLES (2400000UL)
#define CAN_SCOPE_TEST_PERIOD_MS  (100U)
#define STATUS_CURRENT_DEADBAND_MA (100)

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
    uint16_t ssb = 0U;
    uint16_t ssc = 0U;
    char line[80];
    int len;

    if (!bq_direct_command(AlarmStatus, &alarm, R))
        return;

    if (!bq_direct_command(SafetyStatusA, &ssa, R))
        return;

    if (!bq_direct_command(SafetyStatusB, &ssb, R))
        return;

    if (!bq_direct_command(SafetyStatusC, &ssc, R))
        return;

    len = snprintf(
        line,
        sizeof(line),
        "alert=0x%04X,ssa=0x%04X,ssb=0x%04X,ssc=0x%04X\r\n",
        (unsigned int)alarm,
        (unsigned int)ssa,
        (unsigned int)ssb,
        (unsigned int)ssc);

    if (len <= 0)
        return;

    if ((size_t)len >= sizeof(line))
        len = (int)(sizeof(line) - 1U);

    uart_write_blocking((const uint8_t *)line, (size_t)len);
}

static void uart_write_alert_pfa(void)
{
    uint16_t alarm = 0U;
    uint16_t ssa = 0U;
    uint16_t ssb = 0U;
    uint16_t ssc = 0U;
    uint16_t fet = 0U;
    char line[96];
    int len;

    if (!bq_direct_command(PFStatusA, &alarm, R))
        return;

    if (!bq_direct_command(PFStatusB, &ssa, R))
        return;

    if (!bq_direct_command(PFStatusC, &ssb, R))
        return;

    if (!bq_direct_command(PFStatusD, &ssc, R))
        return;

    if (!bq_direct_command(FETStatus, &fet, R))
        return;

    len = snprintf(
        line,
        sizeof(line),
        "pfa=0x%04X,pfb=0x%04X,pfc=0x%04X,pfd=0x%04X,fet=0x%04X\r\n",
        (unsigned int)alarm,
        (unsigned int)ssa,
        (unsigned int)ssb,
        (unsigned int)ssc,
        (unsigned int)fet);

    if (len <= 0)
        return;

    if ((size_t)len >= sizeof(line))
        len = (int)(sizeof(line) - 1U);

    uart_write_blocking((const uint8_t *)line, (size_t)len);
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
    // delay_cycles(VOLTAGE_TEST_DELAY_CYCLES);

    tx ^= 0xFFU;
}

void voltage_test_init(void)
{
    LED_R_Clear();
    LED_Y_Clear();

    uart_write_text("BMS voltage test started\r\n");
    uart_write_text("voltage_order,c1,c2,c3,c4,c5,c10\r\n");
}

static void uart_write_current(void)
{
    int16_t current = 0;
    char line[32];
    int len;

    if (!bq_direct_command(CC2Current, (uint16_t*)&current, R))
    {
        return;
    }

    len = snprintf(line, sizeof(line), "current=%d mA\r\n", current);

    if (len > 0 && len < sizeof(line))
    {
        uart_write_blocking((uint8_t*)line, len);
    }
}

static bool read_data_memory(uint16_t address, uint8_t *data, uint8_t length)
{
    uint8_t subcmd[2];
    uint8_t echo[2] = {0xFFU, 0xFFU};
    uint8_t frame[34];
    uint8_t payload_len;
    uint8_t copy_len;
    uint8_t checksum_calc;
    uint8_t checksum_read;
    uint32_t tries = 0U;
    uint8_t i;

    if ((data == NULL) || (length == 0U) || (length > 32U))
    {
        return false;
    }

    subcmd[0] = (uint8_t)(address & 0xFFU);
    subcmd[1] = (uint8_t)((address >> 8) & 0xFFU);

    if (write_reg(0x3EU, subcmd, 2U) != BQ_OK)
    {
        return false;
    }

    do
    {
        if (!read_reg(0x3EU, echo, 2U))
        {
            return false;
        }

        tries++;
        if (tries > BQ_SUBCMD_MAX_POLLS)
        {
            return false;
        }
    } while ((echo[0] == 0xFFU && echo[1] == 0xFFU) ||
             (echo[0] != subcmd[0]) ||
             (echo[1] != subcmd[1]));

    if (!read_reg(0x40U, frame, (uint8_t)sizeof(frame)))
    {
        return false;
    }

    if (frame[0x61U - 0x40U] < 4U)
    {
        return false;
    }

    payload_len = (uint8_t)(frame[0x61U - 0x40U] - 4U);
    if (payload_len > 32U)
    {
        payload_len = 32U;
    }

    copy_len = (length < payload_len) ? length : payload_len;
    memcpy(data, frame, copy_len);

    checksum_calc = (uint8_t)(subcmd[0] + subcmd[1]);
    for (i = 0U; i < payload_len; i++)
    {
        checksum_calc = (uint8_t)(checksum_calc + frame[i]);
    }
    checksum_calc = (uint8_t)(0xFFU - (checksum_calc & 0xFFU));
    checksum_read = frame[0x60U - 0x40U];

    return (checksum_calc == checksum_read);
}

static void uart_write_fet_options_and_mfg_status_init(void)
{
    uint8_t fet_options = 0U;
    uint8_t mfg_status_bytes[2] = {0U, 0U};
    uint16_t mfg_status_init;
    char line[56];
    int len;

    if (!read_data_memory(FETOptions, &fet_options, 1U))
    {
        return;
    }

    if (!read_data_memory(MfgStatusInit, mfg_status_bytes, 2U))
    {
        return;
    }

    mfg_status_init = (uint16_t)mfg_status_bytes[0] |
                      ((uint16_t)mfg_status_bytes[1] << 8);

    len = snprintf(
        line,
        sizeof(line),
        "fet_opt=0x%02X,mfg_init=0x%04X\r\n",
        (unsigned int)fet_options,
        (unsigned int)mfg_status_init);

    if (len > 0)
    {
        if ((size_t)len >= sizeof(line))
        {
            len = (int)(sizeof(line) - 1U);
        }
        (void)uart_write_blocking((const uint8_t *)line, (size_t)len);
    }
}

static const char *bms_state_to_mode_text(bms_state_t state)
{
    switch (state)
    {
        case BMS_STATE_PRECHARGE:
            return "PRECHARGE";
        case BMS_STATE_CHARGING:
            return "CHARGING";
        case BMS_STATE_DISCHARGING:
            return "DISCHARGING";
        case BMS_STATE_IDLE:
            return "IDLE";
        case BMS_STATE_TRANSITION:
            return "TRANSITION";
        case BMS_STATE_READ_FAIL:
        default:
            return "READ_FAIL";
    }
}

static void uart_write_battery_status(void)
{
    uint16_t batt = 0U;
    uint16_t ctrl = 0U;
    uint8_t fet = 0U;
    bms_state_t state = BMS_STATE_READ_FAIL;
    int16_t current_mA = 0;
    bool chg_on = false;
    bool pchg_on = false;
    bool dsg_on = false;
    bool fet_status_ok = false;
    bool current_ok = false;
    bool have_valid_fet_bits = false;
    const char *mode;
    char line[96];
    int len;

    if (!bq_direct_command(ControlStatus, &ctrl, R))
    {
        return;
    }

    if (!bq_direct_command(BatteryStatus, &batt, R))
    {
        return;
    }

    fet_status_ok = bms_battery_status_get(&fet, &state);
    current_ok = bq_direct_command(CC2Current, (uint16_t *)&current_mA, R);

    if (fet_status_ok)
    {
        chg_on = ((fet & (1U << 0)) != 0U);
        pchg_on = ((fet & (1U << 1)) != 0U);
        dsg_on = ((fet & (1U << 2)) != 0U);
        have_valid_fet_bits = (chg_on || pchg_on || dsg_on);
    }

    // Some setups report 0x00 in FET status during normal operation.
    // Fallback to one-hot CHG/DSG/PCHG flags from state/current direction.
    if (!have_valid_fet_bits)
    {
        chg_on = false;
        pchg_on = false;
        dsg_on = false;

        if (state == BMS_STATE_PRECHARGE)
        {
            pchg_on = true;
        }
        else if (state == BMS_STATE_CHARGING)
        {
            chg_on = true;
        }
        else if (state == BMS_STATE_DISCHARGING)
        {
            dsg_on = true;
        }
        else if (current_ok)
        {
            if (current_mA > STATUS_CURRENT_DEADBAND_MA)
            {
                chg_on = true;
            }
            else if (current_mA < -STATUS_CURRENT_DEADBAND_MA)
            {
                dsg_on = true;
            }
        }
    }

    if (pchg_on)
    {
        mode = "PRECHARGE";
    }
    else if (chg_on && !dsg_on)
    {
        mode = "CHARGING";
    }
    else if (dsg_on && !chg_on)
    {
        mode = "DISCHARGING";
    }
    else if (!chg_on && !dsg_on)
    {
        mode = "IDLE";
    }
    else
    {
        mode = bms_state_to_mode_text(state);
    }

    len = snprintf(
        line,
        sizeof(line),
        "mode=%s,chg=%u,pchg=%u,dsg=%u,ctrl=0x%04X,batt=0x%04X,fet=0x%04X\r\n",
        mode,
        (unsigned int)chg_on,
        (unsigned int)pchg_on,
        (unsigned int)dsg_on,
        (unsigned int)ctrl,
        (unsigned int)batt,
        (unsigned int)fet);

    if (len <= 0)
    {
        return;
    }

    if ((size_t)len >= sizeof(line))
    {
        len = (int)(sizeof(line) - 1U);
    }

    uart_write_blocking((const uint8_t *)line, (size_t)len);
}

void voltage_test_step(void)
{
    uint16_t cell_mV[6] = {0U};
    bool ok = read_cells_1to6(cell_mV);

    if (ok)
    {
        uart_write_voltages(cell_mV);
        uart_write_alert_ssa();
        
        uart_write_current();
        uart_write_battery_status();
        uart_write_fet_options_and_mfg_status_init();

        uart_write_alert_pfa();
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
