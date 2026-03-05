#include <ad7606_driver.h>
//#include "main.h" // refactor eventually so this won't be needed
#include <stdio.h>
#include <stm32h753xx.h>
#include <stm32h7xx_hal_gpio.h>
#include <stm32h7xx_hal_spi.h>
#include <sys/_stdint.h>

#define AD7606_MAX_DEVICES 6  // adjust as needed

#define CS device->pins.cs.GPIO_port, device->pins.cs.GPIO_pin
#define BUSY device->pins.busy.GPIO_port, device->pins.busy.GPIO_pin
#define FRSTDATA device->pins.frstdata.GPIO_port, device->pins.frstdata.GPIO_pin
#define CONVST device->pins.convst.GPIO_port, device->pins.convst.GPIO_pin

#define SDI device->spi_handles.sdi
#define DOUTA device->spi_handles.douta
#define DOUTB device->spi_handles.doutb
#define DOUTC device->spi_handles.doutc
#define DOUTD device->spi_handles.doutd
#define DOUTE device->spi_handles.doute
#define DOUTF device->spi_handles.doutf
#define DOUTG device->spi_handles.doutg
#define DOUTH device->spi_handles.douth

// Private (static - internal use only, not exposed in header)
static void ad7606_register_device(struct ad7606_device* device);
static void ad7606_set_spi(struct ad7606_device* device, struct ad7606_spi spi_handles);
static void ad7606_set_pins(struct ad7606_device* device, struct ad7606_pins pins);
static void ad7606_set_config(struct ad7606_device* device, struct ad7606_config config);
static void ad7606_set_channels(struct ad7606_device* device, struct ad7606_channel channels[8]);
static void ad7606_set_digital_diagnostics(struct ad7606_device* device, struct ad7606_digital_diagnostics diagnostics);
static void ad7606_set_oversampling(struct ad7606_device* device, struct ad7606_oversampling oversampling);
static void ad7606_write_all_registers(struct ad7606_device* device);

static struct ad7606_device* _ad7606_devices[AD7606_MAX_DEVICES] = {0};
static uint8_t _ad7606_device_count = 0;

const static uint16_t EXIT_REGISTER_MODE = 0x0000;
const static uint16_t EXIT_ADC_MODE = 0x4100;

struct ad7606_registers ad7606_default_registers =
{
    { 0x01, 0x00 },
    { 0x02, 0x08 },
{
    { 0x03, 0x33 },
    { 0x04, 0x33 },
    { 0x05, 0x33 },
    { 0x06, 0x33 }
},
    { 0x07, 0x00 },
    { 0x08, 0x00 },
{
    { 0x09, 0x00 },
    { 0x0A, 0x00 },
    { 0x0B, 0x00 },
    { 0x0C, 0x00 },
    { 0x0D, 0x00 },
    { 0x0E, 0x00 },
    { 0x0F, 0x00 },
    { 0x10, 0x00 }
},
{
    { 0x11, 0x80 },
    { 0x12, 0x80 },
    { 0x13, 0x80 },
    { 0x14, 0x80 },
    { 0x15, 0x80 },
    { 0x16, 0x80 },
    { 0x17, 0x80 },
    { 0x18, 0x80 }
},
{
    { 0x19, 0x00 },
    { 0x1A, 0x00 },
    { 0x1B, 0x00 },
    { 0x1C, 0x00 },
    { 0x1D, 0x00 },
    { 0x1E, 0x00 },
    { 0x1F, 0x00 },
    { 0x20, 0x00 }
},
    { 0x21, 0x01 },
    { 0x22, 0x00 },
    { 0x23, 0x00 },
    { 0x24, 0x00 },
{
    { 0x28, 0x00 },
    { 0x29, 0x00 },
    { 0x2A, 0x00 },
    { 0x2B, 0x00 }
},
    { 0x2C, 0x00 },
    { 0x2D, 0x00 },
    { 0x2E, 0x00 },
    { 0x2F, 0x31 },
};

static int conf_len = 44;

// Public
void ad7606_init(struct ad7606_device* device,
				 struct ad7606_registers* registers,
				 struct ad7606_pins pins,
				 struct ad7606_spi spi_handles,
				 struct ad7606_settings settings){

	*registers = ad7606_default_registers;
	device->registers = registers;

	ad7606_set_pins(device, pins);
	ad7606_set_spi(device, spi_handles);
	ad7606_set_registers(device, settings);
	ad7606_write_all_registers(device);
}

void ad7606_set_registers(struct ad7606_device* device,
						  struct ad7606_settings settings){
							
	ad7606_set_config(device, settings.config);
	ad7606_set_oversampling(device, settings.oversampling);
	ad7606_set_digital_diagnostics(device, settings.digital_diagnostics);
	ad7606_set_channels(device, settings.channels);
}

uint8_t ad7606_read_register(const struct ad7606_device* const device, const struct ad7606_register reg){
	uint16_t data_frames[2] = {ad7606_construct_SPI_frame(false, true, reg), EXIT_REGISTER_MODE};
	uint16_t receive_frames[2] = {0,0};

	HAL_GPIO_WritePin(CS, GPIO_PIN_RESET); // CS LOW
	HAL_SPI_TransmitReceive(SDI, (const uint8_t*)data_frames, (uint8_t*)&receive_frames, 2, 1);
	HAL_GPIO_WritePin(CS, GPIO_PIN_SET); // CS High

	return (uint8_t)(receive_frames[1] & 0x00FF);
}

void ad7606_write_to_register(struct ad7606_device* device, struct ad7606_register reg){
	uint16_t data_frame = ad7606_construct_SPI_frame(false, false, reg);

	HAL_GPIO_WritePin(CS, GPIO_PIN_RESET); // CS LOW
	HAL_SPI_Transmit(SDI, (const uint8_t*)&data_frame, 1, 1);
	HAL_GPIO_WritePin(CS, GPIO_PIN_SET); // CS High
}

HAL_StatusTypeDef ad7606_write_to_register_DMA(struct ad7606_device* device, struct ad7606_register reg){
	uint16_t data_frame = ad7606_construct_SPI_frame(false, false, reg); // non-static
	static uint16_t dma_frame[3]; // static buffer DMA reads from
	dma_frame[0] = EXIT_ADC_MODE;
	dma_frame[1] = data_frame;    // copy into stable static before DMA starts
	dma_frame[2] = EXIT_REGISTER_MODE;

	HAL_GPIO_WritePin(CS, GPIO_PIN_RESET); // CS LOW
	return HAL_SPI_Transmit_DMA(SDI, (const uint8_t*)&dma_frame, 3);
}

void ad7606_spi_tx_complete_handler(SPI_HandleTypeDef *hspi) {
    for (uint8_t i = 0; i < _ad7606_device_count; i++) {
        if (_ad7606_devices[i]->spi_handles.sdi == hspi) {
            HAL_GPIO_WritePin(
                _ad7606_devices[i]->pins.cs.GPIO_port,
                _ad7606_devices[i]->pins.cs.GPIO_pin,
                GPIO_PIN_SET
            );
        }
    }
}

uint16_t ad7606_construct_SPI_frame(uint8_t command_bit,
									uint8_t read_write_bit,
									struct ad7606_register target_register){
	uint16_t data_frame = 0x0000;
	read_write_bit |= target_register.read_only;
	data_frame |= ((command_bit << 15) | (read_write_bit << 14));
	data_frame |= ((target_register.address & 0x3F) << 8);
	data_frame |= ((target_register.data & 0xFF) << 0);

	return data_frame;
}

// Private
static void ad7606_register_device(struct ad7606_device *device) {
    if (_ad7606_device_count < AD7606_MAX_DEVICES) {
        _ad7606_devices[_ad7606_device_count++] = device;
    }
}

static void ad7606_set_spi(struct ad7606_device* device, struct ad7606_spi spi_handles){
    device->spi_handles = spi_handles;
}

static void ad7606_set_pins(struct ad7606_device* device, struct ad7606_pins pins){
    device->pins = pins;
}

static void ad7606_set_config(struct ad7606_device* device, struct ad7606_config config){
	uint8_t data = 0x00;
	data |= ((config.status_header & 0x01) << 6);
	data |= ((config.external_oversampling_clock & 0x01) << 5);
	data |= ((config.dout_format & 0x03) << 3);
	data |= ((config.operation_mode & 0x03) << 5);

	device->registers->config.data = data;
}

static void ad7606_set_channels(struct ad7606_device* device, struct ad7606_channel channels[8]){
	device->registers->bandwidth.data = 0x00;
	device->registers->open_detect_enable.data = 0x00;
	for(int i = 0; i < 8; i++){
		device->registers->bandwidth.data |= ((channels[i].high_bandwidth & 0x01) << i);
		device->registers->open_detect_enable.data |= ((channels[i].open_detect & 0x01) << i);
		device->registers->channel_gain[i].data = channels[i].gain;
		device->registers->channel_phase[i].data = channels[i].phase;
		device->registers->channel_offset[i].data = channels[i].offset;
		device->registers->channel_range[i/4].data &= ~((channels[i].range & 0x0F) << (4*(i/2)));
		device->registers->channel_range[i/4].data |= ((channels[i].range & 0x0F) << (4*(i/2)));
		device->registers->channel_range[i/4].data &= ~((channels[i].mux_ctrl & 0x07) << (3*(i/2)));
		device->registers->channel_range[i/4].data |= ((channels[i].mux_ctrl & 0x07) << (3*(i/2)));
	}
}

static void ad7606_set_digital_diagnostics(struct ad7606_device* device, struct ad7606_digital_diagnostics diagnostics){
    uint8_t data = 0x00;
    data |= ((diagnostics.interface_check_en      & 0x01) << 7);
    data |= ((diagnostics.clk_fs_os_en            & 0x01) << 6);
    data |= ((diagnostics.busy_stuck_high_err_en  & 0x01) << 5);
    data |= ((diagnostics.spi_read_err_en         & 0x01) << 4);
    data |= ((diagnostics.spi_write_err_en        & 0x01) << 3);
    data |= ((diagnostics.int_CRC_err_en          & 0x01) << 2);
    data |= ((diagnostics.mm_CRC_err_en           & 0x01) << 1);
    data |= ((diagnostics.rom_CRC_err_en          & 0x01) << 0);

    device->registers->digital_diagnostics_enable.data = data;
}

static void ad7606_set_oversampling(struct ad7606_device* device, struct ad7606_oversampling oversampling){
	uint8_t data = 0x00;
	data |= oversampling.oversampling_ratio & 0x0F;
	data |= oversampling.oversampling_padding & 0xF0;
	device->registers->oversampling.data = data;
}

static void ad7606_write_all_registers(struct ad7606_device* device){

	struct ad7606_register* registers = &device->registers->status;
	uint16_t data_frames[conf_len];

	for(int i = 0; i < conf_len; i++){
		data_frames[i] = ad7606_construct_SPI_frame(false, false, registers[i]);
	}

	HAL_GPIO_WritePin(CS, GPIO_PIN_RESET); // CS LOW
	HAL_SPI_Transmit(SDI, (const uint8_t*)&EXIT_ADC_MODE, 1, 1);
	HAL_SPI_Transmit(SDI, (const uint8_t*)data_frames, conf_len, 1);
	HAL_SPI_Transmit(SDI, (const uint8_t*)&EXIT_REGISTER_MODE, 1, 1);
	HAL_GPIO_WritePin(CS, GPIO_PIN_SET); // CS High
}
