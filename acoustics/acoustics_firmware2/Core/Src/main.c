/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2026 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "ad7606_driver.h"
#include "acoustics.h"
#include "utils.h"
#include "memory_placement.h"
#include "embedded_macros.h"
#include "find_peaks.h"
#include "dsp.h"
#include "cwt.h"
#include "hilbert.h"
#include "interrupts.h"
#include "fast_mdma.h"
#include "stm_temp_driver.h"
#include "tdoa.h"

#include "stm32h753xx.h"
#include "stm32h7xx_hal.h"
#include "stm32h7xx_hal_gpio.h"
#include "stm32h7xx_hal_gpio_ex.h"
#include "stm32h7xx_hal_spi.h"
#include "stm32h7xx_hal_mdma.h"
#include "stm32h7xx_hal_fdcan.h"

#include "arm_math_types.h"
#include "arm_math.h"
#include "arm_const_structs.h"

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc3;
DMA_HandleTypeDef hdma_adc3;

CRC_HandleTypeDef hcrc;

FDCAN_HandleTypeDef hfdcan1;

RTC_HandleTypeDef hrtc;

SPI_HandleTypeDef hspi1;
SPI_HandleTypeDef hspi2;
SPI_HandleTypeDef hspi3;
SPI_HandleTypeDef hspi4;
SPI_HandleTypeDef hspi5;
SPI_HandleTypeDef hspi6;
DMA_HandleTypeDef hdma_spi1_rx;
DMA_HandleTypeDef hdma_spi2_rx;
DMA_HandleTypeDef hdma_spi3_rx;
DMA_HandleTypeDef hdma_spi4_rx;
DMA_HandleTypeDef hdma_spi5_rx;
DMA_HandleTypeDef hdma_spi6_rx;

TIM_HandleTypeDef htim1;
TIM_HandleTypeDef htim6;
DMA_HandleTypeDef hdma_tim1_ch4;

UART_HandleTypeDef huart1;
DMA_HandleTypeDef hdma_usart1_tx;

MDMA_HandleTypeDef hmdma_mdma_channel0_sw_0;
/* USER CODE BEGIN PV */
static struct ad7606_device my_ADC;
static union ad7606_registers ADC_regs;
static struct ad7606_settings ADC_settings;

SPI_HandleTypeDef* const dout_channel_handles[N_HYDROPHONES] = {DOUTA, DOUTB, DOUTC, DOUTD, DOUTE};
volatile DMA_SPI_ChannelState dma_channel_state[N_HYDROPHONES + 1] = {DMA_SPI_IDLE};

PLACE_IN_D2_SRAM q15_t hydrophone_buffers[N_HYDROPHONES][N_BLOCKS][BLOCK_LEN];

PLACE_IN_D3_SRAM q15_t diagnostics_buffer[BLOCK_LEN];
volatile uint16_t diagnostics_sample;
float32_t diagnostics_temp;
PLACE_IN_D3_SRAM float32_t stm32_temp;

PLACE_IN_DTCM arm_rfft_fast_instance_f32 detection_fft_instance_f32;
PLACE_IN_DTCM arm_rfft_instance_q15 detection_fft_instance_q15;
MDMA_BUF_DTCM(ALIGN_DMA_BURST_8_WORD) q15_t detection_buffer[2][BLOCK_LEN];
PLACE_IN_DTCM float32_t fft_input_f32[DETECTION_FFT_SIZE];
PLACE_IN_DTCM float32_t fft_output_f32[DETECTION_FFT_SIZE * 2];
PLACE_IN_DTCM float32_t magnitude_output_f32[DETECTION_FFT_SIZE / 2];


PLACE_IN_DTCM q15_t fft_output_q15[DETECTION_FFT_SIZE * 2];
PLACE_IN_DTCM q15_t magnitude_output_q15[DETECTION_FFT_SIZE / 2];
PLACE_IN_DTCM float32_t SNR = 1;

PLACE_IN_DTCM volatile uint8_t mdma_half = 0;
PLACE_IN_DTCM volatile bool mdma_done_flag = false;

PLACE_IN_DTCM arm_rfft_instance_q15 processing_fft_instance;
PLACE_IN_DTCM arm_rfft_instance_q15 processing_ifft_instance;

static PLACE_IN_DTCM float32_t processing_workspace[N_HYDROPHONES][PROCESSING_FFT_SIZE];

static PLACE_IN_DTCM float32_t envelope[PROCESSING_FFT_SIZE];
static PLACE_IN_DTCM float32_t envelope_edge[PROCESSING_FFT_SIZE];

static PLACE_IN_DTCM uint16_t idxs[N_HYDROPHONES] = {0};
static PLACE_IN_DTCM float32_t times_of_arrival[N_HYDROPHONES] = {0};
static PLACE_IN_DTCM float32_t direction_of_arrival[3] = {0};

PLACE_IN_D3_SRAM uint8_t hydrophone_valid[N_HYDROPHONES] = {0};
static PLACE_IN_DTCM float32_t hydrophone_positions[N_HYDROPHONES][3] = {
		{0.0,0.0,0.0},
		{0.0,0.0,0.0},
		{0.5,0.0,0.0},
		{0.25,0.25,0.354},
		{0.25,-0.25,0.354}
};

static PLACE_IN_DTCM uint16_t max_idx_difference;

volatile PLACE_IN_DTCM bool dump_trigger = false;
volatile PLACE_IN_DTCM bool send_magnitude = false;
volatile PLACE_IN_DTCM bool verbose = false;

volatile PLACE_IN_DTCM uint8_t detected = false;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
void PeriphCommonClock_Config(void);
static void MPU_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_MDMA_Init(void);
static void MX_BDMA_Init(void);
static void MX_FDCAN1_Init(void);
static void MX_SPI1_Init(void);
static void MX_SPI2_Init(void);
static void MX_SPI3_Init(void);
static void MX_SPI4_Init(void);
static void MX_SPI5_Init(void);
static void MX_SPI6_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_CRC_Init(void);
static void MX_TIM1_Init(void);
static void MX_RTC_Init(void);
static void MX_ADC3_Init(void);
static void MX_TIM6_Init(void);
/* USER CODE BEGIN PFP */
static void init_adc_and_buffers();
uint32_t threshold_binary_search(float32_t* signal, uint32_t signal_len, float32_t threshold);
uint32_t threshold_search(float32_t* signal, uint32_t signal_len, float32_t threshold, const uint32_t patience);
float32_t min_max_threshold(float32_t* signal, uint32_t signal_len, float32_t threshold, uint32_t n_high, uint32_t n_low);
void threshold_applier(float32_t* signal, uint32_t signal_len, float32_t threshold);
void spike_filter(float32_t *signal, uint32_t signal_len, float32_t threshold);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

#define MAX_TROUGHS WORKSPACE_LEN

uint32_t find_da_edge(const float32_t *signal, uint32_t signal_len)
{

	float32_t buf_min;
	uint32_t min_idx;
	arm_min_f32(signal, signal_len, &buf_min, &min_idx);
	float32_t min_depth = buf_min*0.1f; //deepest trough

	float32_t min_prominence;
	arm_std_f32(signal, signal_len, &min_prominence);
	min_prominence *= 0.01;

    uint32_t           trough_idxs[MAX_TROUGHS];
    find_peaks_props_f32_t props[MAX_TROUGHS];
    uint32_t           n_troughs;

    find_peaks_config_f32_t cfg = FIND_PEAKS_CONFIG_F32_DEFAULT;
    cfg.height     = min_depth;
    cfg.prominence = min_prominence;
    cfg.distance   = 5;

    if(find_troughs_f32(signal, signal_len, &cfg, trough_idxs, props, MAX_TROUGHS, &n_troughs) != FIND_PEAKS_OK){
    	Error_Handler();
    }


    // 4. first_peak = np.min(find_peaks_data) → lowest index found
    //    peak_idx is already in ascending index order, so index 0 is the first
    uint32_t first_trough;
    if (n_troughs > 0) {
        first_trough = trough_idxs[0];          // leftmost peak (min index)
    } else {
        arm_min_f32(signal, signal_len, &buf_min, &first_trough);  // fallback: argmin
    }
    return first_trough;
}



void print_binary(uint16_t value, uint8_t bits) {
    for (int i = bits - 1; i >= 0; i--) {
        printf("%c", (value >> i) & 1 ? '1' : '0');
    }
}

void read_all_registers_binary(void){
	for(int i = 0; i < 44; i++){
		printf("Register %#04x:\t",my_ADC.registers->all[i].address);
		uint8_t data = ad7606_read_register(&my_ADC, my_ADC.registers->all[i]);
		print_binary(data,8);
		printf("\r\n");
	}
}

void SWO_Init(void)
{
    /* Enable ITM and DWT */
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;

    /* ITM unlock */
    ITM->LAR = 0xC5ACCE55;

    /* Enable ITM port 0 */
    ITM->TCR |= ITM_TCR_ITMENA_Msk;
    ITM->TER |= (1UL << 0);  // Enable stimulus port 0
}

void MyMDMA_TransferCompleteCallback(MDMA_HandleTypeDef *hmdma) {
    // Handle transfer complete
    mdma_done_flag = 1;
    mdma_half = (mdma_half) ? 0 : 1;
}

void MyMDMA_ErrorCallback(MDMA_HandleTypeDef *hmdma) {
    // Handle error
	Error_Handler();
}



bool signal_present(uint8_t half_idx) {

    arm_q15_to_float(detection_buffer[half_idx], fft_input_f32, DETECTION_FFT_SIZE);
    arm_rfft_fast_f32(&detection_fft_instance_f32, fft_input_f32, fft_output_f32, 0);
    arm_cmplx_mag_squared_f32(fft_output_f32, magnitude_output_f32, DETECTION_FFT_SIZE/2);

    const uint32_t SIGNAL_BIN_LOW = 14;
    const uint32_t SIGNAL_BIN_HIGH = 17;
    const uint32_t SIGNAL_BIN_COUNT = SIGNAL_BIN_HIGH-SIGNAL_BIN_LOW;
    const uint32_t NOISE_BIN_COUNT = DETECTION_FFT_SIZE/2 - 1 - SIGNAL_BIN_COUNT;

    float32_t noise_power = 0.0f;
    for (int i = 1; i < SIGNAL_BIN_LOW; i++)
        noise_power += magnitude_output_f32[i];
    for (int i = SIGNAL_BIN_HIGH; i < DETECTION_FFT_SIZE / 2; i++)
        noise_power += magnitude_output_f32[i];

    bool present = false;
    if (unlikely(noise_power <= 0.0f)){
    	goto NO_SIGNAL_PRESENT;
    }

    float32_t signal_power = magnitude_output_f32[14] + magnitude_output_f32[15] + magnitude_output_f32[16];
    if (likely(signal_power < SIGNAL_MIN_POWER)){
    	goto NO_SIGNAL_PRESENT;
    }

    // signal_power/2 > (noise_power/NOISE_BIN_COUNT) * LINEAR_THRESHOLD
    present = (signal_power * NOISE_BIN_COUNT) > (noise_power * SIGNAL_BIN_COUNT * LINEAR_THRESHOLD);
    if(unlikely(present && !detected)){
    	detected = DETECTION_PATIENCE;
    	return true;
    }

    NO_SIGNAL_PRESENT:
		switch(detected){
			case(0):
				detected = false;
			break;
			default:
				detected--;
			break;
		}
		return false;
}

bool signal_present_q15(uint8_t half_idx) {

    arm_rfft_q15(&detection_fft_instance_q15, detection_buffer[half_idx], fft_output_q15);
    dsp_fill_headroom_q15(fft_output_q15, DETECTION_FFT_SIZE);
    arm_cmplx_mag_fast_q15(fft_output_q15, magnitude_output_q15, DETECTION_FFT_SIZE/2);

    const uint32_t SIGNAL_BIN_LOW = 14;
    const uint32_t SIGNAL_BIN_HIGH = 17;
    const uint32_t SIGNAL_BIN_COUNT = SIGNAL_BIN_HIGH-SIGNAL_BIN_LOW;
    const uint32_t NOISE_BIN_COUNT = DETECTION_FFT_SIZE/2 - 1 - SIGNAL_BIN_COUNT;

    uint32_t noise_power = 0;
    for (int i = 1; i < SIGNAL_BIN_LOW; i++)
        noise_power += magnitude_output_q15[i];
    for (int i = SIGNAL_BIN_HIGH; i < DETECTION_FFT_SIZE / 2; i++)
        noise_power += magnitude_output_q15[i];

    bool present = false;

    uint32_t signal_power = magnitude_output_q15[14] + magnitude_output_q15[15] + magnitude_output_q15[16];

    // signal_power/2 > (noise_power/NOISE_BIN_COUNT) * LINEAR_THRESHOLD
    present = (signal_power * NOISE_BIN_COUNT) > (noise_power * SIGNAL_BIN_COUNT * LINEAR_THRESHOLD);
    if(unlikely(present && !detected)){
    	detected = DETECTION_PATIENCE;
    	return true;
    }

	switch(detected){
		case(0):
			detected = false;
		break;
		default:
			detected--;
		break;
	}
	return false;
}

bool signal_present_f32(float32_t* signal){

	arm_copy_f32(signal, fft_input_f32, DETECTION_FFT_SIZE);
    arm_rfft_fast_f32(&detection_fft_instance_f32, fft_input_f32, fft_output_f32, 0);
    arm_cmplx_mag_f32(fft_output_f32, magnitude_output_f32, DETECTION_FFT_SIZE/2);

    const uint32_t SIGNAL_BIN_LOW = 14;
    const uint32_t SIGNAL_BIN_HIGH = 17;
    const uint32_t SIGNAL_BIN_COUNT = SIGNAL_BIN_HIGH-SIGNAL_BIN_LOW;
    const uint32_t NOISE_BIN_COUNT = DETECTION_FFT_SIZE/2 - 1 - SIGNAL_BIN_COUNT;

    float32_t noise_power = 0.0f;
    for (int i = 1; i < SIGNAL_BIN_LOW; i++)
        noise_power += magnitude_output_f32[i];
    for (int i = SIGNAL_BIN_HIGH; i < DETECTION_FFT_SIZE / 2; i++)
        noise_power += magnitude_output_f32[i];

    if (unlikely(noise_power <= 0.0f)){
    	return false;
    }

    float32_t signal_power = magnitude_output_f32[14] + magnitude_output_f32[15] + magnitude_output_f32[16];
    if (likely(signal_power < SIGNAL_MIN_POWER)){
    	return false;
    }

    // signal_power/2 > (noise_power/NOISE_BIN_COUNT) * LINEAR_THRESHOLD
    return (signal_power * NOISE_BIN_COUNT) > (noise_power * SIGNAL_BIN_COUNT * LINEAR_THRESHOLD);
}


// claude generated
// Add this to the top of your file
#define DWT_CYCCNT  (*((volatile uint32_t *)0xE0001004))
#define DWT_CTRL    (*((volatile uint32_t *)0xE0001000))
#define DEM_CR      (*((volatile uint32_t *)0xE000EDFC))

// Enable DWT cycle counter (do this once in init)
void DWT_Init(void) {
    DEM_CR    |= (1 << 24);  // Enable TRC
    DWT_CYCCNT = 0;
    DWT_CTRL  |= (1 << 0);   // Enable CYCCNT
}

// claude generated
int32_t f32_whole(float32_t x) {
    return (int32_t)x;
}

int32_t f32_frac(float32_t x, uint8_t decimals) {
    int32_t whole = (int32_t)x;
    float32_t frac = x - (float32_t)whole;
    if (frac < 0) frac = -frac;
    uint32_t scale = 1;
    for (uint8_t i = 0; i < decimals; i++) scale *= 10;
    return (int32_t)(frac * scale);
}

void dump_python_array_q15(q15_t* arr, int len) {
    if (arr == NULL || len <= 0) return;

    printf("[");
    for (int i = 0; i < len - 1; i++) {
        printf("%d,", arr[i]);
    }
    printf("%d]", arr[len - 1]);
}

void dump_python_array_f32(float32_t* arr, int len) {
    if (arr == NULL || len <= 0) return;

    printf("[");
    for (int i = 0; i < len; i++) {
        if (arr[i] < 0.0f && f32_whole(arr[i]) == 0)
            printf("-");
        printf("%ld.%06ld", f32_whole(arr[i]), f32_frac(arr[i], 6));
        if (i < len - 1) printf(",");
    }
    printf("]");
}

#define DUMP_ARRAY_NAMED_DICT_Q15(name, arr, len) do { \
    printf("\t\"" name "\" : ");                        \
    fflush(stdout);                                     \
    dump_python_array_q15((q15_t*)(arr), (len));        \
    printf("\r\n");                                     \
} while(0)

#define DUMP_ARRAY_NAMED_DICT_F32(name, arr, len) do { \
    printf("\t\"" name "\" : ");                        \
    fflush(stdout);                                     \
    dump_python_array_f32((float32_t*)(arr), (len));    \
    printf("\r\n");                                     \
} while(0)

#define DUMP_ARRAY_NAMED_Q15(name, arr, len) do { \
    printf(name " = ");                            \
    fflush(stdout);                                \
    dump_python_array_q15((q15_t*)(arr), (len));   \
    printf("\r\n");                                \
} while(0)

#define DUMP_ARRAY_NAMED_F32(name, arr, len) do { \
    printf(name " = ");                            \
    fflush(stdout);                                \
    dump_python_array_f32((float32_t*)(arr), (len)); \
    printf("\r\n");                                \
} while(0)

void clear_buffer_q15(q15_t* arr, int len){
	for(int i = 0; i < len; i++) arr[i] = 0;
}

void clear_buffer_f32(float32_t* arr, int len){
	for(int i = 0; i < len; i++) arr[i] = 0;
}

/**
 * @brief Unwraps a circular Q15 buffer into a linear float32 array.
 *
 * @param src        Pointer to the circular Q15 buffer
 * @param dst        Pointer to the output float32 array (must be at least data_len long)
 * @param data_len   Number of samples to copy and convert
 * @param buffer_len Total length of the circular buffer
 * @param start_idx  Index of the oldest sample (read head)
 */
void circ_unwrap_to_f32(q15_t *src, float32_t *dst, uint32_t data_len,
                         uint32_t buffer_len, uint32_t start_idx)
{
    /* How many samples from start_idx to the end of the buffer */
    uint32_t chunk1 = buffer_len - start_idx;

    if (chunk1 >= data_len) {
        /* No wrap-around: all data sits in one contiguous block */
        arm_q15_to_float(src + start_idx, dst, data_len);
    } else {
        /* Two chunks: tail of buffer, then beginning of buffer */
        uint32_t chunk2 = data_len - chunk1;
        arm_q15_to_float(src + start_idx, dst,          chunk1);
        arm_q15_to_float(src,             dst + chunk1, chunk2);
    }
}

void CAN_send_direction(FDCAN_HandleTypeDef *hfdcan, uint32_t id, float32_t vec[3], float32_t weight)
{
    FDCAN_TxHeaderTypeDef txHeader;
    uint8_t txData[16];  // 3 x float32 = 12 bytes

    memcpy(txData, vec, 12);
    memcpy(txData + 12, &weight, 4);

    txHeader.Identifier          = id;
    txHeader.IdType              = FDCAN_STANDARD_ID;
    txHeader.TxFrameType         = FDCAN_DATA_FRAME;
    txHeader.DataLength          = FDCAN_DLC_BYTES_16;
    txHeader.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
    txHeader.BitRateSwitch       = FDCAN_BRS_ON;
    txHeader.FDFormat            = FDCAN_FD_CAN;
    txHeader.TxEventFifoControl  = FDCAN_NO_TX_EVENTS;
    txHeader.MessageMarker       = 0;

    while (HAL_FDCAN_GetTxFifoFreeLevel(hfdcan) == 0);

    HAL_FDCAN_AddMessageToTxFifoQ(hfdcan, &txHeader, txData);
}

void UART_send_direction_verbose(UART_HandleTypeDef *huart, float32_t vec[3], float32_t weight)
{
    char buf[128];
    int len = 0;

    // Helper to append each component
    for (int i = 0; i < 3; i++)
    {
        int32_t whole = f32_whole(vec[i]);
        int32_t frac  = f32_frac(vec[i], 4);

        if (i == 0)
            len += snprintf(buf + len, sizeof(buf) - len, "[");

        if (vec[i] < 0.0f && whole == 0)
            len += snprintf(buf + len, sizeof(buf) - len, "-0.%04ld", frac);
        else
            len += snprintf(buf + len, sizeof(buf) - len, "%ld.%04ld", whole, frac);

        if (i < 2)
            len += snprintf(buf + len, sizeof(buf) - len, ", ");
        else
            len += snprintf(buf + len, sizeof(buf) - len, "],");
    }

    int32_t whole = f32_whole(weight);
    int32_t frac  = f32_frac(weight, 4);

    len += snprintf(buf + len, sizeof(buf) - len, "%ld.%04ld\r\n",whole, frac);

    HAL_UART_Transmit(huart, (uint8_t*)buf, len, 100);
}

void UART_send_direction(UART_HandleTypeDef *huart, float32_t vec[3], float32_t weight)
{
    uint8_t buf[16];

    memcpy(buf, vec, 12);
    memcpy(buf + 12, &weight, 4);

    HAL_UART_Transmit(huart, (uint8_t*)buf, sizeof(buf), 100);
}

__attribute__((used, noinline)) void dump_magnitude(void){
	magnitude_output_f32[0] = 0;
	DUMP_ARRAY_NAMED_F32("magnitude",magnitude_output_f32,DETECTION_FFT_SIZE/2);
}

__attribute__((used, noinline)) void dump_everything(uint16_t workspace_idx, uint8_t valid){
	printf("dump = {\r\n");

	magnitude_output_f32[0] = 0;
	DUMP_ARRAY_NAMED_DICT_F32("magnitude",magnitude_output_f32,DETECTION_FFT_SIZE/2);
	printf(",");

	printf("\t\"raw_mv\" : [\r\n\t");
	for(int i = 0; i < N_HYDROPHONES; i++){
		q15_t* buffer_flat = (q15_t*)hydrophone_buffers[i];
		circ_unwrap_to_f32(buffer_flat ,processing_workspace[i], WORKSPACE_LEN, BUFFER_LEN, (uint32_t)workspace_idx);
		float32_t scalar = 32768.0*ad7606_channel_scaling_factor(&my_ADC, i)*1000.0;
		arm_scale_f32(processing_workspace[i],scalar,processing_workspace[i],WORKSPACE_LEN);
			dump_python_array_f32(processing_workspace[i], WORKSPACE_LEN);
			if(i == N_HYDROPHONES - 1){
				printf("\r\n],\r\n");
			}else{
				printf(",\r\n\t");
			}
	    //normalize
	    scalar = 0;
	    arm_mean_f32(processing_workspace[i], WORKSPACE_LEN, &scalar);   // Step 1: compute mean
	    arm_offset_f32(processing_workspace[i],-scalar,processing_workspace[i],WORKSPACE_LEN); // Step 2: subtract it
	    scalar = 0;
		arm_rms_f32(processing_workspace[i], WORKSPACE_LEN, &scalar);
		scalar = 1/scalar;
		arm_scale_f32(processing_workspace[i],scalar,processing_workspace[i],WORKSPACE_LEN);
		spike_filter(processing_workspace[i],WORKSPACE_LEN,10);
	}

	printf("\"normalized\" : [\r\n\t");
	for(int i = 0; i < N_HYDROPHONES; i++){
		dump_python_array_f32(processing_workspace[i], WORKSPACE_LEN);
		if(i == N_HYDROPHONES - 1){
			printf("\r\n],\r\n");
		}else{
			printf(",\r\n\t");
		}
	}

	printf("\"cwt\" : [\r\n\t");
	for(int i = 0; i < N_HYDROPHONES; i++){
		cwt_morlet_magnitude_f32(processing_workspace[i], envelope);
		dump_python_array_f32(envelope, WORKSPACE_LEN);
		if(i == N_HYDROPHONES - 1){
			printf("\r\n],\r\n");
		}else{
			printf(",\r\n\t");
		}
	}


	float32_t thresholds[5];

	printf("\"thresholded\" : [\r\n\t");
	for(int i = 0; i < N_HYDROPHONES; i++){
		cwt_morlet_magnitude_f32(processing_workspace[i], envelope);

		float32_t linear_threshold = 0.01;
		uint8_t max_retries = 6;
		uint32_t dead_space = WORKSPACE_LEN/4;
		idxs[i] = 0;
		float32_t threshold;
		while(idxs[i] < dead_space && max_retries--){
			threshold = min_max_threshold(envelope, WORKSPACE_LEN, linear_threshold, 15, dead_space);
			idxs[i] = threshold_search(envelope,WORKSPACE_LEN, threshold , 3);
			linear_threshold *= 2;
		}

		thresholds[i] = threshold;
		threshold_applier(envelope, WORKSPACE_LEN, threshold);
		dump_python_array_f32(envelope, WORKSPACE_LEN);
		if(i == N_HYDROPHONES - 1){
			printf("\r\n],\r\n");
		}else{
			printf(",\r\n\t");
		}
	}

	printf("\"envelope_edge\" : [\r\n\t");
	for(int i = 0; i < N_HYDROPHONES; i++){
		cwt_morlet_magnitude_f32(processing_workspace[i], envelope);
		hilbert_imag_f32(envelope,envelope_edge);
		dump_python_array_f32(envelope_edge, PROCESSING_FFT_SIZE);
		if(i == N_HYDROPHONES - 1){
			printf("\r\n],\r\n");
		}else{
			printf(",\r\n\t");
		}
	}

	for(int i = 0; i < N_HYDROPHONES; i++){
		cwt_morlet_magnitude_f32(processing_workspace[i], envelope);

		float32_t linear_threshold = 0.01;
		uint8_t max_retries = 6;
		uint32_t dead_space = WORKSPACE_LEN/4;
		idxs[i] = 0;
		while(idxs[i] < dead_space && max_retries--){
			float32_t threshold = min_max_threshold(envelope, WORKSPACE_LEN, linear_threshold, 15, dead_space);
			idxs[i] = threshold_search(envelope,WORKSPACE_LEN, threshold , 3);
			linear_threshold *= 2;
		}
		times_of_arrival[i] = (float32_t)idxs[i];//*1/SAMPLING_FREQUENCY;
	}

	DUMP_ARRAY_NAMED_DICT_Q15("idxs", (q15_t*)idxs, N_HYDROPHONES);
	printf(",");



	int32_t tdoa_status = 0;
	tdoa_status = TDOA_direction_solve_f32(hydrophone_positions,
										  times_of_arrival,
										  hydrophone_valid,
										  N_HYDROPHONES,
										  direction_of_arrival);

	printf("\"hydrophone_pos\" : [\r\n\t");
	for(int i = 0; i < N_HYDROPHONES; i++){
		dump_python_array_f32(hydrophone_positions[i], 3);
		if(i == N_HYDROPHONES - 1){
			printf("\r\n],\r\n");
		}else{
			printf(",\r\n\t");
		}
	}

	DUMP_ARRAY_NAMED_DICT_F32("thresholds",thresholds, 5);
	printf(",");
	DUMP_ARRAY_NAMED_DICT_F32("direction_of_arrival",direction_of_arrival, 3);
	printf(",");
	printf("\t\"valid\" : %d",valid);

	fflush(stdout);
	printf("}\r\n");
}

float32_t estimate_SNR(void){
    const uint32_t SIGNAL_BIN_LOW = 14;
    const uint32_t SIGNAL_BIN_HIGH = 17;
    const uint32_t SIGNAL_BIN_COUNT = SIGNAL_BIN_HIGH-SIGNAL_BIN_LOW;
    const uint32_t NOISE_BIN_COUNT = DETECTION_FFT_SIZE/2 - 1 - SIGNAL_BIN_COUNT;

    float32_t noise_power = 0.0f;
    for (int i = 1; i < SIGNAL_BIN_LOW; i++)
        noise_power += magnitude_output_f32[i];
    for (int i = SIGNAL_BIN_HIGH; i < DETECTION_FFT_SIZE / 2; i++)
        noise_power += magnitude_output_f32[i];

    float32_t signal_power = magnitude_output_f32[14] + magnitude_output_f32[15] + magnitude_output_f32[16];

    return (signal_power * NOISE_BIN_COUNT) / (noise_power * SIGNAL_BIN_COUNT);

}

void restart_buffers_and_spi(void){
	int lengths[8] = {
			BUFFER_LEN,
			BUFFER_LEN,
			BUFFER_LEN,
			BUFFER_LEN,
			BUFFER_LEN,
			0,
			0,
			0,
	};
	int16_t* buffers[8] = {NULL};
	for(int i = 0; i < 5; i++) buffers[i] = (int16_t*)&hydrophone_buffers[i][0][0];

	ad7606_enter_adc_mode(&my_ADC);

	ad7606_init_output_buffers_DMA(&my_ADC, buffers, lengths);
	HAL_MDMA_RegisterCallback(&hmdma_mdma_channel0_sw_0, HAL_MDMA_XFER_CPLT_CB_ID,  MyMDMA_TransferCompleteCallback);
	HAL_MDMA_RegisterCallback(&hmdma_mdma_channel0_sw_0, HAL_MDMA_XFER_ERROR_CB_ID, MyMDMA_ErrorCallback);
	ad7606_dma_spi_init(&my_ADC, &hdma_spi6_rx, diagnostics_buffer, BLOCK_LEN);
}

float32_t abs_f32(float32_t x){
	if(x >= 0){
		return x;
	}else{
		return -x;
	}
}

int32_t abs_int32(int32_t x){
	if(x >= 0){
		return x;
	}else{
		return -x;
	}
}

bool is_along_axis(float32_t vec[3]){
	float32_t sum = 0;
	for(int i = 0; i < 3; i++) sum += abs_f32(vec[i]);
	return sum <= 1;
}

bool is_too_long(float32_t vec[3]){
	float32_t sum = 0;
	for(int i = 0; i < 3; i++) sum += abs_f32(vec[i]);
	return sum > 1.732050807569f;
}

bool is_valid(float32_t vec[3]){
	if(is_along_axis(vec)) return false;

	if(is_too_long(vec)) return false;

	return true;
}

uint32_t threshold_binary_search(float32_t* signal, uint32_t signal_len, float32_t threshold){
    int lo = 0, hi = signal_len - 1, result = -1;
    while (lo <= hi) {
        int mid = lo + (hi - lo) / 2;
        if (signal[mid] <= threshold) {
            result = mid;
            lo = mid + 1;
        } else {
            hi = mid - 1;
        }
    }
    return result;
}

uint32_t threshold_search(float32_t* signal, uint32_t signal_len, float32_t threshold, const uint32_t patience)
{
    uint32_t i = signal_len;
    uint32_t remaining_patience = patience;

    while (i--)
    {
        if (signal[i] < threshold){
        	remaining_patience--;
        }else{
        	remaining_patience = patience;
        }
        if(remaining_patience == 0){
        	return i + patience;
        }
    }

    return 0;
}

float32_t min_max_threshold(float32_t* signal, uint32_t signal_len, float32_t threshold, uint32_t n_high, uint32_t n_low)
{
    /*
     * Finds the average of the bottom N and top N points in the array,
     * then returns an interpolated threshold between those two averages.
     *
     * threshold = 0.0 -> returns the low average
     * threshold = 1.0 -> returns the high average
     * threshold = 0.5 -> returns the midpoint between them
     */

    /* --- Sort a copy of the signal using an in-place insertion sort ---
     * For large arrays consider a faster algorithm, but insertion sort
     * has zero heap allocation and is fine for typical DSP frame sizes. */
    float32_t sorted[signal_len];
    arm_copy_f32(signal, sorted, signal_len);

    /* Insertion sort (ascending) */
    for (uint32_t i = 1; i < signal_len; i++)
    {
        float32_t key = sorted[i];
        int32_t j = (int32_t)i - 1;
        while (j >= 0 && sorted[j] > key)
        {
            sorted[j + 1] = sorted[j];
            j--;
        }
        sorted[j + 1] = key;
    }

    /* --- Average the N lowest values --- */
    float32_t low_mean = 0.0f;
    arm_mean_f32(sorted, n_low, &low_mean);

    /* --- Average the N highest values --- */
    float32_t high_mean = 0.0f;
    arm_mean_f32(&sorted[signal_len - n_high], n_high, &high_mean);

    /* --- Interpolate between the two averages --- */
    /* result = low + threshold * (high - low) */
    float32_t result = 0.0f;
    arm_add_f32(                          /* low + t*(high-low)          */
        &low_mean,                        /* not a vector call, so we    */
        &(float32_t){threshold *          /* use scalar arithmetic below */
            (high_mean - low_mean)},
        &result, 1);

    /* Simpler and equally valid on Cortex-M7 with FPU: */
    result = low_mean + threshold * (high_mean - low_mean);

    return result;
}

void threshold_applier(float32_t* signal, uint32_t signal_len, float32_t threshold){
	for(int i = 0; i < signal_len; i++){
		signal[i] = 100*(signal[i] > threshold);
	}
}

void spike_filter(float32_t *signal, uint32_t signal_len, float32_t threshold){
	const uint8_t window_radius = 3; //left and right distance
	for(int i = window_radius; i < signal_len - window_radius; i++){
		float32_t sum = 0;
		for(int j = -window_radius; j < window_radius + 1; j++){
			if(j != 0){
				sum += abs_f32(signal[i+j]);
			}
		}
		sum /= window_radius*2;
		if(abs_f32(signal[i]) > sum*threshold){
			signal[i] = sum;
		}
	}
}

float32_t distance_3d(float32_t vec1[3], float32_t vec2[3])
{
    float32_t diff[3];
    float32_t dot;
    float32_t result;

    arm_sub_f32(vec1, vec2, diff, 3);
    arm_dot_prod_f32(diff, diff, 3, &dot);
    arm_sqrt_f32(dot, &result);

    return result;
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MPU Configuration--------------------------------------------------------*/
  MPU_Config();

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */
  SWO_Init();
  dump_trigger = true;
  send_magnitude = false;
  verbose = false;
  SNR = 1;
  mdma_half = 0;
  mdma_done_flag = false;
  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* Configure the peripherals common clocks */
  PeriphCommonClock_Config();

  /* USER CODE BEGIN SysInit */
  //utils_DWT_init();
  DWT_Init();

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_MDMA_Init();
  MX_BDMA_Init();
  MX_FDCAN1_Init();
  MX_SPI1_Init();
  MX_SPI2_Init();
  MX_SPI3_Init();
  MX_SPI4_Init();
  MX_SPI5_Init();
  MX_SPI6_Init();
  MX_USART1_UART_Init();
  MX_CRC_Init();
  MX_TIM1_Init();
  MX_RTC_Init();
  MX_ADC3_Init();
  MX_TIM6_Init();
  /* USER CODE BEGIN 2 */
  //MDMA_UserInit();
	uint8_t msg[] = "USART1 OK\r\n";
	HAL_UART_Transmit(&huart1, msg, sizeof(msg) - 1, 100);
	HAL_GPIO_WritePin(GREEN_LED, GPIO_PIN_SET);

	stm_temp_sensor_init(&hadc3, &htim6);

	HAL_FDCAN_Start(&hfdcan1);
	FDCAN_TxHeaderTypeDef txHeader;
	uint8_t txData[12] = {0x4F, 0x4B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}; // "OK"

	txHeader.Identifier          = 0x123;
	txHeader.IdType              = FDCAN_STANDARD_ID;
	txHeader.TxFrameType         = FDCAN_DATA_FRAME;
	txHeader.DataLength          = FDCAN_DLC_BYTES_8;
	txHeader.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
	txHeader.BitRateSwitch       = FDCAN_BRS_OFF;
	txHeader.FDFormat            = FDCAN_CLASSIC_CAN;
	txHeader.TxEventFifoControl  = FDCAN_NO_TX_EVENTS;
	txHeader.MessageMarker       = 0;

	if (HAL_FDCAN_AddMessageToTxFifoQ(&hfdcan1, &txHeader, txData) != HAL_OK)
	{
	    Error_Handler();
	}

//	float32_t hydrophone_positions_temp[N_HYDROPHONES][3] = {
//			{0.0,0.0,0.0},
//			{0.0,0.0,0.0},
//			{0.5,0.0,0.0},
//			{0.25,0.25,0.354},
//			{0.25,-0.25,0.354}
//	};

	float32_t hydrophone_positions_temp[N_HYDROPHONES][3] = {
			{0.0,0.0,0.0},
			{0.0,0.0,0.0},
			{32.3,-4.3,3.66},
			{-6.6,34.7,1.66},
			{-4.6,0.0,-34.34}
	};

	for(int i = 0; i < N_HYDROPHONES; i++){
		hydrophone_valid[i] = false;
		hydrophone_positions[i][0] = hydrophone_positions_temp[i][0];
		hydrophone_positions[i][1] = hydrophone_positions_temp[i][1];
		hydrophone_positions[i][2] = hydrophone_positions_temp[i][2];
	}

	float32_t biggest_distance = 0;
	for(int i = 1; i < N_HYDROPHONES; i++){
		float32_t dist = distance_3d(hydrophone_positions[0],hydrophone_positions[i]);
		if(dist > biggest_distance){
			biggest_distance = dist;
		}
	}
	{
		float32_t idx_distance = (biggest_distance/(WAVE_SPEED*100)*SAMPLING_FREQUENCY) + BLOCK_LEN/4;
		uint16_t n = (uint16_t)ceilf(idx_distance);
		n--;
		n |= n >> 1;
		n |= n >> 2;
		n |= n >> 4;
		n |= n >> 8;
		n |= n >> 16;
		n++;
		max_idx_difference = n;
	}



	init_adc_and_buffers();
	arm_rfft_fast_init_f32(&detection_fft_instance_f32, DETECTION_FFT_SIZE);
	arm_rfft_init_q15(&detection_fft_instance_q15, DETECTION_FFT_SIZE, 0, 1);
	cwt_init_f32(TARGET_FREQUENCY, SAMPLING_FREQUENCY, 0.5);
	hilbert_init_f32();
	direction_of_arrival[0] = 1.0;
	direction_of_arrival[1] = 0.0;
	direction_of_arrival[2] = 0.0;

	if(!dump_trigger){
		printf("dump = [\r\n\t");
	}

	HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_4);


  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
	utils_delay(1000000);
	HAL_GPIO_WritePin(YELLOW_LED, GPIO_PIN_SET);

    while (1) {
		uint8_t target_block = fast_get_detection_block_pos();
		uint8_t processing_half = !mdma_half;
		fast_MDMA_copy_block(hydrophone_buffers[0][target_block], detection_buffer[mdma_half], &hmdma_mdma_channel0_sw_0);
		//process data ...

		diagnostics_temp = (float32_t)ad7606_voltage_to_temp(ad7606_reading_to_voltage(&my_ADC,7,diagnostics_sample));
		stm32_temp = stm_temp_get_latest();

		__NOP();

		if(unlikely(send_magnitude)){
			dump_magnitude();
		}

    	if(unlikely(signal_present(processing_half))){ // processing_half
			HAL_TIM_PWM_Stop(&htim1, TIM_CHANNEL_4);
			HAL_GPIO_WritePin(CS, GPIO_PIN_SET);
			HAL_GPIO_WritePin(GREEN_LED, GPIO_PIN_RESET);


			//printf("dump = {\r\n");

			magnitude_output_f32[0] = 0;
			//DUMP_ARRAY_NAMED_DICT_F32("magnitude",magnitude_output_f32,DETECTION_FFT_SIZE/2);
			//printf(",");

			if(!dump_trigger){
				dump_magnitude();
			}

			uint16_t workspace_idx = (target_block*BLOCK_LEN+BUFFER_LEN-(WORKSPACE_LEN-WORKSPACE_OFFSET*BLOCK_LEN))%BUFFER_LEN;

			//printf("\t\"raw_mv\" : [\r\n\t");
			for(int i = 0; i < N_HYDROPHONES; i++){
				q15_t* buffer_flat = (q15_t*)hydrophone_buffers[i];
				circ_unwrap_to_f32(buffer_flat ,processing_workspace[i], WORKSPACE_LEN, BUFFER_LEN, (uint32_t)workspace_idx);
	    	    //normalize
	    	    float32_t scalar = 0;
	    	    arm_mean_f32(processing_workspace[i], WORKSPACE_LEN, &scalar);   // Step 1: compute mean
	    	    arm_offset_f32(processing_workspace[i],-scalar,processing_workspace[i],WORKSPACE_LEN); // Step 2: subtract it
	    	    scalar = 0;
				arm_rms_f32(processing_workspace[i], WORKSPACE_LEN, &scalar);
				scalar = 1/scalar;
				arm_scale_f32(processing_workspace[i],scalar,processing_workspace[i],WORKSPACE_LEN);
				cwt_morlet_magnitude_f32(processing_workspace[i], processing_workspace[i]);
			}

			bool valid_result = false;
			uint8_t max_calculation_retries = 3;

			while(!valid_result && max_calculation_retries--){

				uint8_t valid_buffers = 0;
				uint8_t valid_buffers_array[N_HYDROPHONES] = {0};

				for(int i = 0; i < N_HYDROPHONES; i++){
					valid_buffers_array[i] = hydrophone_valid[i];
					uint8_t n_signal_present_blocks = 0;
					for(int j; j < WORKSPACE_LEN/BLOCK_LEN; j++){
						n_signal_present_blocks += signal_present_f32(&processing_workspace[i][j*BLOCK_LEN]);
					}
					bool valid = (n_signal_present_blocks < WORKSPACE_LEN/BLOCK_LEN);
					valid_buffers += valid;
					valid_buffers_array[i] &= valid;
				}
				bool valid_data = (valid_buffers >= MINIMUM_VALID_BUFFERS);

				float32_t linear_threshold = 0.01;
				uint8_t max_retries = 6;
				uint32_t dead_space = WORKSPACE_LEN/2;
				idxs[0] = 0;
				while(((idxs[0] < dead_space) || (idxs[0] > (WORKSPACE_LEN-(WORKSPACE_OFFSET-1)*BLOCK_LEN))) && max_retries--){
					float32_t threshold = min_max_threshold(processing_workspace[0], WORKSPACE_LEN, linear_threshold, 15, dead_space);
					idxs[0] = threshold_search(processing_workspace[0],WORKSPACE_LEN, threshold , PROCESSING_PATIENCE);
					linear_threshold *= 2;
				}
				times_of_arrival[0] = (float32_t)idxs[0];

				for(int i = 1; i < N_HYDROPHONES; i++){
					float32_t linear_threshold = 0.01;
					uint8_t max_retries = 6;
					uint32_t dead_space = WORKSPACE_LEN/2;
					idxs[i] = 0;
					while(((abs_int32(idxs[0] - idxs[i])) > max_idx_difference) && max_retries--){
						float32_t threshold = min_max_threshold(processing_workspace[i], WORKSPACE_LEN, linear_threshold, 15, dead_space);
						idxs[i] = threshold_search(processing_workspace[i],WORKSPACE_LEN, threshold , PROCESSING_PATIENCE);
						linear_threshold *= 2;
					}
					times_of_arrival[i] = (float32_t)idxs[i];
				}

				valid_buffers = 0;

				for(int i = 1; i < N_HYDROPHONES; i++){
					bool valid = (max_idx_difference > abs_int32(idxs[0] - idxs[i]));
					valid_buffers += valid;
					valid_buffers_array[i] &= valid;
				}

				bool valid_idxs = (valid_buffers >= (MINIMUM_VALID_BUFFERS - 1));

				int32_t tdoa_status = 0;
				if(valid_data){
					tdoa_status = TDOA_direction_solve_f32(hydrophone_positions,
														  times_of_arrival,
														  valid_buffers_array,
														  N_HYDROPHONES,
														  direction_of_arrival);
				}

				valid_result = valid_data && is_valid(direction_of_arrival) && valid_idxs && (tdoa_status == 0);
			}

			if(unlikely(dump_trigger)){
				dump_everything(workspace_idx, valid_result);
				__NOP();
			}

			if(valid_result){
				float32_t snr = estimate_SNR();
				CAN_send_direction(&hfdcan1, 0x200, direction_of_arrival, snr);
				UART_send_direction(&huart1, direction_of_arrival, snr);

				__NOP();

				utils_DWT_delay_ms(300);

				printf("{");
				dump_python_array_f32(direction_of_arrival, 3);
				printf(",");
				printf("%ld.%06ld", f32_whole(snr), f32_frac(snr, 6));
				printf("},\r\n\t");

				__NOP();
			}else{
				printf("invalid ping\r\n");
			}

			for(int i = 0; i < 5; i++){
				clear_buffer_f32(processing_workspace[i], WORKSPACE_LEN);
			}
			HAL_GPIO_WritePin(CS, GPIO_PIN_RESET);
			restart_buffers_and_spi();
			HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_4);
			HAL_GPIO_WritePin(GREEN_LED, GPIO_PIN_SET);
    	}
		  while(mdma_done_flag == false) {
			__NOP();
			  // Optionally, add a timeout here to avoid infinite blocking
		  }
		  mdma_done_flag = false;

    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    }
    while(1)__NOP();
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Supply configuration update enable
  */
  HAL_PWREx_ConfigSupply(PWR_LDO_SUPPLY);

  /** Configure the main internal regulator output voltage
  */
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE0);

  while(!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}

  /** Configure LSE Drive Capability
  */
  HAL_PWR_EnableBkUpAccess();
  __HAL_RCC_LSEDRIVE_CONFIG(RCC_LSEDRIVE_LOW);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE|RCC_OSCILLATORTYPE_LSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_BYPASS;
  RCC_OscInitStruct.LSEState = RCC_LSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 4;
  RCC_OscInitStruct.PLL.PLLN = 96;
  RCC_OscInitStruct.PLL.PLLP = 2;
  RCC_OscInitStruct.PLL.PLLQ = 5;
  RCC_OscInitStruct.PLL.PLLR = 2;
  RCC_OscInitStruct.PLL.PLLRGE = RCC_PLL1VCIRANGE_3;
  RCC_OscInitStruct.PLL.PLLVCOSEL = RCC_PLL1VCOWIDE;
  RCC_OscInitStruct.PLL.PLLFRACN = 0;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2
                              |RCC_CLOCKTYPE_D3PCLK1|RCC_CLOCKTYPE_D1PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.SYSCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB3CLKDivider = RCC_APB3_DIV2;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_APB2_DIV2;
  RCC_ClkInitStruct.APB4CLKDivider = RCC_APB4_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief Peripherals Common Clock Configuration
  * @retval None
  */
void PeriphCommonClock_Config(void)
{
  RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};

  /** Initializes the peripherals clock
  */
  PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_SPI6|RCC_PERIPHCLK_ADC
                              |RCC_PERIPHCLK_SPI3|RCC_PERIPHCLK_SPI2
                              |RCC_PERIPHCLK_SPI1|RCC_PERIPHCLK_SPI4
                              |RCC_PERIPHCLK_SPI5|RCC_PERIPHCLK_FDCAN;
  PeriphClkInitStruct.PLL2.PLL2M = 16;
  PeriphClkInitStruct.PLL2.PLL2N = 128;
  PeriphClkInitStruct.PLL2.PLL2P = 4;
  PeriphClkInitStruct.PLL2.PLL2Q = 4;
  PeriphClkInitStruct.PLL2.PLL2R = 2;
  PeriphClkInitStruct.PLL2.PLL2RGE = RCC_PLL2VCIRANGE_1;
  PeriphClkInitStruct.PLL2.PLL2VCOSEL = RCC_PLL2VCOMEDIUM;
  PeriphClkInitStruct.PLL2.PLL2FRACN = 0;
  PeriphClkInitStruct.Spi123ClockSelection = RCC_SPI123CLKSOURCE_PLL2;
  PeriphClkInitStruct.Spi45ClockSelection = RCC_SPI45CLKSOURCE_PLL2;
  PeriphClkInitStruct.FdcanClockSelection = RCC_FDCANCLKSOURCE_PLL2;
  PeriphClkInitStruct.AdcClockSelection = RCC_ADCCLKSOURCE_PLL2;
  PeriphClkInitStruct.Spi6ClockSelection = RCC_SPI6CLKSOURCE_PLL2;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief ADC3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC3_Init(void)
{

  /* USER CODE BEGIN ADC3_Init 0 */

  /* USER CODE END ADC3_Init 0 */

  ADC_AnalogWDGConfTypeDef AnalogWDGConfig = {0};
  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC3_Init 1 */

  /* USER CODE END ADC3_Init 1 */

  /** Common config
  */
  hadc3.Instance = ADC3;
  hadc3.Init.ClockPrescaler = ADC_CLOCK_ASYNC_DIV8;
  hadc3.Init.Resolution = ADC_RESOLUTION_16B;
  hadc3.Init.ScanConvMode = ADC_SCAN_DISABLE;
  hadc3.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  hadc3.Init.LowPowerAutoWait = DISABLE;
  hadc3.Init.ContinuousConvMode = DISABLE;
  hadc3.Init.NbrOfConversion = 1;
  hadc3.Init.DiscontinuousConvMode = DISABLE;
  hadc3.Init.ExternalTrigConv = ADC_EXTERNALTRIG_T6_TRGO;
  hadc3.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_RISING;
  hadc3.Init.ConversionDataManagement = ADC_CONVERSIONDATA_DMA_CIRCULAR;
  hadc3.Init.Overrun = ADC_OVR_DATA_PRESERVED;
  hadc3.Init.LeftBitShift = ADC_LEFTBITSHIFT_NONE;
  hadc3.Init.OversamplingMode = ENABLE;
  hadc3.Init.Oversampling.Ratio = 256;
  hadc3.Init.Oversampling.RightBitShift = ADC_RIGHTBITSHIFT_8;
  hadc3.Init.Oversampling.TriggeredMode = ADC_TRIGGEREDMODE_SINGLE_TRIGGER;
  hadc3.Init.Oversampling.OversamplingStopReset = ADC_REGOVERSAMPLING_CONTINUED_MODE;
  if (HAL_ADC_Init(&hadc3) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Analog WatchDog 1
  */
  AnalogWDGConfig.WatchdogNumber = ADC_ANALOGWATCHDOG_1;
  AnalogWDGConfig.WatchdogMode = ADC_ANALOGWATCHDOG_SINGLE_REG;
  AnalogWDGConfig.Channel = ADC_CHANNEL_TEMPSENSOR;
  AnalogWDGConfig.ITMode = ENABLE;
  AnalogWDGConfig.HighThreshold = 14477;
  AnalogWDGConfig.LowThreshold = 10900;
  if (HAL_ADC_AnalogWDGConfig(&hadc3, &AnalogWDGConfig) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_TEMPSENSOR;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLETIME_810CYCLES_5;
  sConfig.SingleDiff = ADC_SINGLE_ENDED;
  sConfig.OffsetNumber = ADC_OFFSET_NONE;
  sConfig.Offset = 0;
  sConfig.OffsetSignedSaturation = DISABLE;
  if (HAL_ADC_ConfigChannel(&hadc3, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC3_Init 2 */

  /* USER CODE END ADC3_Init 2 */

}

/**
  * @brief CRC Initialization Function
  * @param None
  * @retval None
  */
static void MX_CRC_Init(void)
{

  /* USER CODE BEGIN CRC_Init 0 */

  /* USER CODE END CRC_Init 0 */

  /* USER CODE BEGIN CRC_Init 1 */

  /* USER CODE END CRC_Init 1 */
  hcrc.Instance = CRC;
  hcrc.Init.DefaultPolynomialUse = DEFAULT_POLYNOMIAL_ENABLE;
  hcrc.Init.DefaultInitValueUse = DEFAULT_INIT_VALUE_ENABLE;
  hcrc.Init.InputDataInversionMode = CRC_INPUTDATA_INVERSION_NONE;
  hcrc.Init.OutputDataInversionMode = CRC_OUTPUTDATA_INVERSION_DISABLE;
  hcrc.InputDataFormat = CRC_INPUTDATA_FORMAT_BYTES;
  if (HAL_CRC_Init(&hcrc) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN CRC_Init 2 */

  /* USER CODE END CRC_Init 2 */

}

/**
  * @brief FDCAN1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_FDCAN1_Init(void)
{

  /* USER CODE BEGIN FDCAN1_Init 0 */

  /* USER CODE END FDCAN1_Init 0 */

  /* USER CODE BEGIN FDCAN1_Init 1 */

  /* USER CODE END FDCAN1_Init 1 */
  hfdcan1.Instance = FDCAN1;
  hfdcan1.Init.FrameFormat = FDCAN_FRAME_FD_BRS;
  hfdcan1.Init.Mode = FDCAN_MODE_NORMAL;
  hfdcan1.Init.AutoRetransmission = DISABLE;
  hfdcan1.Init.TransmitPause = DISABLE;
  hfdcan1.Init.ProtocolException = DISABLE;
  hfdcan1.Init.NominalPrescaler = 1;
  hfdcan1.Init.NominalSyncJumpWidth = 1;
  hfdcan1.Init.NominalTimeSeg1 = 131;
  hfdcan1.Init.NominalTimeSeg2 = 28;
  hfdcan1.Init.DataPrescaler = 1;
  hfdcan1.Init.DataSyncJumpWidth = 1;
  hfdcan1.Init.DataTimeSeg1 = 31;
  hfdcan1.Init.DataTimeSeg2 = 8;
  hfdcan1.Init.MessageRAMOffset = 0;
  hfdcan1.Init.StdFiltersNbr = 0;
  hfdcan1.Init.ExtFiltersNbr = 0;
  hfdcan1.Init.RxFifo0ElmtsNbr = 0;
  hfdcan1.Init.RxFifo0ElmtSize = FDCAN_DATA_BYTES_12;
  hfdcan1.Init.RxFifo1ElmtsNbr = 4;
  hfdcan1.Init.RxFifo1ElmtSize = FDCAN_DATA_BYTES_16;
  hfdcan1.Init.RxBuffersNbr = 4;
  hfdcan1.Init.RxBufferSize = FDCAN_DATA_BYTES_16;
  hfdcan1.Init.TxEventsNbr = 0;
  hfdcan1.Init.TxBuffersNbr = 1;
  hfdcan1.Init.TxFifoQueueElmtsNbr = 16;
  hfdcan1.Init.TxFifoQueueMode = FDCAN_TX_FIFO_OPERATION;
  hfdcan1.Init.TxElmtSize = FDCAN_DATA_BYTES_16;
  if (HAL_FDCAN_Init(&hfdcan1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN FDCAN1_Init 2 */
  FDCAN_FilterTypeDef filterConfig;
  filterConfig.IdType       = FDCAN_STANDARD_ID;
  filterConfig.FilterIndex  = 0;
  filterConfig.FilterType   = FDCAN_FILTER_MASK;
  filterConfig.FilterConfig = FDCAN_FILTER_TO_RXFIFO0;
  filterConfig.FilterID1    = 0x000;   // ID
  filterConfig.FilterID2    = 0x000;   // Mask: 0x000 = accept all

  HAL_FDCAN_ConfigFilter(&hfdcan1, &filterConfig);

  // Reject non-matching and remote frames (recommended)
  HAL_FDCAN_ConfigGlobalFilter(
      &hfdcan1,
      FDCAN_REJECT,         // Non-matching standard
      FDCAN_REJECT,         // Non-matching extended
      FDCAN_FILTER_REMOTE,  // Remote standard
      FDCAN_FILTER_REMOTE   // Remote extended
  );
  /* USER CODE END FDCAN1_Init 2 */

}

/**
  * @brief RTC Initialization Function
  * @param None
  * @retval None
  */
static void MX_RTC_Init(void)
{

  /* USER CODE BEGIN RTC_Init 0 */

  /* USER CODE END RTC_Init 0 */

  /* USER CODE BEGIN RTC_Init 1 */

  /* USER CODE END RTC_Init 1 */

  /** Initialize RTC Only
  */
  hrtc.Instance = RTC;
  hrtc.Init.HourFormat = RTC_HOURFORMAT_24;
  hrtc.Init.AsynchPrediv = 127;
  hrtc.Init.SynchPrediv = 255;
  hrtc.Init.OutPut = RTC_OUTPUT_DISABLE;
  hrtc.Init.OutPutPolarity = RTC_OUTPUT_POLARITY_HIGH;
  hrtc.Init.OutPutType = RTC_OUTPUT_TYPE_OPENDRAIN;
  hrtc.Init.OutPutRemap = RTC_OUTPUT_REMAP_NONE;
  if (HAL_RTC_Init(&hrtc) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN RTC_Init 2 */

  /* USER CODE END RTC_Init 2 */

}

/**
  * @brief SPI1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI1_Init(void)
{

  /* USER CODE BEGIN SPI1_Init 0 */

  /* USER CODE END SPI1_Init 0 */

  /* USER CODE BEGIN SPI1_Init 1 */

  /* USER CODE END SPI1_Init 1 */
  /* SPI1 parameter configuration*/
  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_SLAVE;
  hspi1.Init.Direction = SPI_DIRECTION_2LINES_RXONLY;
  hspi1.Init.DataSize = SPI_DATASIZE_16BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_HIGH;
  hspi1.Init.CLKPhase = SPI_PHASE_2EDGE;
  hspi1.Init.NSS = SPI_NSS_SOFT;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 0x0;
  hspi1.Init.NSSPMode = SPI_NSS_PULSE_DISABLE;
  hspi1.Init.NSSPolarity = SPI_NSS_POLARITY_LOW;
  hspi1.Init.FifoThreshold = SPI_FIFO_THRESHOLD_01DATA;
  hspi1.Init.TxCRCInitializationPattern = SPI_CRC_INITIALIZATION_ALL_ZERO_PATTERN;
  hspi1.Init.RxCRCInitializationPattern = SPI_CRC_INITIALIZATION_ALL_ZERO_PATTERN;
  hspi1.Init.MasterSSIdleness = SPI_MASTER_SS_IDLENESS_00CYCLE;
  hspi1.Init.MasterInterDataIdleness = SPI_MASTER_INTERDATA_IDLENESS_00CYCLE;
  hspi1.Init.MasterReceiverAutoSusp = SPI_MASTER_RX_AUTOSUSP_DISABLE;
  hspi1.Init.MasterKeepIOState = SPI_MASTER_KEEP_IO_STATE_DISABLE;
  hspi1.Init.IOSwap = SPI_IO_SWAP_DISABLE;
  if (HAL_SPI_Init(&hspi1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI1_Init 2 */

  /* USER CODE END SPI1_Init 2 */

}

/**
  * @brief SPI2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI2_Init(void)
{

  /* USER CODE BEGIN SPI2_Init 0 */

  /* USER CODE END SPI2_Init 0 */

  /* USER CODE BEGIN SPI2_Init 1 */

  /* USER CODE END SPI2_Init 1 */
  /* SPI2 parameter configuration*/
  hspi2.Instance = SPI2;
  hspi2.Init.Mode = SPI_MODE_SLAVE;
  hspi2.Init.Direction = SPI_DIRECTION_2LINES_RXONLY;
  hspi2.Init.DataSize = SPI_DATASIZE_16BIT;
  hspi2.Init.CLKPolarity = SPI_POLARITY_HIGH;
  hspi2.Init.CLKPhase = SPI_PHASE_2EDGE;
  hspi2.Init.NSS = SPI_NSS_SOFT;
  hspi2.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi2.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi2.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi2.Init.CRCPolynomial = 0x0;
  hspi2.Init.NSSPMode = SPI_NSS_PULSE_DISABLE;
  hspi2.Init.NSSPolarity = SPI_NSS_POLARITY_LOW;
  hspi2.Init.FifoThreshold = SPI_FIFO_THRESHOLD_01DATA;
  hspi2.Init.TxCRCInitializationPattern = SPI_CRC_INITIALIZATION_ALL_ZERO_PATTERN;
  hspi2.Init.RxCRCInitializationPattern = SPI_CRC_INITIALIZATION_ALL_ZERO_PATTERN;
  hspi2.Init.MasterSSIdleness = SPI_MASTER_SS_IDLENESS_00CYCLE;
  hspi2.Init.MasterInterDataIdleness = SPI_MASTER_INTERDATA_IDLENESS_00CYCLE;
  hspi2.Init.MasterReceiverAutoSusp = SPI_MASTER_RX_AUTOSUSP_DISABLE;
  hspi2.Init.MasterKeepIOState = SPI_MASTER_KEEP_IO_STATE_DISABLE;
  hspi2.Init.IOSwap = SPI_IO_SWAP_DISABLE;
  if (HAL_SPI_Init(&hspi2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI2_Init 2 */

  /* USER CODE END SPI2_Init 2 */

}

/**
  * @brief SPI3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI3_Init(void)
{

  /* USER CODE BEGIN SPI3_Init 0 */

  /* USER CODE END SPI3_Init 0 */

  /* USER CODE BEGIN SPI3_Init 1 */

  /* USER CODE END SPI3_Init 1 */
  /* SPI3 parameter configuration*/
  hspi3.Instance = SPI3;
  hspi3.Init.Mode = SPI_MODE_SLAVE;
  hspi3.Init.Direction = SPI_DIRECTION_2LINES_RXONLY;
  hspi3.Init.DataSize = SPI_DATASIZE_16BIT;
  hspi3.Init.CLKPolarity = SPI_POLARITY_HIGH;
  hspi3.Init.CLKPhase = SPI_PHASE_2EDGE;
  hspi3.Init.NSS = SPI_NSS_SOFT;
  hspi3.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi3.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi3.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi3.Init.CRCPolynomial = 0x0;
  hspi3.Init.NSSPMode = SPI_NSS_PULSE_DISABLE;
  hspi3.Init.NSSPolarity = SPI_NSS_POLARITY_LOW;
  hspi3.Init.FifoThreshold = SPI_FIFO_THRESHOLD_01DATA;
  hspi3.Init.TxCRCInitializationPattern = SPI_CRC_INITIALIZATION_ALL_ZERO_PATTERN;
  hspi3.Init.RxCRCInitializationPattern = SPI_CRC_INITIALIZATION_ALL_ZERO_PATTERN;
  hspi3.Init.MasterSSIdleness = SPI_MASTER_SS_IDLENESS_00CYCLE;
  hspi3.Init.MasterInterDataIdleness = SPI_MASTER_INTERDATA_IDLENESS_00CYCLE;
  hspi3.Init.MasterReceiverAutoSusp = SPI_MASTER_RX_AUTOSUSP_DISABLE;
  hspi3.Init.MasterKeepIOState = SPI_MASTER_KEEP_IO_STATE_DISABLE;
  hspi3.Init.IOSwap = SPI_IO_SWAP_DISABLE;
  if (HAL_SPI_Init(&hspi3) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI3_Init 2 */

  /* USER CODE END SPI3_Init 2 */

}

/**
  * @brief SPI4 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI4_Init(void)
{

  /* USER CODE BEGIN SPI4_Init 0 */

  /* USER CODE END SPI4_Init 0 */

  /* USER CODE BEGIN SPI4_Init 1 */

  /* USER CODE END SPI4_Init 1 */
  /* SPI4 parameter configuration*/
  hspi4.Instance = SPI4;
  hspi4.Init.Mode = SPI_MODE_SLAVE;
  hspi4.Init.Direction = SPI_DIRECTION_2LINES_RXONLY;
  hspi4.Init.DataSize = SPI_DATASIZE_16BIT;
  hspi4.Init.CLKPolarity = SPI_POLARITY_HIGH;
  hspi4.Init.CLKPhase = SPI_PHASE_2EDGE;
  hspi4.Init.NSS = SPI_NSS_SOFT;
  hspi4.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi4.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi4.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi4.Init.CRCPolynomial = 0x0;
  hspi4.Init.NSSPMode = SPI_NSS_PULSE_DISABLE;
  hspi4.Init.NSSPolarity = SPI_NSS_POLARITY_LOW;
  hspi4.Init.FifoThreshold = SPI_FIFO_THRESHOLD_01DATA;
  hspi4.Init.TxCRCInitializationPattern = SPI_CRC_INITIALIZATION_ALL_ZERO_PATTERN;
  hspi4.Init.RxCRCInitializationPattern = SPI_CRC_INITIALIZATION_ALL_ZERO_PATTERN;
  hspi4.Init.MasterSSIdleness = SPI_MASTER_SS_IDLENESS_00CYCLE;
  hspi4.Init.MasterInterDataIdleness = SPI_MASTER_INTERDATA_IDLENESS_00CYCLE;
  hspi4.Init.MasterReceiverAutoSusp = SPI_MASTER_RX_AUTOSUSP_DISABLE;
  hspi4.Init.MasterKeepIOState = SPI_MASTER_KEEP_IO_STATE_DISABLE;
  hspi4.Init.IOSwap = SPI_IO_SWAP_DISABLE;
  if (HAL_SPI_Init(&hspi4) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI4_Init 2 */

  /* USER CODE END SPI4_Init 2 */

}

/**
  * @brief SPI5 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI5_Init(void)
{

  /* USER CODE BEGIN SPI5_Init 0 */

  /* USER CODE END SPI5_Init 0 */

  /* USER CODE BEGIN SPI5_Init 1 */

  /* USER CODE END SPI5_Init 1 */
  /* SPI5 parameter configuration*/
  hspi5.Instance = SPI5;
  hspi5.Init.Mode = SPI_MODE_SLAVE;
  hspi5.Init.Direction = SPI_DIRECTION_2LINES_RXONLY;
  hspi5.Init.DataSize = SPI_DATASIZE_16BIT;
  hspi5.Init.CLKPolarity = SPI_POLARITY_HIGH;
  hspi5.Init.CLKPhase = SPI_PHASE_2EDGE;
  hspi5.Init.NSS = SPI_NSS_SOFT;
  hspi5.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi5.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi5.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi5.Init.CRCPolynomial = 0x0;
  hspi5.Init.NSSPMode = SPI_NSS_PULSE_DISABLE;
  hspi5.Init.NSSPolarity = SPI_NSS_POLARITY_LOW;
  hspi5.Init.FifoThreshold = SPI_FIFO_THRESHOLD_01DATA;
  hspi5.Init.TxCRCInitializationPattern = SPI_CRC_INITIALIZATION_ALL_ZERO_PATTERN;
  hspi5.Init.RxCRCInitializationPattern = SPI_CRC_INITIALIZATION_ALL_ZERO_PATTERN;
  hspi5.Init.MasterSSIdleness = SPI_MASTER_SS_IDLENESS_00CYCLE;
  hspi5.Init.MasterInterDataIdleness = SPI_MASTER_INTERDATA_IDLENESS_00CYCLE;
  hspi5.Init.MasterReceiverAutoSusp = SPI_MASTER_RX_AUTOSUSP_DISABLE;
  hspi5.Init.MasterKeepIOState = SPI_MASTER_KEEP_IO_STATE_DISABLE;
  hspi5.Init.IOSwap = SPI_IO_SWAP_DISABLE;
  if (HAL_SPI_Init(&hspi5) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI5_Init 2 */

  /* USER CODE END SPI5_Init 2 */

}

/**
  * @brief SPI6 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI6_Init(void)
{

  /* USER CODE BEGIN SPI6_Init 0 */
	HAL_GPIO_WritePin(GPIOG, GPIO_PIN_13, GPIO_PIN_SET);   // SCK High

  /* USER CODE END SPI6_Init 0 */

  /* USER CODE BEGIN SPI6_Init 1 */

  /* USER CODE END SPI6_Init 1 */
  /* SPI6 parameter configuration*/
  hspi6.Instance = SPI6;
  hspi6.Init.Mode = SPI_MODE_MASTER;
  hspi6.Init.Direction = SPI_DIRECTION_2LINES;
  hspi6.Init.DataSize = SPI_DATASIZE_16BIT;
  hspi6.Init.CLKPolarity = SPI_POLARITY_HIGH;
  hspi6.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi6.Init.NSS = SPI_NSS_SOFT;
  hspi6.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_4;
  hspi6.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi6.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi6.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi6.Init.CRCPolynomial = 0x0;
  hspi6.Init.NSSPMode = SPI_NSS_PULSE_ENABLE;
  hspi6.Init.NSSPolarity = SPI_NSS_POLARITY_LOW;
  hspi6.Init.FifoThreshold = SPI_FIFO_THRESHOLD_01DATA;
  hspi6.Init.TxCRCInitializationPattern = SPI_CRC_INITIALIZATION_ALL_ZERO_PATTERN;
  hspi6.Init.RxCRCInitializationPattern = SPI_CRC_INITIALIZATION_ALL_ZERO_PATTERN;
  hspi6.Init.MasterSSIdleness = SPI_MASTER_SS_IDLENESS_00CYCLE;
  hspi6.Init.MasterInterDataIdleness = SPI_MASTER_INTERDATA_IDLENESS_00CYCLE;
  hspi6.Init.MasterReceiverAutoSusp = SPI_MASTER_RX_AUTOSUSP_DISABLE;
  hspi6.Init.MasterKeepIOState = SPI_MASTER_KEEP_IO_STATE_ENABLE;
  hspi6.Init.IOSwap = SPI_IO_SWAP_DISABLE;
  if (HAL_SPI_Init(&hspi6) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI6_Init 2 */

  /* USER CODE END SPI6_Init 2 */

}

/**
  * @brief TIM1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM1_Init(void)
{

  /* USER CODE BEGIN TIM1_Init 0 */

  /* USER CODE END TIM1_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};
  TIM_BreakDeadTimeConfigTypeDef sBreakDeadTimeConfig = {0};

  /* USER CODE BEGIN TIM1_Init 1 */

  /* USER CODE END TIM1_Init 1 */
  htim1.Instance = TIM1;
  htim1.Init.Prescaler = 0;
  htim1.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim1.Init.Period = 1919;
  htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim1.Init.RepetitionCounter = 0;
  htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim1) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim1, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_Init(&htim1) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterOutputTrigger2 = TIM_TRGO2_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim1, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 10;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  sConfigOC.OCIdleState = TIM_OCIDLESTATE_RESET;
  sConfigOC.OCNIdleState = TIM_OCNIDLESTATE_RESET;
  if (HAL_TIM_PWM_ConfigChannel(&htim1, &sConfigOC, TIM_CHANNEL_4) != HAL_OK)
  {
    Error_Handler();
  }
  sBreakDeadTimeConfig.OffStateRunMode = TIM_OSSR_DISABLE;
  sBreakDeadTimeConfig.OffStateIDLEMode = TIM_OSSI_DISABLE;
  sBreakDeadTimeConfig.LockLevel = TIM_LOCKLEVEL_OFF;
  sBreakDeadTimeConfig.DeadTime = 0;
  sBreakDeadTimeConfig.BreakState = TIM_BREAK_DISABLE;
  sBreakDeadTimeConfig.BreakPolarity = TIM_BREAKPOLARITY_HIGH;
  sBreakDeadTimeConfig.BreakFilter = 0;
  sBreakDeadTimeConfig.Break2State = TIM_BREAK2_DISABLE;
  sBreakDeadTimeConfig.Break2Polarity = TIM_BREAK2POLARITY_HIGH;
  sBreakDeadTimeConfig.Break2Filter = 0;
  sBreakDeadTimeConfig.AutomaticOutput = TIM_AUTOMATICOUTPUT_DISABLE;
  if (HAL_TIMEx_ConfigBreakDeadTime(&htim1, &sBreakDeadTimeConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM1_Init 2 */

  /* USER CODE END TIM1_Init 2 */
  HAL_TIM_MspPostInit(&htim1);

}

/**
  * @brief TIM6 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM6_Init(void)
{

  /* USER CODE BEGIN TIM6_Init 0 */

  /* USER CODE END TIM6_Init 0 */

  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM6_Init 1 */

  /* USER CODE END TIM6_Init 1 */
  htim6.Instance = TIM6;
  htim6.Init.Prescaler = 3749;
  htim6.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim6.Init.Period = 249;
  htim6.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim6) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_UPDATE;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim6, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM6_Init 2 */

  /* USER CODE END TIM6_Init 2 */

}

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 900000;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  huart1.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart1.Init.ClockPrescaler = UART_PRESCALER_DIV1;
  huart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetTxFifoThreshold(&huart1, UART_TXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetRxFifoThreshold(&huart1, UART_RXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_DisableFifoMode(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

}

/**
  * Enable DMA controller clock
  */
static void MX_BDMA_Init(void)
{

  /* DMA controller clock enable */
  __HAL_RCC_BDMA_CLK_ENABLE();

  /* DMA interrupt init */
  /* BDMA_Channel0_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(BDMA_Channel0_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(BDMA_Channel0_IRQn);
  /* BDMA_Channel1_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(BDMA_Channel1_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(BDMA_Channel1_IRQn);

}

/**
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void)
{

  /* DMA controller clock enable */
  __HAL_RCC_DMA1_CLK_ENABLE();
  __HAL_RCC_DMA2_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA1_Stream0_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Stream0_IRQn, 14, 0);
  HAL_NVIC_EnableIRQ(DMA1_Stream0_IRQn);
  /* DMA1_Stream1_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Stream1_IRQn, 14, 0);
  HAL_NVIC_EnableIRQ(DMA1_Stream1_IRQn);
  /* DMA1_Stream2_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Stream2_IRQn, 14, 0);
  HAL_NVIC_EnableIRQ(DMA1_Stream2_IRQn);
  /* DMA1_Stream3_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Stream3_IRQn, 14, 0);
  HAL_NVIC_EnableIRQ(DMA1_Stream3_IRQn);
  /* DMA1_Stream4_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Stream4_IRQn, 14, 0);
  HAL_NVIC_EnableIRQ(DMA1_Stream4_IRQn);
  /* DMA1_Stream5_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Stream5_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Stream5_IRQn);
  /* DMA2_Stream0_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA2_Stream0_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA2_Stream0_IRQn);
  /* DMAMUX1_OVR_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMAMUX1_OVR_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMAMUX1_OVR_IRQn);

}

/**
  * Enable MDMA controller clock
  * Configure MDMA for global transfers
  *   hmdma_mdma_channel0_sw_0
  */
static void MX_MDMA_Init(void)
{

  /* MDMA controller clock enable */
  __HAL_RCC_MDMA_CLK_ENABLE();
  /* Local variables */

  /* Configure MDMA channel MDMA_Channel0 */
  /* Configure MDMA request hmdma_mdma_channel0_sw_0 on MDMA_Channel0 */
  hmdma_mdma_channel0_sw_0.Instance = MDMA_Channel0;
  hmdma_mdma_channel0_sw_0.Init.Request = MDMA_REQUEST_SW;
  hmdma_mdma_channel0_sw_0.Init.TransferTriggerMode = MDMA_BLOCK_TRANSFER;
  hmdma_mdma_channel0_sw_0.Init.Priority = MDMA_PRIORITY_VERY_HIGH;
  hmdma_mdma_channel0_sw_0.Init.Endianness = MDMA_LITTLE_ENDIANNESS_PRESERVE;
  hmdma_mdma_channel0_sw_0.Init.SourceInc = MDMA_SRC_INC_WORD;
  hmdma_mdma_channel0_sw_0.Init.DestinationInc = MDMA_DEST_INC_WORD;
  hmdma_mdma_channel0_sw_0.Init.SourceDataSize = MDMA_SRC_DATASIZE_WORD;
  hmdma_mdma_channel0_sw_0.Init.DestDataSize = MDMA_DEST_DATASIZE_WORD;
  hmdma_mdma_channel0_sw_0.Init.DataAlignment = MDMA_DATAALIGN_PACKENABLE;
  hmdma_mdma_channel0_sw_0.Init.BufferTransferLength = 256;
  hmdma_mdma_channel0_sw_0.Init.SourceBurst = MDMA_SOURCE_BURST_SINGLE;
  hmdma_mdma_channel0_sw_0.Init.DestBurst = MDMA_DEST_BURST_SINGLE;
  hmdma_mdma_channel0_sw_0.Init.SourceBlockAddressOffset = 0;
  hmdma_mdma_channel0_sw_0.Init.DestBlockAddressOffset = 0;
  if (HAL_MDMA_Init(&hmdma_mdma_channel0_sw_0) != HAL_OK)
  {
    Error_Handler();
  }

  /* MDMA interrupt initialization */
  /* MDMA_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(MDMA_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(MDMA_IRQn);

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOE_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOF_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOG_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(CS_GPIO_Port, CS_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOD, LEDG_Pin|LEDY_Pin|LEDR_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : FRSTDATA_Pin */
  GPIO_InitStruct.Pin = FRSTDATA_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(FRSTDATA_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : BUSY_Pin */
  GPIO_InitStruct.Pin = BUSY_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(BUSY_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : CS_Pin */
  GPIO_InitStruct.Pin = CS_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(CS_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : LEDG_Pin LEDY_Pin LEDR_Pin */
  GPIO_InitStruct.Pin = LEDG_Pin|LEDY_Pin|LEDR_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(BUSY_EXTI_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(BUSY_EXTI_IRQn);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
static void init_adc_and_buffers()	{
	struct ad7606_pins pins = {
			.cs = {CS},
			.busy = {BUSY},
			.frstdata = {FRSTDATA},
			.convst = {CONVST},
	};

	union ad7606_spi spi = {
		.by_name = {
			.douta = DOUTA,
			.doutb = DOUTB,
			.doutc = DOUTC,
			.doutd = DOUTD,
			.doute = DOUTE,
			.doutf = NULL,
			.doutg = NULL,
			.douth = DOUTH,
			.sdi   = MASTER_SPI,
		}
	};

	struct ad7606_config config = {
		.status_header = false,
		.external_oversampling_clock = false,
		.dout_format = AD7606_DOUT_8,
		.operation_mode = AD7606_OPERATION_NORMAL,
	};

	struct ad7606_channel channels[8];
	for(int i = 0; i < 8; i++){
//			AD7606_MUX_CTRL_TEMP,
//			AD7606_MUX_CTRL_2V5_REF,
//			AD7606_MUX_CTRL_1V8_ALDO,
//			AD7606_MUX_CTRL_1V8_DLDO,
//			AD7606_MUX_CTRL_V_DRIVE,
//			AD7606_MUX_CTRL_A_GND,
//			AD7606_MUX_CTRL_AV_CC;
		AD7606_CHANNEL_MUX_CTRL mux_ctrl = AD7606_MUX_CTRL_A_IN; // = (i != 8) ? (AD7606_MUX_CTRL_A_IN) : (AD7606_MUX_CTRL_TEMP);
		AD7606_CHANNEL_RANGE range = AD7606_RANGE_SE_PM_12_5V;
		switch(i){
			case(2):
				range = AD7606_RANGE_SE_PM_2_5V;
			break;
			case(5):
				mux_ctrl = AD7606_MUX_CTRL_A_GND;
				range = AD7606_RANGE_SE_PM_2_5V;
			break;
			case(6):
				mux_ctrl = AD7606_MUX_CTRL_AV_CC;
				range = AD7606_RANGE_SE_0_TO_5V;
			break;
			case(7):
				mux_ctrl = AD7606_MUX_CTRL_TEMP;
				range = AD7606_RANGE_SE_PM_2_5V;
			break;
		}
	    struct ad7606_channel ch = {
	        .open_detect    = false,
	        .high_bandwidth = true,
	        .range          = range,
			.gain 			= 0,
			.phase 			= 0,
			.offset 		= 0x80,
			.mux_ctrl 		= mux_ctrl,
	    };
	    channels[i] = ch;
	}

	struct ad7606_oversampling oversampling = {
			.oversampling_ratio = 3, // 2^N oversampling
			.oversampling_padding = 0,
	};

	struct ad7606_digital_diagnostics digital_diagnostics = {
			.rom_CRC_err_en = true,
			.mm_CRC_err_en = false,
			.int_CRC_err_en = false,
			.spi_write_err_en = false,
			.spi_read_err_en = false,
			.busy_stuck_high_err_en = true,
			.clk_fs_os_en = false,
			.interface_check_en = false,
	};

	ADC_settings.config = config;
	ADC_settings.digital_diagnostics = digital_diagnostics;
	ADC_settings.oversampling = oversampling;

	for(int i = 0; i < 8; i++){
		ADC_settings.channels[i] = channels[i];
	}
	my_ADC.cooked = true;
	ad7606_init(&my_ADC, &ADC_regs, pins, spi, &ADC_settings, &diagnostics_sample);
	if(verbose){
		printf("ADC initialized. Status register:\t");
		print_binary(ad7606_check_status(&my_ADC),8);
		printf("\r\n");

		printf("Digital diagnostics error register:\t");
		print_binary(ad7606_check_digital_error(&my_ADC),8);
		printf("\r\n");
	}

	uint8_t interface_check_result[8];
	ad7606_check_interface(&my_ADC, interface_check_result);
	if(!hydrophone_valid[0]){
		for(int i = 0; i < N_HYDROPHONES; i++){
			hydrophone_valid[i] = interface_check_result[i];
		}
	}
	if(verbose){
		printf("Interface check result:\r\n");
		for(int i = 0; i < 8; i++){
			printf("Channel V%d: ",i+1);
		  switch(interface_check_result[i]){
		  case 0xFF:
			  printf("Not configured");
			  break;
		  case 0:
			  printf("Fail");
			  break;
		  case 1:
			  printf("Pass");
			break;
		  default:
			printf("Unknown result");
			break;
		  }
		  printf("\t\t\t");
		  if(i%4 == 3) printf("\r\n");
		}
		printf("\r\n");
	}

	int lengths[8] = {
			BUFFER_LEN,
			BUFFER_LEN,
			BUFFER_LEN,
			BUFFER_LEN,
			BUFFER_LEN,
			0,
			0,
			0,
	};
	int16_t* buffers[8] = {NULL};
	for(int i = 0; i < 5; i++) buffers[i] = (int16_t*)&hydrophone_buffers[i][0][0];

	ad7606_enter_adc_mode(&my_ADC);

	for(int i = 0; i < 5; i++){
		clear_buffer_q15(hydrophone_buffers[i][0], BUFFER_LEN);
	}
	clear_buffer_q15(detection_buffer[0], BLOCK_LEN*2);


	ad7606_init_output_buffers_DMA(&my_ADC, buffers, lengths);
	HAL_MDMA_RegisterCallback(&hmdma_mdma_channel0_sw_0, HAL_MDMA_XFER_CPLT_CB_ID,  MyMDMA_TransferCompleteCallback);
	HAL_MDMA_RegisterCallback(&hmdma_mdma_channel0_sw_0, HAL_MDMA_XFER_ERROR_CB_ID, MyMDMA_ErrorCallback);
	ad7606_dma_spi_init(&my_ADC, &hdma_spi6_rx, diagnostics_buffer, BLOCK_LEN);
}
/* USER CODE END 4 */

 /* MPU Configuration */

void MPU_Config(void)
{
  MPU_Region_InitTypeDef MPU_InitStruct = {0};

  /* Disables the MPU */
  HAL_MPU_Disable();

  /** Initializes and configures the Region and the memory to be protected
  */
  MPU_InitStruct.Enable = MPU_REGION_ENABLE;
  MPU_InitStruct.Number = MPU_REGION_NUMBER0;
  MPU_InitStruct.BaseAddress = 0x0;
  MPU_InitStruct.Size = MPU_REGION_SIZE_4GB;
  MPU_InitStruct.SubRegionDisable = 0x87;
  MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL0;
  MPU_InitStruct.AccessPermission = MPU_REGION_NO_ACCESS;
  MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_DISABLE;
  MPU_InitStruct.IsShareable = MPU_ACCESS_SHAREABLE;
  MPU_InitStruct.IsCacheable = MPU_ACCESS_NOT_CACHEABLE;
  MPU_InitStruct.IsBufferable = MPU_ACCESS_NOT_BUFFERABLE;

  HAL_MPU_ConfigRegion(&MPU_InitStruct);
  /* Enables the MPU */
  HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);

}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
    /* User can add his own implementation to report the HAL error return state
     */
	HAL_TIM_PWM_Stop(&htim1, TIM_CHANNEL_4);
    __disable_irq();
    HAL_GPIO_WritePin(GREEN_LED, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(YELLOW_LED, GPIO_PIN_RESET);
    while(1){
    	HAL_GPIO_WritePin(RED_LED, GPIO_PIN_SET);
    	utils_DWT_delay_ms(1000);
    	HAL_GPIO_WritePin(RED_LED, GPIO_PIN_RESET);

    	HAL_NVIC_SystemReset();
    }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
    /* User can add his own implementation to report the file name and line
       number, ex: printf("Wrong parameters value: file %s on line %d\r\n",
       file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
