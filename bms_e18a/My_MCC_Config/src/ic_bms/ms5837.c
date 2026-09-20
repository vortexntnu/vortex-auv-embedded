#include <stdbool.h>
#include <string.h>

#include "config/default/peripheral/systick/plib_systick.h"
#include "peripheral/sercom/i2c_master/plib_sercom1_i2c_master.h"
#include "ms5837.h"

#define MS5837_ADDR               (0x76U)
#define MS5837_RESET_DELAY_MS     (10U)
#define MS5837_I2C_TIMEOUT_LOOPS  (1000000UL)

#define MS5837_CMD_RESET      (0x1EU)
#define MS5837_CMD_PROM_READ  (0xA0U)
#define MS5837_CMD_ADC_READ   (0x00U)
#define MS5837_CMD_CONV_D1    (0x40U)
#define MS5837_CMD_CONV_D2    (0x50U)

static bool ms5837_wait_i2c_complete(void)
{
    uint32_t timeout = MS5837_I2C_TIMEOUT_LOOPS;

    while (SERCOM1_I2C_IsBusy())
    {
        if (timeout-- == 0U)
        {
            SERCOM1_I2C_TransferAbort();
            return false;
        }
    }

    return (SERCOM1_I2C_ErrorGet() == SERCOM_I2C_ERROR_NONE);
}

static bool ms5837_write_cmd(uint8_t cmd)
{
    if (!SERCOM1_I2C_Write(MS5837_ADDR, &cmd, 1))
    {
        return false;
    }

    return ms5837_wait_i2c_complete();
}

static bool ms5837_write_read_cmd(uint8_t cmd, uint8_t *rx, uint32_t rx_len)
{
    if ((rx == NULL) || (rx_len == 0U))
    {
        return false;
    }

    if (!SERCOM1_I2C_WriteRead(MS5837_ADDR, &cmd, 1, rx, rx_len))
    {
        return false;
    }

    return ms5837_wait_i2c_complete();
}

static uint8_t ms5837_crc4(const uint16_t prom[8])
{
    uint16_t n_prom[8];
    uint16_t n_rem = 0U;
    int cnt;
    int n_bit;

    memcpy(n_prom, prom, sizeof(n_prom));
    n_prom[0] &= 0x0FFFU;
    n_prom[7] = 0U;

    for (cnt = 0; cnt < 16; cnt++)
    {
        if ((cnt & 1) != 0)
        {
            n_rem ^= (uint16_t)(n_prom[cnt >> 1] & 0x00FFU);
        }
        else
        {
            n_rem ^= (uint16_t)(n_prom[cnt >> 1] >> 8);
        }

        for (n_bit = 0; n_bit < 8; n_bit++)
        {
            if ((n_rem & 0x8000U) != 0U)
            {
                n_rem = (uint16_t)((n_rem << 1) ^ 0x3000U);
            }
            else
            {
                n_rem <<= 1;
            }
        }
    }

    return (uint8_t)((n_rem >> 12) & 0x0FU);
}

static bool ms5837_reset(void)
{
    if (!ms5837_write_cmd(MS5837_CMD_RESET))
    {
        return false;
    }

    SYSTICK_DelayMs(MS5837_RESET_DELAY_MS);
    return true;
}

static bool ms5837_read_prom(struct ms5837_t *s)
{
    uint8_t rx[2];
    uint8_t cmd = MS5837_CMD_PROM_READ;
    uint8_t i;

    for (i = 0U; i < 8U; i++)
    {
        if (!ms5837_write_read_cmd(cmd, rx, sizeof(rx)))
        {
            return false;
        }

        s->C[i] = (uint16_t)(((uint16_t)rx[0] << 8) | rx[1]);
        cmd = (uint8_t)(cmd + 2U);
    }

    return true;
}

static uint16_t ms5837_conversion_delay_ms(uint8_t osr_code)
{
    switch (osr_code)
    {
        case MS5837_OSR_256:
            return 1U;
        case MS5837_OSR_512:
            return 2U;
        case MS5837_OSR_1024:
            return 3U;
        case MS5837_OSR_2048:
            return 5U;
        case MS5837_OSR_4096:
            return 10U;
        default:
            return 10U;
    }
}

static uint8_t ms5837_osr_sanitize(uint8_t osr_code)
{
    switch (osr_code)
    {
        case MS5837_OSR_256:
        case MS5837_OSR_512:
        case MS5837_OSR_1024:
        case MS5837_OSR_2048:
        case MS5837_OSR_4096:
            return osr_code;
        default:
            return MS5837_OSR_256;
    }
}

static bool ms5837_start_conversion(uint8_t base_cmd, uint8_t osr_code)
{
    if (!ms5837_write_cmd((uint8_t)(base_cmd + osr_code)))
    {
        return false;
    }

    SYSTICK_DelayMs(ms5837_conversion_delay_ms(osr_code));
    return true;
}

static bool ms5837_read_adc24(uint32_t *value)
{
    uint8_t rx[3];

    if (value == NULL)
    {
        return false;
    }

    if (!ms5837_write_read_cmd(MS5837_CMD_ADC_READ, rx, sizeof(rx)))
    {
        return false;
    }

    *value = ((uint32_t)rx[0] << 16) | ((uint32_t)rx[1] << 8) | rx[2];
    return true;
}

static void ms5837_compute(struct ms5837_t *s)
{
    int32_t dT;
    int32_t temp;
    int64_t off;
    int64_t sens;
    int32_t pressure;

    dT = (int32_t)s->D2 - ((int32_t)s->C[5] << 8);
    temp = 2000 + (int32_t)(((int64_t)dT * s->C[6]) >> 23);

    off = ((int64_t)s->C[2] << 17) + (((int64_t)s->C[4] * dT) >> 6);
    sens = ((int64_t)s->C[1] << 16) + (((int64_t)s->C[3] * dT) >> 7);
    pressure = (int32_t)(((((int64_t)s->D1 * sens) >> 21) - off) >> 15);

    if (temp < 2000)
    {
        int64_t ti;
        int64_t diff;
        int64_t off_i;
        int64_t sens_i;

        ti = 11 * (((int64_t)dT * dT) >> 35);
        diff = (int64_t)temp - 2000;
        off_i = (31 * diff * diff) >> 3;
        sens_i = (63 * diff * diff) >> 5;

        temp = (int32_t)(temp - ti);
        off -= off_i;
        sens -= sens_i;
        pressure = (int32_t)(((((int64_t)s->D1 * sens) >> 21) - off) >> 15);
    }

    s->temp_C = (float)temp * 0.01f;
    s->press_kPa = (float)pressure * 0.001f;
}

int8_t ms5837_init(struct ms5837_t *s)
{
    uint8_t osr_code;
    uint8_t crc_read;
    uint8_t crc_calc;

    if (s == NULL)
    {
        return -4;
    }

    osr_code = ms5837_osr_sanitize(s->osr_code);
    memset(s, 0, sizeof(*s));
    s->osr_code = osr_code;

    if (!ms5837_reset())
    {
        return -1;
    }

    if (!ms5837_read_prom(s))
    {
        return -2;
    }

    crc_read = (uint8_t)((s->C[0] >> 12) & 0x0FU);
    crc_calc = ms5837_crc4(s->C);
    if (crc_read != crc_calc)
    {
        return -3;
    }

    return 0;
}

bool ms5837_read_sample(struct ms5837_t *s)
{
    if (s == NULL)
    {
        return false;
    }

    s->has_fresh_sample = false;

    if (!ms5837_start_conversion(MS5837_CMD_CONV_D1, s->osr_code))
    {
        return false;
    }

    if (!ms5837_read_adc24(&s->D1))
    {
        return false;
    }

    if (!ms5837_start_conversion(MS5837_CMD_CONV_D2, s->osr_code))
    {
        return false;
    }

    if (!ms5837_read_adc24(&s->D2))
    {
        return false;
    }

    ms5837_compute(s);
    s->has_fresh_sample = true;
    return true;
}

void ms5837_task(struct ms5837_t *s)
{
    (void)ms5837_read_sample(s);
}
