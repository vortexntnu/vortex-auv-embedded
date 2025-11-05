
#include "ad7606_driver.h"
#include "stm32h7xx_hal_spi.h"



static inline void set_config(struct ad7606_config* cfg, uint8_t* config) {
    *config |= (cfg->status_header << 6) & (1 << 6);
    *config |= (cfg->external_oversampling_clock << 5) & (1 << 6);
    *config |= (cfg->dout_format << 3) & (0x4 << 3);
    *config |= (cfg->operation_mode) & (0x4);
}

void ad7606_init(struct ad7606_device* dev,
                 struct ad7606_config* cfg,
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
    set_config(cfg, &dev->registers.config);

}





