#include "ms5837.h"
#include <math.h>
#include <string.h>

/* Include your Harmony-generated I2C header here */
#include "plib_sercom3_i2c_master.h"   // adjust filename if needed

/* Thresholds from your original code/context */
#define MS5837_02BA_MAX_SENSITIVITY    49000U
#define MS5837_02BA_30BA_SEPARATION    37000U
#define MS5837_30BA_MIN_SENSITIVITY    26000U

static uint8_t MS5837_CRC4(uint16_t n_prom[8]);
static bool MS5837_Reset(void);
static bool MS5837_ReadPROM(MS5837_t *dev);
static void MS5837_Calculate(MS5837_t *dev);

/* -------------------------------------------------------------------------- */
/* Replace this with a proper Harmony delay                                    */
/* -------------------------------------------------------------------------- */
void MS5837_DelayMs(uint32_t ms)
{
    /* Examples:
       - SYS_TIME_DelayMS(ms, &handle);
       - or a simple blocking loop if you already have one
       For now this is a stub to be implemented by you.
    */
    volatile uint32_t i, j;
    for (i = 0; i < ms; i++)
    {
        for (j = 0; j < 4000U; j++)
        {
            __asm__ volatile ("nop");
        }
    }
}

/* -------------------------------------------------------------------------- */

static bool MS5837_Reset(void)
{
    uint8_t cmd = MS5837_RESET;
    return SERCOM3_I2C_Write(MS5837_ADDR, &cmd, 1U);
}

static bool MS5837_ReadPROM(MS5837_t *dev)
{
    uint8_t addr;
    uint8_t rx[2];
    uint8_t i;

    for (i = 0; i < 8U; i++)
    {
        addr = (uint8_t)(MS5837_PROM_READ + (i * 2U));

        if (!SERCOM3_I2C_WriteRead(MS5837_ADDR, &addr, 1U, rx, 2U))
        {
            return false;
        }

        dev->C[i] = (uint16_t)(((uint16_t)rx[0] << 8) | (uint16_t)rx[1]);
    }

    return true;
}

bool MS5837_Init(MS5837_t *dev)
{
    uint8_t crcRead;
    uint8_t crcCalculated;
    uint16_t promCopy[8];

    if (dev == NULL)
    {
        return false;
    }

    memset(dev, 0, sizeof(*dev));
    dev->fluidDensity = 1029.0f;
    dev->model = MS5837_MODEL_UNRECOGNISED;
    dev->initialized = false;

    if (!MS5837_Reset())
    {
        return false;
    }

    MS5837_DelayMs(10U);

    if (!MS5837_ReadPROM(dev))
    {
        return false;
    }

    memcpy(promCopy, dev->C, sizeof(promCopy));

    crcRead = (uint8_t)(dev->C[0] >> 12);
    crcCalculated = MS5837_CRC4(promCopy);

    if (crcCalculated != crcRead)
    {
        return false;
    }

    /* Auto-detect sensor model from PROM word 1 */
    if ((dev->C[1] < MS5837_30BA_MIN_SENSITIVITY) ||
        (dev->C[1] > MS5837_02BA_MAX_SENSITIVITY))
    {
        dev->model = MS5837_MODEL_UNRECOGNISED;
    }
    else if (dev->C[1] > MS5837_02BA_30BA_SEPARATION)
    {
        dev->model = MS5837_MODEL_02BA;
    }
    else
    {
        dev->model = MS5837_MODEL_30BA;
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
        return MS5837_MODEL_UNRECOGNISED;
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
    uint8_t cmd;
    uint8_t rx[3];

    if ((dev == NULL) || (!dev->initialized))
    {
        return false;
    }

    /* Request D1 conversion */
    cmd = MS5837_CONVERT_D1_8192;
    if (!SERCOM3_I2C_Write(MS5837_ADDR, &cmd, 1U))
    {
        return false;
    }

    MS5837_DelayMs(20U);

    /* Read D1 */
    cmd = MS5837_ADC_READ;
    if (!SERCOM3_I2C_WriteRead(MS5837_ADDR, &cmd, 1U, rx, 3U))
    {
        return false;
    }

    dev->D1_pres = ((uint32_t)rx[0] << 16) |
                   ((uint32_t)rx[1] << 8)  |
                   ((uint32_t)rx[2]);

    /* Request D2 conversion */
    cmd = MS5837_CONVERT_D2_8192;
    if (!SERCOM3_I2C_Write(MS5837_ADDR, &cmd, 1U))
    {
        return false;
    }

    MS5837_DelayMs(20U);

    /* Read D2 */
    cmd = MS5837_ADC_READ;
    if (!SERCOM3_I2C_WriteRead(MS5837_ADDR, &cmd, 1U, rx, 3U))
    {
        return false;
    }

    dev->D2_temp = ((uint32_t)rx[0] << 16) |
                   ((uint32_t)rx[1] << 8)  |
                   ((uint32_t)rx[2]);

    MS5837_Calculate(dev);
    return true;
}

static void MS5837_Calculate(MS5837_t *dev)
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

    if (dev->model == MS5837_MODEL_02BA)
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

    /* Second-order compensation */
    if (dev->model == MS5837_MODEL_02BA)
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
                OFFi  += (int32_t)(7LL * (dev->TEMP + 1500L) * (dev->TEMP + 1500L));
                SENSi += (int32_t)(4LL * (dev->TEMP + 1500L) * (dev->TEMP + 1500L));
            }
        }
        else
        {
            Ti    = (int32_t)((2LL * (int64_t)dT * (int64_t)dT) / 137438953472LL);
            OFFi  = (int32_t)(((int64_t)(dev->TEMP - 2000) * (dev->TEMP - 2000)) / 16LL);
            SENSi = 0;
        }
    }

    OFF2  = OFF - OFFi;
    SENS2 = SENS - SENSi;

    dev->TEMP -= Ti;

    if (dev->model == MS5837_MODEL_02BA)
    {
        dev->P = (int32_t)(((((int64_t)dev->D1_pres * SENS2) / 2097152LL) - OFF2) / 32768LL);
    }
    else
    {
        dev->P = (int32_t)(((((int64_t)dev->D1_pres * SENS2) / 2097152LL) - OFF2) / 8192LL);
    }
}

float MS5837_Pressure(const MS5837_t *dev, float conversion)
{
    if (dev == NULL)
    {
        return 0.0f;
    }

    if (dev->model == MS5837_MODEL_02BA)
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

    return ((float)dev->TEMP) / 100.0f;
}

float MS5837_Depth(const MS5837_t *dev)
{
    float pressurePa;

    if (dev == NULL)
    {
        return 0.0f;
    }

    pressurePa = MS5837_Pressure(dev, MS5837_PA);
    return (pressurePa - 101300.0f) / (dev->fluidDensity * 9.80665f);
}

float MS5837_Altitude(const MS5837_t *dev)
{
    float pressure_mbar;

    if (dev == NULL)
    {
        return 0.0f;
    }

    pressure_mbar = MS5837_Pressure(dev, MS5837_MBAR);
    return (1.0f - powf((pressure_mbar / 1013.25f), 0.190284f)) * 145366.45f * 0.3048f;
}

static uint8_t MS5837_CRC4(uint16_t n_prom[8])
{
    uint16_t n_rem = 0;
    uint8_t i;
    uint8_t n_bit;

    n_prom[0] = (uint16_t)(n_prom[0] & 0x0FFFU);
    n_prom[7] = 0U;

    for (i = 0; i < 16U; i++)
    {
        if (i & 1U)
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
