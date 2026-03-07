#ifndef AD7606_DRIVER_H
#define AD7606_DRIVER_H

#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>
#include <stm32h7xx_hal.h>
#include <stm32h7xx.h>

#ifdef __cplusplus
extern "C" {
#endif

struct ad7606_device;

typedef void (*ad7606_data_ready)(struct ad7606_device* dev, uint8_t half);

typedef enum {
	AD7606_DOUT_1,
	AD7606_DOUT_2,
	AD7606_DOUT_4,
	AD7606_DOUT_8,
} AD7606_DOUT_FORMAT;

typedef enum {
	AD7606_OPERATION_NORMAL,
	AD7606_OPERATION_STANDBY,
	AD7606_OPERATION_AUTOSTANDBY,
	AD7606_OPERATON_SHUTDOWN,
} AD7606_OPERATION_MODE;

typedef enum {
	AD7606_RANGE_SE_PM_2_5V,     // ±2.5 V single-ended
	AD7606_RAGNE_SE_PM_5V,       // ±5 V single-ended
	AD7606_RANGE_SE_PM_6_25V,    // ±6.25 V single-ended
	AD7606_RANGE_SE_PM_10V,      // ±10 V single-ended
	AD7606_RANGE_SE_PM_12_5V,    // ±12.5 V single-ended
	AD7606_RANGE_SE_0_TO_5V,     // 0 to 5 V single-ended
	AD7606_RANGE_SE_0_TO_10V,    // 0 to 10 V single-ended
	AD7606_RANGE_SE_0_TO_12_5V,  // 0 to 12.5 V single-ended

	AD7606_RANGE_DIFF_PM_5V,     // ±5 V differential
	AD7606_RANGE_DIFF_PM_10V,    // ±10 V differential
	AD7606_RANGE_DIFF_PM_12_5V,  // ±12.5 V differential
	AD7606_RANGE_DIFF_PM_20V,    // ±20 V differential

} AD7606_CHANNEL_RANGE;

typedef enum {
	AD7606_MUX_CTRL_A_IN,
	AD7606_MUX_CTRL_TEMP,
	AD7606_MUX_CTRL_2V5_REF,
	AD7606_MUX_CTRL_1V8_ALDO,
	AD7606_MUX_CTRL_1V8_DLDO,
	AD7606_MUX_CTRL_V_DRIVE,
	AD7606_MUX_CTRL_A_GND,
	AD7606_MUX_CTRL_AV_CC,
} AD7606_CHANNEL_MUX_CTRL;

struct ad7606_pin {
	GPIO_TypeDef* GPIO_port;
	uint16_t GPIO_pin;
};

struct ad7606_pins {
	struct ad7606_pin cs;
	struct ad7606_pin busy;
	struct ad7606_pin frstdata;
	struct ad7606_pin convst;
};

struct ad7606_spi {
	SPI_HandleTypeDef* sdi;
	SPI_HandleTypeDef* douta;
	SPI_HandleTypeDef* doutb;
	SPI_HandleTypeDef* doutc;
	SPI_HandleTypeDef* doutd;
	SPI_HandleTypeDef* doute;
	SPI_HandleTypeDef* doutf;
	SPI_HandleTypeDef* doutg;
	SPI_HandleTypeDef* douth;
};

struct ad7606_register {
	uint8_t address;
	uint8_t data;
	bool read_only;
};

struct ad7606_registers {
	struct ad7606_register status;
	struct ad7606_register config;
	struct ad7606_register channel_range[4];
	struct ad7606_register bandwidth;
	struct ad7606_register oversampling;
	struct ad7606_register channel_gain[8];
	struct ad7606_register channel_offset[8];
	struct ad7606_register channel_phase[8];
	struct ad7606_register digital_diagnostics_enable;
	struct ad7606_register digital_diagnostics_error;
	struct ad7606_register open_detect_enable;
	struct ad7606_register open_detected;
	struct ad7606_register diagnostics_mux[4];
	struct ad7606_register open_detect_queue;
	struct ad7606_register fs_clk_counter;
	struct ad7606_register os_clk_counter;
	struct ad7606_register id;
};

struct ad7606_device {
    struct ad7606_registers* registers;
    struct ad7606_spi spi_handles;
    struct ad7606_pins pins;
    ad7606_data_ready on_ready; /* user callback when out[half] is filled */
};

struct ad7606_config {
    bool status_header;
    bool external_oversampling_clock;
    AD7606_DOUT_FORMAT dout_format;
    AD7606_OPERATION_MODE operation_mode;
};

struct ad7606_channel {
	bool open_detect;
	bool high_bandwidth;
	AD7606_CHANNEL_RANGE range;
	AD7606_CHANNEL_MUX_CTRL mux_ctrl;
    uint8_t gain;
    uint8_t offset;
    uint8_t phase;
};

struct ad7606_digital_diagnostics {
	bool rom_CRC_err_en;
	bool mm_CRC_err_en;
	bool int_CRC_err_en;
	bool spi_write_err_en;
	bool spi_read_err_en;
	bool busy_stuck_high_err_en;
	bool clk_fs_os_en;
	bool interface_check_en;
};

struct ad7606_oversampling {
	uint8_t oversampling_padding;
	uint8_t oversampling_ratio;
};

struct ad7606_settings {
	struct ad7606_oversampling oversampling;
	struct ad7606_digital_diagnostics digital_diagnostics;
	struct ad7606_channel channels[8];
	struct ad7606_config config;
};

// Public
void ad7606_init(struct ad7606_device* device,
				 struct ad7606_registers* registers,
				 struct ad7606_pins pins,
				 struct ad7606_spi spi_handles,
				 struct ad7606_settings settings);
void ad7606_set_registers(struct ad7606_device* device,
						  struct ad7606_settings settings);
uint8_t ad7606_read_register(const struct ad7606_device* const device, const struct ad7606_register reg);
void ad7606_write_to_register(struct ad7606_device* device, struct ad7606_register reg);
HAL_StatusTypeDef ad7606_write_to_register_DMA(struct ad7606_device* device, struct ad7606_register reg);
void ad7606_spi_tx_complete_handler(SPI_HandleTypeDef *hspi);
uint16_t ad7606_construct_SPI_frame(uint8_t command_bit, uint8_t read_write_bit, struct ad7606_register target_register);

#ifdef __cplusplus
}
#endif

#endif  // !AD7606_DRIVER_H
