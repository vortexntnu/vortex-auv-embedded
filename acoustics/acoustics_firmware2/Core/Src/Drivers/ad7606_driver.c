#include <ad7606_driver.h>

#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <assert.h>

#include <stm32h753xx.h>
#include <stm32h7xx_hal_gpio.h>
#include <stm32h7xx_hal_spi.h>

const union ad7606_registers ad7606_default_registers =
{
		.by_name = \
		{
		    { 0x01, 0x00, true },
		    { 0x02, 0x08, false },
		{
		    { 0x03, 0x33, false },
		    { 0x04, 0x33, false },
		    { 0x05, 0x33, false },
		    { 0x06, 0x33, false }
		},
		    { 0x07, 0x00, false },
		    { 0x08, 0x00, false },
		{
		    { 0x09, 0x00, false },
		    { 0x0A, 0x00, false },
		    { 0x0B, 0x00, false },
		    { 0x0C, 0x00, false },
		    { 0x0D, 0x00, false },
		    { 0x0E, 0x00, false },
		    { 0x0F, 0x00, false },
		    { 0x10, 0x00, false }
		},
		{
		    { 0x11, 0x80, false },
		    { 0x12, 0x80, false },
		    { 0x13, 0x80, false },
		    { 0x14, 0x80, false },
		    { 0x15, 0x80, false },
		    { 0x16, 0x80, false },
		    { 0x17, 0x80, false },
		    { 0x18, 0x80, false }
		},
		{
		    { 0x19, 0x00, false },
		    { 0x1A, 0x00, false },
		    { 0x1B, 0x00, false },
		    { 0x1C, 0x00, false },
		    { 0x1D, 0x00, false },
		    { 0x1E, 0x00, false },
		    { 0x1F, 0x00, false },
		    { 0x20, 0x00, false }
		},
		    { 0x21, 0x01, false },
		    { 0x22, 0x00, false },
		    { 0x23, 0x00, false },
		    { 0x24, 0x00, false },
		{
		    { 0x28, 0x00, false },
		    { 0x29, 0x00, false },
		    { 0x2A, 0x00, false },
		    { 0x2B, 0x00, false }
		},
		    { 0x2C, 0x00, false  },
		    { 0x2D, 0x00, true },
		    { 0x2E, 0x00, true },
		    { 0x2F, 0x31, true },
		}
};


//static uint8_t conf_len = 44;
// copy end

typedef char static_assert_register_count[
    sizeof(((union ad7606_registers*)0)->by_name) ==
    sizeof(((union ad7606_registers*)0)->all)
    ? 1 : -1
];

#define AD7606_MAX_DEVICES 2  // adjust as needed

#define CS device->pins.cs.GPIO_port, device->pins.cs.GPIO_pin
#define BUSY device->pins.busy.GPIO_port, device->pins.busy.GPIO_pin
#define FRSTDATA device->pins.frstdata.GPIO_port, device->pins.frstdata.GPIO_pin
#define CONVST device->pins.convst.GPIO_port, device->pins.convst.GPIO_pin

#define SDI   device->spi_handles[AD7606_SPI_SDI]
#define DOUTA device->spi_handles[AD7606_SPI_DOUTA]
#define DOUTB device->spi_handles[AD7606_SPI_DOUTB]
#define DOUTC device->spi_handles[AD7606_SPI_DOUTC]
#define DOUTD device->spi_handles[AD7606_SPI_DOUTD]
#define DOUTE device->spi_handles[AD7606_SPI_DOUTE]
#define DOUTF device->spi_handles[AD7606_SPI_DOUTF]
#define DOUTG device->spi_handles[AD7606_SPI_DOUTG]
#define DOUTH device->spi_handles[AD7606_SPI_DOUTH]

// Private (static - internal use only, not exposed in header)
static void ad7606_register_device(struct ad7606_device* device);
static void ad7606_set_spi(struct ad7606_device* device, union ad7606_spi spi);
static void ad7606_set_pins(struct ad7606_device* device, struct ad7606_pins pins);
static void ad7606_set_config(struct ad7606_device* device, struct ad7606_config config);
static void ad7606_set_channels(struct ad7606_device* device, struct ad7606_channel channels[8]);
static void ad7606_set_digital_diagnostics(struct ad7606_device* device, struct ad7606_digital_diagnostics diagnostics);
static void ad7606_set_oversampling(struct ad7606_device* device, struct ad7606_oversampling oversampling);
static void ad7606_write_all_registers(struct ad7606_device* device);

static struct ad7606_device* _ad7606_devices[AD7606_MAX_DEVICES] = {NULL};
static uint8_t _ad7606_device_count = 0;

static const uint16_t EXIT_REGISTER_MODE = 0x0000;
static const uint16_t EXIT_ADC_MODE = 0x4100;
static const uint16_t READ_AIN_FRAME = 0x0000;

static const double ad7606_conversion_table[] = {
		76.3e-6,                 // ±2.5 V single-ended
		152.6e-6,                // ±5 V single-ended
		190.7e-6,                // ±6.25 V single-ended
		305.2e-6,                // ±10 V single-ended
		381.5e-6,                // ±12.5 V single-ended

		76.3e-6,                 // 0 to 5 V single-ended
		152.6e-6,                // 0 to 10 V single-ended
		190.7e-6,                // 0 to 12.5 V single-ended'

		152.6e-6,                // ±5 V differential
		305.2e-6,                // ±10 V differential
		381.5e-6,                // ±12.5 V differential
		610.35e-6,               // ±20 V differential
};

// =============================================================================
// Initialization
// =============================================================================

void ad7606_init(struct ad7606_device* device,
                 union ad7606_registers* registers,
                 struct ad7606_pins pins,
				 union ad7606_spi spi,
                 struct ad7606_settings* settings,
				 volatile uint16_t* diagnostic_sample){

    *registers = ad7606_default_registers;
    device->registers = registers;
    device->settings = settings;
    device->diagnostic_sample = diagnostic_sample;

    ad7606_set_pins(device, pins);
    ad7606_set_spi(device, spi);
    ad7606_set_registers(device, settings);
    ad7606_register_device(device);

    ad7606_write_all_registers(device);
}

void ad7606_set_registers(struct ad7606_device* device,
                          struct ad7606_settings* settings){

    ad7606_set_config(device, settings->config);
    ad7606_set_oversampling(device, settings->oversampling);
    ad7606_set_digital_diagnostics(device, settings->digital_diagnostics);
    ad7606_set_channels(device, settings->channels);
}

// =============================================================================
// Diagnostics
// =============================================================================

uint8_t ad7606_check_status(struct ad7606_device* device){
	uint8_t status = ad7606_read_register(device, device->registers->by_name.status);
	return status;
}

void ad7606_check_interface(struct ad7606_device* device, uint8_t result[8]){

	struct ad7606_register reg = {
			.address = device->registers->by_name.digital_diagnostics_enable.address,
			.data = (device->registers->by_name.digital_diagnostics_enable.data | (1 << 7))
	};
	ad7606_write_to_register(device, reg);

	uint16_t test_buffers[AD7606_SPI_COUNT] = {0};
	uint16_t control_values[8] = {
		0xACCA,
		0x5CC5,
		0xA33A,
		0x5335,
		0xCAAC,
		0xC55C,
		0x3AA3,
		0x3553
	};
	if(device->cooked){
		for(int i = 0; i < 8; i++){
			control_values[(i+7)%8] = control_values[i];
		}
	}


    ad7606_enter_adc_mode(device);

    int master_idx = 8;
    for(int i = 0; i < 8; i++){
		if ((device->spi_handles[i] != NULL) &&
			(HAL_SPI_GetState(device->spi_handles[i]) == HAL_SPI_STATE_READY) &&
			(device->spi_handles[i] != SDI)){
			HAL_SPI_Receive_IT(device->spi_handles[i], (uint8_t*)&test_buffers[i], 1);
		}
		if(device->spi_handles[i] == SDI){
			master_idx = i;
		}
	}

    ad7606_start_conversion_and_wait(device);

    HAL_GPIO_WritePin(CS, GPIO_PIN_RESET); // CS LOW
    HAL_SPI_Receive(device->spi_handles[master_idx], (uint8_t*)&test_buffers[master_idx], 1, 1);
    HAL_GPIO_WritePin(CS, GPIO_PIN_SET); // CS HIGH

    ad7606_exit_adc_mode(device);

    for(int i = 0; i < 8; i++){
		if (device->spi_handles[i] != NULL){
			result[i] = (test_buffers[i] == control_values[i]);
		}else{
			result[i] = -1; // indicates unconfigured channel
		}
	}
    ad7606_write_to_register(device, device->registers->by_name.digital_diagnostics_enable);
}

uint8_t ad7606_check_digital_error(struct ad7606_device* device){
	uint8_t digital_errors = ad7606_read_register(device, device->registers->by_name.digital_diagnostics_error);
	return digital_errors;
}

uint8_t ad7606_check_open_detect(struct ad7606_device* device){
	uint8_t open_detect = ad7606_read_register(device, device->registers->by_name.open_detected);
	return open_detect;
}

// =============================================================================
// Register access
// =============================================================================

uint8_t ad7606_read_register(const struct ad7606_device* const device, const struct ad7606_register reg){
    uint16_t data_frame = ad7606_construct_SPI_frame(false, true, reg);
    uint16_t received_frame = 0;

    ad7606_enter_register_mode(device);

    HAL_GPIO_WritePin(CS, GPIO_PIN_RESET); // CS LOW
	HAL_SPI_Transmit(SDI, (const uint8_t*)&data_frame, 1, 1);
	HAL_GPIO_WritePin(CS, GPIO_PIN_SET); // CS HIGH

    HAL_SPI_Receive_IT(DOUTA, (uint8_t*)&received_frame, 1);

    HAL_GPIO_WritePin(CS, GPIO_PIN_RESET); // CS LOW
    HAL_SPI_Transmit(SDI, (const uint8_t*)&data_frame, 1, 1);
    HAL_GPIO_WritePin(CS, GPIO_PIN_SET); // CS HIGH

    ad7606_exit_register_mode(device);

    return (uint8_t)(received_frame & 0x00FF);
}

void ad7606_write_to_register(struct ad7606_device* device, struct ad7606_register reg){
	uint16_t data_frame = ad7606_construct_SPI_frame(false, false, reg);

	ad7606_enter_register_mode(device);

    HAL_GPIO_WritePin(CS, GPIO_PIN_RESET); // CS LOW
    HAL_SPI_Transmit(SDI, (const uint8_t*)&data_frame, 1, 1);
    HAL_GPIO_WritePin(CS, GPIO_PIN_SET); // CS HIGH

    ad7606_exit_register_mode(device);
}

void ad7606_enter_register_mode(const struct ad7606_device* const device){
    HAL_GPIO_WritePin(CS, GPIO_PIN_RESET); // CS LOW
    HAL_SPI_Transmit(SDI, (const uint8_t*)&EXIT_ADC_MODE, 1, 1);
    HAL_GPIO_WritePin(CS, GPIO_PIN_SET); // CS HIGH
}

void ad7606_exit_register_mode(const struct ad7606_device* const device){
    HAL_GPIO_WritePin(CS, GPIO_PIN_RESET); // CS LOW
    HAL_SPI_Transmit(SDI, (const uint8_t*)&EXIT_REGISTER_MODE, 1, 1);
    HAL_GPIO_WritePin(CS, GPIO_PIN_SET); // CS HIGH
}

void ad7606_enter_adc_mode(const struct ad7606_device* const device){
    HAL_GPIO_WritePin(CS, GPIO_PIN_RESET); // CS LOW
    HAL_SPI_Transmit(SDI, (const uint8_t*)&EXIT_REGISTER_MODE, 1, 1);
    HAL_GPIO_WritePin(CS, GPIO_PIN_SET); // CS HIGH
}

void ad7606_exit_adc_mode(const struct ad7606_device* const device){
    HAL_GPIO_WritePin(CS, GPIO_PIN_RESET); // CS LOW
    HAL_SPI_Transmit(SDI, (const uint8_t*)&EXIT_ADC_MODE, 1, 1);
    HAL_GPIO_WritePin(CS, GPIO_PIN_SET); // CS HIGH
}

uint16_t ad7606_construct_SPI_frame(const uint8_t command_bit,
									const uint8_t read_write_bit,
									const struct ad7606_register target_register){
    uint16_t data_frame = 0x0000;
    uint8_t rw_bit = read_write_bit || target_register.read_only;
    data_frame |= ((command_bit << 15) | (rw_bit << 14));
    data_frame |= ((target_register.address & 0x3F) << 8);
    data_frame |= ((target_register.data & 0xFF) << 0);

    return data_frame;
}

// =============================================================================
// Analog Output
// =============================================================================

// these need to be refactored to fit with all DOUTs
void ad7606_init_output_buffers_DMA(struct ad7606_device* device, int16_t* buffers[8], int lengths[8]){

    for(int i = 0; i < 8; i++){
        if ((lengths[i] > 0) &&
            (device->spi_handles[i] != NULL) &&
            (HAL_SPI_GetState(device->spi_handles[i]) == HAL_SPI_STATE_READY)){
            HAL_SPI_Receive_DMA(device->spi_handles[i], (uint8_t*)buffers[i], lengths[i]);
        }
    }
}

int16_t ad7606_read_adc_output(struct ad7606_device* device, ad7606_spi_index channel_id){

	int16_t received_output;

	ad7606_enter_adc_mode(device);

	HAL_SPI_Receive_IT(device->spi_handles[channel_id], (uint8_t*)&received_output, 1);

	ad7606_start_conversion_and_wait(device);

    HAL_GPIO_WritePin(CS, GPIO_PIN_RESET); // CS LOW
    HAL_SPI_Transmit(SDI, (uint8_t*)&READ_AIN_FRAME, 1, 1);
    HAL_GPIO_WritePin(CS, GPIO_PIN_SET); // CS HIGH

    ad7606_exit_adc_mode(device);

    return received_output;
}

void ad7606_start_conversion_and_wait(struct ad7606_device* device){

	HAL_GPIO_WritePin(CONVST, GPIO_PIN_SET);
	HAL_GPIO_WritePin(CONVST, GPIO_PIN_RESET);

	while(HAL_GPIO_ReadPin(BUSY)) __NOP();
}

// =============================================================================
// Fast SPI (direct register access, 125kHz diagnostic channel)
// NOTE: ad7606_fast_spi_callback() must be called from your SPI EOT ISR or timer tick.
//       Failure to do so will leave EOT/TXTF flags set and lock the SPI peripheral.
// =============================================================================

static uint32_t buffer_size = 1;
static volatile uint32_t rx_head = 0; /* incremented in EOT ISR    */
static int16_t* rx_buf;

void ad7606_dma_spi_init(struct ad7606_device* device,
						 DMA_HandleTypeDef* hdma_rx,
						 int16_t* __rx_buf,
						 uint32_t buff_size)
{
	SPI_HandleTypeDef* hspi = SDI;
    HAL_SPI_Abort(hspi);

    buffer_size = buff_size;
    rx_buf = __rx_buf;

    /* --- configure SPI peripheral directly --- */
    hspi->Instance->CR1 &= ~SPI_CR1_SPE;          // disable while configuring

    hspi->Instance->CFG2 &= ~SPI_CFG2_COMM_Msk;  // 0b00 = full duplex
    hspi->Instance->CFG1 &= ~SPI_CFG1_DSIZE_Msk;
    hspi->Instance->CFG1 |=  (15U << SPI_CFG1_DSIZE_Pos); // 16-bit frames

    /* Enable RX DMA request */
    hspi->Instance->CFG1 |= SPI_CFG1_RXDMAEN;

    /* TSIZE = 1: one 16-bit frame per CSTART burst */
    hspi->Instance->CR2 = 1U;

    /* Enable EOT interrupt */
    hspi->Instance->IER |= SPI_IER_EOTIE;

    /* --- configure DMA stream for circular RX --- */
    /* Assumes hdma_rx is already linked to SPI6_RX request in CubeMX  *
     * with: Memory increment ON, Peripheral increment OFF,            *
     *       data width 16-bit both sides, Circular mode               */\

    /* Manually start DMA in circular mode pointing at rx_buf */
    /* HAL_DMA_Start is fine here — we're NOT using HAL_SPI_Receive_DMA
     * because that would also arm CSTART via the HAL state machine     */
    HAL_DMA_Start(hdma_rx,
                  (uint32_t)&hspi->Instance->RXDR, // source: SPI RX FIFO
                  (uint32_t)__rx_buf,                 // dest:   your buffer
				  buffer_size);                // circular length

     hdma_rx->XferErrorCallback = ad7606_dma_error_callback;
     __HAL_DMA_ENABLE_IT(hdma_rx, DMA_IT_TE); // Transfer Error

    /* CS low — AD7606 ready */
    HAL_GPIO_WritePin(CS, GPIO_PIN_RESET);

    /* Arm SPI — does NOT start clocking, just enables the peripheral */
    hspi->Instance->CR1 |= SPI_CR1_SPE;
}

/* ---------------------------------------------------------------
 * Call this whenever you want one 16-bit burst
 * Safe to call from ISR or main loop
 * --------------------------------------------------------------- */
__attribute__((always_inline))
inline void ad7606_trigger_burst(SPI_HandleTypeDef *hspi)
{
    /* Reload TSIZE for next transaction (cleared after each EOT) */
    hspi->Instance->CR2 = 1U;

    /* On master TX-only or half-duplex you'd push a dummy word;
     * on full-duplex the TX FIFO needs something to clock out.  *
     * Writing TXDR is sufficient — no need to clear SPE first.  */
    *(volatile uint16_t*)&hspi->Instance->TXDR = 0xFFFF;

    /* Fire */
    hspi->Instance->CR1 |= SPI_CR1_CSTART;
}

/* ---------------------------------------------------------------
 * EOT ISR — called from SPI6_IRQHandler
 * --------------------------------------------------------------- */
void ad7606_eot_callback(SPI_HandleTypeDef *hspi, int device_id)
{
    if (hspi->Instance->SR & SPI_SR_EOT)
    {
        /* DMA has already written the word into rx_buf[rx_head % BUF_DEPTH]
         * because the DMA transfer completed before EOT fires.            *
         * Just track position and clear flags.                            */
         *(_ad7606_devices[device_id]->diagnostic_sample) = rx_buf[rx_head % buffer_size];

        rx_head++;

        hspi->Instance->IFCR = SPI_IFCR_EOTC | SPI_IFCR_TXTFC;
        /* CR2 is auto-cleared after EOT on H7 — reload happens in trigger */
    }
}

static void ad7606_dma_error_callback(DMA_HandleTypeDef *hdma)
{
    // Log / set a flag for diagnostics
	printf("SPI Error\r\n");

	struct ad7606_device* device = _ad7606_devices[0];
	SPI_HandleTypeDef* hspi = SDI;

    // Restart the stream
    HAL_DMA_Abort(hdma);
    HAL_DMA_Start(hdma,
                  (uint32_t)&hspi->Instance->RXDR,
                  (uint32_t)rx_buf,
                  buffer_size);
}

// =============================================================================
// Conversion utilities
// =============================================================================

double ad7606_reading_to_voltage(struct ad7606_device* device, uint8_t channel_id, int16_t reading){
    AD7606_CHANNEL_RANGE range = device->settings->channels[channel_id].range;
    if((AD7606_RANGE_SE_0_TO_5V <= range) && (range <= AD7606_RANGE_SE_0_TO_12_5V)) reading = (uint16_t)reading;
    return (double)reading * ad7606_conversion_table[range];
}

double ad7606_channel_scaling_factor(struct ad7606_device* device, uint8_t channel_id){
    AD7606_CHANNEL_RANGE range = device->settings->channels[channel_id].range;
    return ad7606_conversion_table[range];
}

double ad7606_voltage_to_temp(double voltage){
    return (voltage - 0.18353)/0.000480 + 25;
}

// =============================================================================
// Private
// =============================================================================

static void ad7606_register_device(struct ad7606_device *device) {
    if (_ad7606_device_count < AD7606_MAX_DEVICES) {
    	device->device_id = _ad7606_device_count;
        _ad7606_devices[_ad7606_device_count++] = device;
    }
}

static void ad7606_set_spi(struct ad7606_device* device, union ad7606_spi spi){
	memcpy(device->spi_handles, spi.all, AD7606_SPI_COUNT * sizeof(SPI_HandleTypeDef*));
}

static void ad7606_set_pins(struct ad7606_device* device, struct ad7606_pins pins){
    device->pins = pins;
}

static void ad7606_set_config(struct ad7606_device* device, struct ad7606_config config){
    uint8_t data = 0x00;
    data |= ((config.status_header & 0x01) << 6);
    data |= ((config.external_oversampling_clock & 0x01) << 5);
    data |= ((config.dout_format & 0x03) << 3);
    data |= ((config.operation_mode & 0x03) << 0);

    device->registers->by_name.config.data = data;
}

static void ad7606_set_channels(struct ad7606_device* device, struct ad7606_channel channels[8]){
    device->registers->by_name.bandwidth.data = 0x00;
    device->registers->by_name.open_detect_enable.data = 0x00;
    for(int i = 0; i < 8; i++){
        device->registers->by_name.bandwidth.data |= ((channels[i].high_bandwidth & 0x01) << i);
        device->registers->by_name.open_detect_enable.data |= ((channels[i].open_detect & 0x01) << i);
        device->registers->by_name.channel_gain[i].data = channels[i].gain;
        device->registers->by_name.channel_phase[i].data = channels[i].phase;
        device->registers->by_name.channel_offset[i].data = channels[i].offset;
        device->registers->by_name.channel_range[i/2].data &= ~((0x0F) << (4*(i%2)));
        device->registers->by_name.channel_range[i/2].data |= ((channels[i].range & 0x0F) << (4*(i%2)));
        device->registers->by_name.diagnostics_mux[i/2].data &= ~(0x07 << (3*(i%2)));
        device->registers->by_name.diagnostics_mux[i/2].data |= ((channels[i].mux_ctrl & 0x07) << (3*(i%2)));
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

    device->registers->by_name.digital_diagnostics_enable.data = data;
}

static void ad7606_set_oversampling(struct ad7606_device* device, struct ad7606_oversampling oversampling){
    uint8_t data = 0x00;
    data |= oversampling.oversampling_ratio & 0x0F;
    data |= oversampling.oversampling_padding & 0xF0;
    device->registers->by_name.oversampling.data = data;
}

static void ad7606_write_all_registers(struct ad7606_device* device){
    struct ad7606_register* registers = &device->registers->by_name.status;
    uint16_t data_frames[AD7606_N_REGISTERS];

    for(int i = 0; i < AD7606_N_REGISTERS; i++){
        data_frames[i] = ad7606_construct_SPI_frame(false, false, registers[i]);
    }

    ad7606_enter_register_mode(device);

    HAL_GPIO_WritePin(CS, GPIO_PIN_RESET); // CS LOW
    HAL_SPI_Transmit(SDI, (const uint8_t*)data_frames, AD7606_N_REGISTERS, 1);
    HAL_GPIO_WritePin(CS, GPIO_PIN_SET); // CS HIGH

    ad7606_exit_register_mode(device);
}
