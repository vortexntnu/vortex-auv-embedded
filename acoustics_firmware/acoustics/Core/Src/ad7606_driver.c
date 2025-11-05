
#include "ad7606_driver.h"
#include <string.h>
#include "stm32h7xx_hal_spi.h"

static inline void set_config(struct ad7606_config* cfg, uint8_t* config) {
    *config |= (cfg->status_header << 6) & (1 << 6);
    *config |= (cfg->external_oversampling_clock << 5) & (1 << 6);
    *config |= (cfg->dout_format << 3) & (0x4 << 3);
    *config |= (cfg->operation_mode) & (0x4);
}

static void set_channel_range(uint8_t* ranges,
                              struct ad7606_register* registers,
                              uint8_t num_channels) {
    int end = num_channels >> 1;

    for (int i = 0; i < end; i++) {
        registers->channel_range[i] =
            (ranges[2 * i + 1] << 4) | (ranges[2 * i] & 0xFF);
    }
}

void ad7606_init(struct ad7606_device* dev,
                 SPI_HandleTypeDef* hspi_master,
                 SPI_HandleTypeDef* hspi_sdo_1,
                 SPI_HandleTypeDef* hspi_sdo_2,
                 SPI_HandleTypeDef* hspi_sdo_3,
                 SPI_HandleTypeDef* hspi_sdo_4,
                 SPI_HandleTypeDef* hspi_sdo_5) {
    dev->hspi_master = hspi_master;
    dev->hspi_sdo_1 = hspi_sdo_1;
    dev->hspi_sdo_2 = hspi_sdo_2;
    dev->hspi_sdo_3 = hspi_sdo_3;
    dev->hspi_sdo_4 = hspi_sdo_4;
    dev->hspi_sdo_5 = hspi_sdo_5;
}

void ad7606_set_registers(struct ad7606_register* registers,
                          struct ad7606_config* config,
                          uint8_t* channel_range,
                          uint8_t* channel_gain,
                          uint8_t* channel_offset,
                          uint8_t* channel_phase,
                          uint8_t num_channels) {
    registers->config_address = AD7606_CONFIG_ADDRESS;
    set_config(config, &registers->config);
    set_channel_range(channel_range, registers, num_channels);
    memcpy(&registers->channel_gain, channel_gain, num_channels);
    memcpy(&registers->channel_offset, channel_offset, num_channels);
    memcpy(&registers->channel_phase, channel_phase, num_channels);
}

static inline void ad7606_send_registers(struct ad7606_device* dev, struct ad7606_register* reg){
    HAL_SPI_Transmit_DMA(dev->hspi_master, (void*) reg, sizeof(*reg));
}
