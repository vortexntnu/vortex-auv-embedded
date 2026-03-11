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

#define AD7606_N_REGISTERS 44

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

typedef enum {
    AD7606_SPI_DOUTA = 0,
    AD7606_SPI_DOUTB,
    AD7606_SPI_DOUTC,
    AD7606_SPI_DOUTD,
    AD7606_SPI_DOUTE,
    AD7606_SPI_DOUTF,
    AD7606_SPI_DOUTG,
    AD7606_SPI_DOUTH,
	AD7606_SPI_SDI,
	AD7606_SPI_COUNT,
} ad7606_spi_index;

struct ad7606_register {
	uint8_t address;
	uint8_t data;
	bool read_only;
};

__attribute__((packed)) union ad7606_registers {
    struct {
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
    } by_name;
    struct ad7606_register all[AD7606_N_REGISTERS];
};

union ad7606_spi {
	struct {
		SPI_HandleTypeDef* douta;
		SPI_HandleTypeDef* doutb;
		SPI_HandleTypeDef* doutc;
		SPI_HandleTypeDef* doutd;
		SPI_HandleTypeDef* doute;
		SPI_HandleTypeDef* doutf;
		SPI_HandleTypeDef* doutg;
		SPI_HandleTypeDef* douth;
		SPI_HandleTypeDef* sdi;
	} by_name;
	SPI_HandleTypeDef* all[AD7606_SPI_COUNT];
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

struct ad7606_device {
    union ad7606_registers* registers;
    SPI_HandleTypeDef* spi_handles[AD7606_SPI_COUNT];
    struct ad7606_pins pins;
    struct ad7606_settings* settings;
    volatile uint16_t* diagnostic_sample;
    bool cooked;
};

extern const union ad7606_registers ad7606_default_registers;

// Initialization
void ad7606_init(struct ad7606_device* device,
                 union ad7606_registers* registers,
                 struct ad7606_pins pins,
				 union ad7606_spi spi,
                 struct ad7606_settings* settings,
				 volatile uint16_t* diagnostic_sample);

void ad7606_set_registers(struct ad7606_device* device,
                          struct ad7606_settings* settings);

// Diagnostics
uint8_t ad7606_check_status(struct ad7606_device* device);
void 	ad7606_check_interface(struct ad7606_device* device, uint8_t result[8]);
uint8_t ad7606_check_digital_error(struct ad7606_device* device);
uint8_t ad7606_check_open_detect(struct ad7606_device* device);

// Register access
uint8_t  ad7606_read_register(const struct ad7606_device* const device, const struct ad7606_register reg);
void     ad7606_write_to_register(struct ad7606_device* device, struct ad7606_register reg);
void     ad7606_enter_register_mode(const struct ad7606_device* const device);
void     ad7606_exit_register_mode(const struct ad7606_device* const device);
void     ad7606_enter_adc_mode(const struct ad7606_device* const device);
void     ad7606_exit_adc_mode(const struct ad7606_device* const device);
uint16_t ad7606_construct_SPI_frame(const uint8_t command_bit, const uint8_t read_write_bit, const struct ad7606_register target_register);

// Analog Output
void ad7606_init_output_buffers_DMA(struct ad7606_device* device, int16_t* buffers[8], int lengths[8]);
int16_t ad7606_read_adc_output(struct ad7606_device* device, ad7606_spi_index channel_id);
void ad7606_start_conversion_and_wait(struct ad7606_device* device);

// Fast SPI (direct register access, 125kHz diagnostic channel)
// NOTE: ad7606_fast_spi_callback() must be called from your SPI EOT ISR or timer tick
void ad7606_fast_spi_init(struct ad7606_device* device);
void ad7606_fast_spi_run(SPI_HandleTypeDef* hspi);
void ad7606_fast_spi_callback(void);

// Conversion utilities
double ad7606_reading_to_voltage(struct ad7606_device* device, uint8_t channel_id, int16_t reading);
double ad7606_voltage_to_temp(double voltage);

#ifdef __cplusplus
}
#endif

#endif  // !AD7606_DRIVER_H
