#include "bmp280_service.h"

#include <stdbool.h>

#include "bme280.h"

#include "peripheral/sercom/i2c_master/plib_sercom3_i2c_master.h"
#include "peripheral/systick/plib_systick.h"

static uint8_t dev_addr = BME280_I2C_ADDR_PRIM;
static struct bme280_dev dev;
static struct bme280_settings settings;
static uint8_t settings_sel;
static bool sample_pending = false;
static uint32_t sample_ready_ms = 0U;

enum
{
    BMP280_SAMPLE_OK = 0,
    BMP280_SAMPLE_NOT_READY = 1
};

static int8_t platform_i2c_wait(uint32_t timeout_ms)
{
    uint32_t loops = timeout_ms * 1000U;

    while (SERCOM3_I2C_IsBusy())
    {
        if (loops == 0U)
        {
            SERCOM3_I2C_TransferAbort();
            return -1;
        }

        SYSTICK_DelayUs(1);
        loops--;
    }

    if (SERCOM3_I2C_ErrorGet() != SERCOM_I2C_ERROR_NONE)
    {
        return -1;
    }

    return 0;
}

static int8_t platform_i2c_read(uint8_t reg_addr, uint8_t *reg_data, uint32_t len, void *intf_ptr)
{
    uint8_t dev_addr_local = *(uint8_t *)intf_ptr;

    if (SERCOM3_I2C_WriteRead(dev_addr_local, &reg_addr, 1, reg_data, len) == false)
    {
        return -1;
    }

    return platform_i2c_wait(50);
}

static int8_t platform_i2c_write(uint8_t reg_addr, const uint8_t *reg_data, uint32_t len, void *intf_ptr)
{
    uint8_t dev_addr_local = *(uint8_t *)intf_ptr;
    uint8_t tx[16];

    if (len > (sizeof(tx) - 1U))
    {
        return -1;
    }

    tx[0] = reg_addr;
    for (uint32_t i = 0; i < len; i++)
    {
        tx[i + 1U] = reg_data[i];
    }

    if (SERCOM3_I2C_Write(dev_addr_local, tx, len + 1U) == false)
    {
        return -1;
    }

    return platform_i2c_wait(50);
}

static void platform_delay_us(uint32_t period, void *intf_ptr)
{
    (void)intf_ptr;
    SYSTICK_DelayUs(period);
}

int8_t bmp280_init_device(void)
{
    int8_t rslt;

    dev.intf = BME280_I2C_INTF;
    dev.read = platform_i2c_read;
    dev.write = platform_i2c_write;
    dev.delay_us = platform_delay_us;
    dev.intf_ptr = &dev_addr;

    rslt = bme280_init(&dev);
    if (rslt != BME280_OK)
    {
        return rslt;
    }

    settings.osr_t = BME280_OVERSAMPLING_1X;
    settings.osr_p = BME280_OVERSAMPLING_4X;

    settings.filter = BME280_FILTER_COEFF_OFF;
    settings.standby_time = BME280_STANDBY_TIME_0_5_MS;

    settings_sel =
               BME280_SEL_OSR_PRESS |
               BME280_SEL_OSR_TEMP |
               BME280_SEL_FILTER |
               BME280_SEL_STANDBY;

    rslt = bme280_set_sensor_settings(settings_sel, &settings, &dev);
    if (rslt == BME280_OK)
    {
        sample_pending = false;
        sample_ready_ms = 0U;
    }

    return rslt;
}

int8_t bmp280_start_sample(uint32_t now_ms)
{
    int8_t rslt;
    uint32_t delay_us;
    uint32_t delay_ms;

    if (sample_pending)
    {
        return BMP280_SAMPLE_NOT_READY;
    }

    rslt = bme280_set_sensor_mode(BME280_POWERMODE_FORCED, &dev);
    if (rslt != BME280_OK)
    {
        return rslt;
    }

    bme280_cal_meas_delay(&delay_us, &settings);
    delay_ms = (delay_us + 999U) / 1000U;

    sample_ready_ms = now_ms + delay_ms;
    sample_pending = true;

    return BMP280_SAMPLE_OK;
}

int8_t bmp280_try_read_sample(uint32_t now_ms, float *temperature, float *pressure)
{
    int8_t rslt;
    struct bme280_data data;

    if ((temperature == NULL) || (pressure == NULL))
    {
        return BME280_E_NULL_PTR;
    }

    if (!sample_pending)
    {
        return BMP280_SAMPLE_NOT_READY;
    }

    if ((uint32_t)(now_ms - sample_ready_ms) < 0x80000000U)
    {
        rslt = bme280_get_sensor_data(BME280_PRESS | BME280_TEMP, &data, &dev);
        if (rslt == BME280_OK)
        {
            *temperature = data.temperature;
            *pressure = data.pressure;
            sample_pending = false;
            return BMP280_SAMPLE_OK;
        }

        sample_pending = false;
        return rslt;
    }

    return BMP280_SAMPLE_NOT_READY;
}

int8_t bmp280_read_sample(float *temperature, float *pressure)
{
    int8_t rslt;
    uint32_t delay_us;
    struct bme280_data data;

    if ((temperature == NULL) || (pressure == NULL))
    {
        return BME280_E_NULL_PTR;
    }

    rslt = bme280_set_sensor_mode(BME280_POWERMODE_FORCED, &dev);
    if (rslt != BME280_OK)
    {
        return rslt;
    }

    bme280_cal_meas_delay(&delay_us, &settings);
    dev.delay_us(delay_us, dev.intf_ptr);

    rslt = bme280_get_sensor_data(BME280_PRESS | BME280_TEMP, &data, &dev);
    if (rslt == BME280_OK)
    {
        *temperature = data.temperature;
        *pressure = data.pressure;
    }

    return rslt;
}