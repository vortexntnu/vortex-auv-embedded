#ifndef AD7606_DRIVER_H
#define AD7606_DRIVER_H

#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>
#include <stm32h7xx_hal.h>
#include <stm32h7xx.h>

#define AD7606_CONFIG_ADDRESS 0x02

#ifdef __cplusplus
extern "C" {
#endif

struct ad7606_device;

typedef void (*ad7606_data_ready)(struct ad7606_device* dev, uint8_t half);

typedef enum {
    DOUT_1,
    DOUT_2,
    DOUT_4,
    DOUT_8,
} DOUT_FORMAT;

typedef enum {
    OPERATION_NORMAL,
    OPERATION_STANDBY,
    OPERATION_AUTOSTANDBY,
    OPERATON_SHUTDOWN,
} OPERATION_MODE;

typedef enum {
    RANGE_SE_PM_2_5V,     // ±2.5 V single-ended
    RAGNE_SE_PM_5V,       // ±5 V single-ended
    RANGE_SE_PM_6_25V,    // ±6.25 V single-ended
    RANGE_SE_PM_10V,      // ±10 V single-ended
    RANGE_SE_PM_12_5V,    // ±12.5 V single-ended
    RANGE_SE_0_TO_5V,     // 0 to 5 V single-ended
    RANGE_SE_0_TO_10V,    // 0 to 10 V single-ended
    RANGE_SE_0_TO_12_5V,  // 0 to 12.5 V single-ended

    RANGE_DIFF_PM_5V,     // ±5 V differential
    RANGE_DIFF_PM_10V,    // ±10 V differential
    RANGE_DIFF_PM_12_5V,  // ±12.5 V differential
    RANGE_DIFF_PM_20V,    // ±20 V differential

} AD7606_CHANNEL_RANGE;

struct ad7606_config {
    bool status_header;
    bool external_oversampling_clock;
    uint8_t dout_format;
    uint8_t operation_mode;
};

struct ad7606_register {
    uint8_t config_address;
    uint8_t config;
    uint8_t channel_range[4];
    uint8_t bandwith;
    uint8_t oversampling;
    uint8_t channel_gain[8];
    uint8_t channel_offset[8];
    uint8_t channel_phase[8];
};

struct ad7606_channel {
    uint8_t range;
    uint8_t gain;
    uint8_t offset;
    uint8_t phase;
};

struct ad7606_device {
    struct ad7606_register* registers;
    ad7606_data_ready on_ready; /* user callback when out[half] is filled */
};



void ad7606_init(struct ad7606_device* dev,
                 struct ad7606_register* reg,
                 struct ad7606_config* cfg,
                 struct ad7606_channel* channels,
                 SPI_HandleTypeDef* hspi_master);

void ad7606_set_registers(struct ad7606_register* registers,
                          struct ad7606_config* config,
                          struct ad7606_channel* channels,
                          uint8_t num_channels);

void ad7606_init_from_arrays(SPI_HandleTypeDef* hspi_master);

void ad7606_init_from_arrays_debug(SPI_HandleTypeDef* hspi_master_send,SPI_HandleTypeDef* hspi_master_receive);

uint16_t ad7606_construct_SPI_frame(uint8_t read_enable, uint8_t read_write, uint8_t adc_register_address, uint8_t data);

void ad7606_DOUT8_read_adc(SPI_HandleTypeDef* const spi_handle_array[6], int16_t received_data[8]);

void ad7606_DOUT4_read_adc(SPI_HandleTypeDef* const spi_handle_array[6], int16_t received_data[8]);

void ad7606_DOUT1_read_adc(SPI_HandleTypeDef* const spi_handle_array[6], int16_t received_data[8]);

void ad7606_read_registers(SPI_HandleTypeDef* hspi_master_send, SPI_HandleTypeDef* hspi_master_receive);

#ifdef __cplusplus
}
#endif

#endif  // !AD7606_DRIVER_H
