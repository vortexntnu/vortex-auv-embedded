#include "external_pressure.h"

#include <string.h>
#include <math.h>

/* Include your Harmony / peripheral header here */
#include "definitions.h"   /* or the correct header for SERCOM3_I2C_xxx */

/* ---------- Configuration ---------- */

#define MS5837_ADDR              0x76U
#define MS5837_RESET             0x1EU
#define MS5837_ADC_READ          0x00U
#define MS5837_PROM_READ         0xA0U
#define MS5837_CONVERT_D1_8192   0x4AU
#define MS5837_CONVERT_D2_8192   0x5AU

/* context: https://github.com/ArduPilot/ardupilot/pull/29122#issuecomment-2877269114 */
#define MS5837_02BA_MAX_SENSITIVITY   49000U
#define MS5837_02BA_30BA_SEPARATION   37000U
#define MS5837_30BA_MIN_SENSITIVITY   26000U

/* Replace this with your system delay function */
static void ms5837_delay_ms(uint32_t ms)
{
    /* Example options:
       - SYS_TIME_DelayMS(ms, &handle);
       - vTaskDelay(pdMS_TO_TICKS(ms));
       - core timer loop
    */

    /* Placeholder busy loop - replace for real hardware */
    volatile uint32_t count;
    while (ms--)
    {
        for (count = 0; count < 10000U; count++)
        {
            __asm__ volatile ("nop");
        }
    }
}

/* ---------- Internal helpers ---------- */

static bool ms5837_write_command(uint8_t cmd)
{
    return SERCOM3_I2C_Write(MS5837_ADDR, &cmd, 1U);
}

static bool ms5837_read_adc24(uint32_t *value)
{
    uint8_t cmd = MS5837_ADC_READ;
    uint8_t buf[3];

    if (!SERCOM3_I2C_WriteRead(MS5837_ADDR, &cmd, 1U, buf, 3U))
    {
        return false;
    }

    *value = ((uint32_t)buf[0] << 16)
           | ((uint32_t)buf[1] << 8)
           | ((uint32_t)buf[2]);

    return true;
}

static bool ms5837_read_prom_word(uint8_t index, uint16_t *word)
{
    uint8_t cmd = (uint8_t)(MS5837_PROM_READ + (index * 2U));
    uint8_t buf[2];

    if (!SERCOM3_I2C_WriteRead(MS5837_ADDR, &cmd, 1U, buf, 2U))
    {
        return false;
    }

    *word = ((uint16_t)buf[0] << 8) | (uint16_t)buf[1];
    return true;
}

static uint8_t ms5837_crc4(uint16_t n_prom[])
{
    uint16_t n_rem = 0;
    uint8_t i;
    uint8_t n_bit;

    n_prom[0] = (uint16_t)(n_prom[0] & 0x0FFFU);
    n_prom[7] = 0U;

    for (i = 0; i < 16U; i++)
    {
        if ((i % 2U) == 1U)
        {
            n_rem ^= (uint16_t)(n_prom[i >> 1] & 0x00FFU);
        }
        else
        {
            n_rem ^= (uint16_t)(n_prom[i >> 1] >> 8);
        }

        for (n_bit = 8U; n_bit > 0U; n_bit--)
        {
            if (n_rem & 0x8000U)
            {
                n_rem = (uint16_t)((n_rem << 1) ^ 0x3000U);
            }
            else
            {
                n_rem = (uint16_t)(n_rem << 1);
            }
        }
    }

    n_rem = (uint16_t)((n_rem >> 12) & 0x000FU);
    return (uint8_t)(n_rem ^ 0x00U);
}

static void ms5837_calculate(MS5837_t *dev)
{
    int32_t dT = 0;
    int64_t SENS = 0;
    int64_t OFF = 0;
    int32_t SENSi = 0;
    int32_t OFFi = 0;
    int32_t Ti = 0;
    int64_t OFF2 = 0;
    int64_t SENS2 = 0;

    dT = (int32_t)(dev->D2_temp - ((uint32_t)dev->C[5] * 256UL));

    if (dev->model == MS5837_02BA)
    {
        SENS = ((int64_t)dev->C[1] * 65536LL) + (((int64_t)dev->C[3] * dT) / 128LL);
        OFF  = ((int64_t)dev->C[2] * 131072LL) + (((int64_t)dev->C[4] * dT) / 64LL);
        dev->P = (int32_t)((((int64_t)dev->D1_pres * SENS) / 2097152LL - OFF) / 32768LL);
    }
    else
    {
        SENS = ((int64_t)dev->C[1] * 32768LL) + (((int64_t)dev->C[3] * dT) / 256LL);
        OFF  = ((int64_t)dev->C[2] * 65536LL) + (((int64_t)dev->C[4] * dT) / 128LL);
        dev->P = (int32_t)((((int64_t)dev->D1_pres * SENS) / 2097152LL - OFF) / 8192LL);
    }

    dev->TEMP = (int32_t)(2000LL + ((int64_t)dT * dev->C[6]) / 8388608LL);

    if (dev->model == MS5837_02BA)
    {
        if ((dev->TEMP / 100) < 20)
        {
            Ti    = (int32_t)((11LL * (int64_t)dT * (int64_t)dT) / 34359738368LL);
            OFFi  = (int32_t)((31LL * (dev->TEMP - 2000) * (dev->TEMP - 2000)) / 8LL);
            SENSi = (int32_t)((63LL * (dev->TEMP - 2000) * (dev->TEMP - 2000)) / 32LL);
        }
    }
    else
    {
        if ((dev->TEMP / 100) < 20)
        {
            Ti    = (int32_t)((3LL * (int64_t)dT * (int64_t)dT) / 8589934592LL);
            OFFi  = (int32_t)((3LL * (dev->TEMP - 2000) * (dev->TEMP - 2000)) / 2LL);
            SENSi = (int32_t)((5LL * (dev->TEMP - 2000) * (dev->TEMP - 2000)) / 8LL);

            if ((dev->TEMP / 100) < -15)
            {
                OFFi  += (int32_t)(7LL * (dev->TEMP + 1500) * (dev->TEMP + 1500));
                SENSi += (int32_t)(4LL * (dev->TEMP + 1500) * (dev->TEMP + 1500));
            }
        }
        else
        {
            Ti    = (int32_t)((2LL * (int64_t)dT * (int64_t)dT) / 137438953472LL);
            OFFi  = (int32_t)(((int64_t)(dev->TEMP - 2000) * (dev->TEMP - 2000)) / 16LL);
            SENSi = 0;
        }
    }

    OFF2 = OFF - OFFi;
    SENS2 = SENS - SENSi;

    dev->TEMP -= Ti;

    if (dev->model == MS5837_02BA)
    {
        dev->P = (int32_t)((((int64_t)dev->D1_pres * SENS2) / 2097152LL - OFF2) / 32768LL);
    }
    else
    {
        dev->P = (int32_t)((((int64_t)dev->D1_pres * SENS2) / 2097152LL - OFF2) / 8192LL);
    }
}

/* ---------- Public API ---------- */

bool MS5837_Begin(MS5837_t *dev)
{
    return MS5837_Init(dev);
}

bool MS5837_Init(MS5837_t *dev)
{
    uint8_t i;
    uint8_t crcRead;
    uint8_t crcCalculated;
    uint16_t prom_copy[8];

    if (dev == NULL)
    {
        return false;
    }

    memset(dev, 0, sizeof(*dev));
    dev->fluidDensity = 1029.0f;
    dev->model = MS5837_UNRECOGNISED;
    dev->initialized = false;

    SERCOM3_I2C_Initialize();

    if (!ms5837_write_command(MS5837_RESET))
    {
        return false;
    }

    ms5837_delay_ms(10U);

    for (i = 0; i < 7U; i++)
    {
        if (!ms5837_read_prom_word(i, &dev->C[i]))
        {
            return false;
        }
    }

    dev->C[7] = 0U;

    for (i = 0; i < 8U; i++)
    {
        prom_copy[i] = dev->C[i];
    }

    crcRead = (uint8_t)(dev->C[0] >> 12);
    crcCalculated = ms5837_crc4(prom_copy);

    if (crcCalculated != crcRead)
    {
        return false;
    }

    if ((dev->C[1] < MS5837_30BA_MIN_SENSITIVITY) ||
        (dev->C[1] > MS5837_02BA_MAX_SENSITIVITY))
    {
        dev->model = MS5837_UNRECOGNISED;
    }
    else if (dev->C[1] > MS5837_02BA_30BA_SEPARATION)
    {
        dev->model = MS5837_02BA;
    }
    else
    {
        dev->model = MS5837_30BA;
    }

    dev->initialized = true;
    return true;
}

void MS5837_SetModel(MS5837_t *dev, uint8_t model)
{
    if (dev != NULL)
    {
        dev->model = model;
    }
}

uint8_t MS5837_GetModel(const MS5837_t *dev)
{
    if (dev == NULL)
    {
        return MS5837_UNRECOGNISED;
    }

    return dev->model;
}

void MS5837_SetFluidDensity(MS5837_t *dev, float density)
{
    if (dev != NULL)
    {
        dev->fluidDensity = density;
    }
}

bool MS5837_Read(MS5837_t *dev)
{
    if ((dev == NULL) || (!dev->initialized))
    {
        return false;
    }

    if (!ms5837_write_command(MS5837_CONVERT_D1_8192))
    {
        return false;
    }

    ms5837_delay_ms(20U);

    if (!ms5837_read_adc24(&dev->D1_pres))
    {
        return false;
    }

    if (!ms5837_write_command(MS5837_CONVERT_D2_8192))
    {
        return false;
    }

    ms5837_delay_ms(20U);

    if (!ms5837_read_adc24(&dev->D2_temp))
    {
        return false;
    }

    ms5837_calculate(dev);
    return true;
}

float MS5837_Pressure(const MS5837_t *dev, float conversion)
{
    if (dev == NULL)
    {
        return 0.0f;
    }

    if (dev->model == MS5837_02BA)
    {
        return ((float)dev->P * conversion) / 100.0f;
    }
    else
    {
        return ((float)dev->P * conversion) / 10.0f;
    }
}

float MS5837_Temperature(const MS5837_t *dev)
{
    if (dev == NULL)
    {
        return 0.0f;
    }

    return (float)dev->TEMP / 100.0f;
}

float MS5837_Depth(const MS5837_t *dev)
{
    if (dev == NULL)
    {
        return 0.0f;
    }

    return (MS5837_Pressure(dev, MS5837_PA) - 101300.0f) / (dev->fluidDensity * 9.80665f);
}

float MS5837_Altitude(const MS5837_t *dev)
{
    if (dev == NULL)
    {
        return 0.0f;
    }

    return (1.0f - powf((MS5837_Pressure(dev, 1.0f) / 1013.25f), 0.190284f)) * 145366.45f * 0.3048f;
}
