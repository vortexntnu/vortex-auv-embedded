#include "ad7606_driver.h"
#include <string.h>
#include "stm32h7xx_hal_spi.h"

static inline void set_config(struct ad7606_config* cfg, uint8_t* config) {
    *config |= (cfg->status_header << 6) & (1 << 6);
    *config |= (cfg->external_oversampling_clock << 5) & (1 << 6);
    *config |= (cfg->dout_format << 3) & (0x4 << 3);
    *config |= (cfg->operation_mode) & (0x4);
}

void ad7606_init(struct ad7606_device* dev,
                 struct ad7606_register* reg,
                 struct ad7606_config* cfg,
                 struct ad7606_channel* channels,
                 SPI_HandleTypeDef* hspi_master) {
    ad7606_set_registers(reg, cfg, channels, 8);
    dev->registers = reg;
    

    HAL_GPIO_WritePin(GPIOE, GPIO_PIN_9, GPIO_PIN_RESET);  // CS LOW

    HAL_SPI_Transmit(hspi_master, (void*)reg, sizeof(*reg), 10);

    HAL_GPIO_WritePin(GPIOE, GPIO_PIN_9, GPIO_PIN_SET); 
}

void ad7606_set_registers(struct ad7606_register* registers,
                          struct ad7606_config* config,
                          struct ad7606_channel* channels,
                          uint8_t num_channels) {
    registers->config_address = AD7606_CONFIG_ADDRESS;
    set_config(config, &registers->config);

    for (int i = 0; i < num_channels; i += 2) {
        registers->channel_range[i] =
            (channels[2 * i + 1].range << 4) | (channels[2 * i].range & 0xFF);
    }

    for (int i = 0; i < num_channels; i++) {
        registers->channel_gain[i] = channels->gain;
        registers->channel_offset[i] = channels->offset;
        registers->channel_phase[i] = channels->phase;
    }
}

