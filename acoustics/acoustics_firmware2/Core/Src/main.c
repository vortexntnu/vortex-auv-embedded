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
#include "find_peaks.h"

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

PLACE_IN_DTCM arm_rfft_instance_q15 detection_fft_instance;
MDMA_BUF_DTCM(ALIGN_DMA_BURST_8_WORD) q15_t detection_buffer[2][BLOCK_LEN];
PLACE_IN_DTCM q15_t detection_fft_output[DETECTION_FFT_SIZE * 2];
PLACE_IN_DTCM q15_t magnitude_output[DETECTION_FFT_SIZE / 2];

PLACE_IN_DTCM volatile uint8_t mdma_half = 0;
PLACE_IN_DTCM volatile bool mdma_done_flag = false;

PLACE_IN_DTCM arm_rfft_instance_q15 processing_fft_instance;
PLACE_IN_DTCM arm_rfft_instance_q15 processing_ifft_instance;

static PLACE_IN_AXI_SRAM q15_t processing_workspace[N_HYDROPHONES][PROCESSING_FFT_SIZE];
static PLACE_IN_AXI_SRAM q15_t processing_fft_input[PROCESSING_FFT_SIZE];
static PLACE_IN_AXI_SRAM q15_t processing_fft_output[PROCESSING_FFT_SIZE*2];

// CWT-specific buffers
static PLACE_IN_AXI_SRAM q15_t cwt_kernel[PROCESSING_FFT_SIZE * 2];     // complex Morlet kernel (freq domain)
static PLACE_IN_AXI_SRAM q15_t cwt_working_buffer[PROCESSING_FFT_SIZE];
static PLACE_IN_AXI_SRAM q15_t cwt_product[PROCESSING_FFT_SIZE * 2];    // complex product buffer
static PLACE_IN_AXI_SRAM q15_t cwt_result[PROCESSING_FFT_SIZE * 2];     // IFFT output (complex)
static PLACE_IN_AXI_SRAM q15_t cwt_out[PROCESSING_FFT_SIZE];

// Hilbert specific
static PLACE_IN_DTCM q15_t hilbert_working_buffer[PROCESSING_FFT_SIZE*2];
static PLACE_IN_DTCM q15_t hilbert_analytic_signal[PROCESSING_FFT_SIZE*2];
static PLACE_IN_DTCM q15_t envelope[PROCESSING_FFT_SIZE];
static PLACE_IN_DTCM q15_t envelope_edge[PROCESSING_FFT_SIZE];

#define PRINT_BUF_SIZE (N_HYDROPHONES * N_BLOCKS * BLOCK_LEN * 8 + 1024)
static char print_buf[PRINT_BUF_SIZE];
int pos = 0;
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
static void init_adc_and_buffers(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

q15_t q15_from_float(float f) {
    return (q15_t)(f * 32767.0f);
}

#define MAX_PEAKS 4

uint32_t find_da_edge(const q15_t *signal, uint32_t signal_len)
{
	q15_t buf_min;
	uint32_t min_idx;
	arm_min_q15(signal, signal_len, &buf_min, &min_idx);
	q15_t min_height = (q15_t)(((int32_t)(-buf_min) * 22938) >> 15);

    uint32_t           peak_idx[MAX_PEAKS];
    find_peaks_props_t props[MAX_PEAKS];
    uint32_t           n_peaks;

    find_peaks_config_t cfg = FIND_PEAKS_CONFIG_DEFAULT;
    cfg.height     = min_height;
    cfg.prominence = 328;          // 0.01 * 32767
    cfg.distance   = 5;

    find_peaks(signal, signal_len, &cfg, peak_idx, props, MAX_PEAKS, &n_peaks);


    // 4. first_peak = np.min(find_peakss_data) → lowest index found
    //    peak_idx is already in ascending index order, so index 0 is the first
    uint32_t first_peak;
    if (n_peaks > 0) {
        first_peak = peak_idx[0];          // leftmost peak (min index)
    } else {
        arm_min_q15(signal, signal_len, &buf_min, &first_peak);  // fallback: argmin
    }
    return first_peak;
}


// claude generated
//#define ARM_CFFT_INSTANCE_Q15(len) (          \
//    (len) == 16   ? &arm_cfft_sR_q15_len16  : \
//    (len) == 32   ? &arm_cfft_sR_q15_len32  : \
//    (len) == 64   ? &arm_cfft_sR_q15_len64  : \
//    (len) == 128  ? &arm_cfft_sR_q15_len128 : \
//    (len) == 256  ? &arm_cfft_sR_q15_len256 : \
//    (len) == 512  ? &arm_cfft_sR_q15_len512 : \
//    (len) == 1024 ? &arm_cfft_sR_q15_len1024: \
//    (len) == 2048 ? &arm_cfft_sR_q15_len2048: \
//    (len) == 4096 ? &arm_cfft_sR_q15_len4096: \
//    NULL)



// claude generated
/**
 * @brief  Hilbert Transform using Q15 fixed-point CMSIS-DSP.
 *         Input:  real Q15 samples  (range -1.0 to ~1.0 mapped to -32768..32767)
 *         Output: imaginary (quadrature) Q15 samples — the analytic signal's imag part.
 *
 * @param  pSrc     Input real Q15 samples
 * @param  pDst     Output quadrature Q15 samples (length fftSize)
 * @param  fftSize  Must be a power of 2: 256, 512, 1024, 2048, ...
 * @param  pScratch Scratch buffer of (fftSize * 2) q15_t elements (interleaved Re/Im)
 */
void hilbert_transform_q15(const q15_t *pSrc,
                           q15_t       *pDst)  // length PROCESSING_FFT_SIZE*2, interleaved Re/Im
{
    arm_rfft_instance_q15 rfft;

    arm_rfft_init_q15(&rfft, PROCESSING_FFT_SIZE, 0, 1);

    arm_copy_q15((q15_t *)pSrc, processing_fft_input, PROCESSING_FFT_SIZE);

    arm_rfft_q15(&rfft, processing_fft_input, hilbert_working_buffer);

    /* Zero DC bin */
    hilbert_working_buffer[0] = 0;
    hilbert_working_buffer[1] = 0;

    /* Positive frequency bins: apply -j rotation */
    for (uint32_t k = 1; k < PROCESSING_FFT_SIZE / 2; k++)
    {
        q15_t re = hilbert_working_buffer[2 * k];
        q15_t im = hilbert_working_buffer[2 * k + 1];

        hilbert_working_buffer[2 * k]     =  im;
        hilbert_working_buffer[2 * k + 1] = -re;
    }

    /* Zero Nyquist bin */
    hilbert_working_buffer[PROCESSING_FFT_SIZE]     = 0;
    hilbert_working_buffer[PROCESSING_FFT_SIZE + 1] = 0;

    /* Zero all negative frequency bins */
    for (uint32_t k = PROCESSING_FFT_SIZE / 2 + 1; k < PROCESSING_FFT_SIZE; k++)
    {
        hilbert_working_buffer[2 * k]     = 0;
        hilbert_working_buffer[2 * k + 1] = 0;
    }
    arm_cfft_instance_q15 cfft;
    arm_cfft_init_q15(&cfft,PROCESSING_FFT_SIZE);
    arm_cfft_q15(&cfft, hilbert_working_buffer, 1, 1);

    /* Copy interleaved Re/Im directly to pDst for arm_cmplx_mag_q15 */
    for (uint32_t i = 0; i < PROCESSING_FFT_SIZE; i++)
    {
        pDst[2 * i]     = hilbert_working_buffer[2 * i];      // Re
        pDst[2 * i + 1] = hilbert_working_buffer[2 * i + 1];  // Im
    }
}

void hilbert_imag_q15(const q15_t *pSrc, q15_t *pDst)
{
    arm_rfft_instance_q15 rfft;
    arm_rfft_init_q15(&rfft, PROCESSING_FFT_SIZE, 0, 1);

    arm_copy_q15((q15_t *)pSrc, processing_fft_input, PROCESSING_FFT_SIZE);

    arm_rfft_q15(&rfft, processing_fft_input, hilbert_working_buffer);

    hilbert_working_buffer[0] = 0;
    hilbert_working_buffer[1] = 0;

    for (uint32_t k = 1; k < PROCESSING_FFT_SIZE / 2; k++)
    {
        q15_t re = hilbert_working_buffer[2 * k];
        q15_t im = hilbert_working_buffer[2 * k + 1];
        hilbert_working_buffer[2 * k]     =  im;
        hilbert_working_buffer[2 * k + 1] = -re;
    }

    hilbert_working_buffer[PROCESSING_FFT_SIZE]     = 0;
    hilbert_working_buffer[PROCESSING_FFT_SIZE + 1] = 0;

    for (uint32_t k = PROCESSING_FFT_SIZE / 2 + 1; k < PROCESSING_FFT_SIZE; k++)
    {
        hilbert_working_buffer[2 * k]     = 0;
        hilbert_working_buffer[2 * k + 1] = 0;
    }

    arm_cfft_instance_q15 cfft;
    arm_cfft_init_q15(&cfft,PROCESSING_FFT_SIZE);
    arm_cfft_q15(&cfft, hilbert_working_buffer, 1, 1);

    /* Imaginary part only */
    for (uint32_t i = 0; i < PROCESSING_FFT_SIZE; i++)
        pDst[i] = hilbert_working_buffer[2 * i + 1];
}


// claude generated
// -----------------------------------------------------------
// Build Morlet kernel at startup (float -> q15 conversion)
// target_freq: the frequency you want to focus on (e.g. 30000.0f)
// fs:          your ADC sample rate (e.g. 200000.0f for AD7606)
// -----------------------------------------------------------
void cwt_build_morlet_kernel_q15(float32_t target_freq, float32_t fs, float32_t f0)
{
    // f0: center frequency parameter (cycles) — controls bandwidth.
    // Higher f0 = narrower bandwidth, better freq resolution, worse time resolution.
    // Typical range: 5.0 to 8.0. Default 5.0 is standard.

    float32_t omega0 = 2.0f * PI * f0;
    float32_t scale  = f0 / target_freq;

    for (uint32_t k = 0; k < PROCESSING_FFT_SIZE / 2; k++)
    {
        // Normalized angular frequency — no fs multiplier
        float32_t omega = 2.0f * PI * (float32_t)k / PROCESSING_FFT_SIZE;
        float32_t arg   = (scale * omega - omega0);
        float32_t val   = expf(-0.5f * arg * arg);

        cwt_kernel[2 * k]     = (q15_t)(val * 32767.0f);
        cwt_kernel[2 * k + 1] = 0;
    }

    // Zero negative frequencies — analytic (one-sided) Morlet
    for (uint32_t k = PROCESSING_FFT_SIZE / 2; k < PROCESSING_FFT_SIZE; k++)
    {
        cwt_kernel[2 * k]     = 0;
        cwt_kernel[2 * k + 1] = 0;
    }
}


// claude generated
// input:    your raw q15_t ADC buffer [PROCESSING_FFT_SIZE]
// out_mag:  output magnitude buffer [PROCESSING_FFT_SIZE] — time-domain envelope
void cwt_morlet_q15(const q15_t *input, q15_t *out_mag)
{
    // 1. Copy input (arm_rfft_q15 modifies in-place)
    arm_copy_q15(input, processing_fft_input, PROCESSING_FFT_SIZE);

    // 2. Forward FFT  →  processing_fft_output is complex interleaved, length PROCESSING_FFT_SIZE*2
    arm_rfft_q15(&processing_fft_instance, processing_fft_input, processing_fft_output);

    // 3. Complex multiply: fft_output * cwt_kernel → cwt_product
    //    arm_cmplx_mult_cmplx_q15 saturates and right-shifts by 1 internally
    arm_cmplx_mult_cmplx_q15(processing_fft_output, cwt_kernel, cwt_product, PROCESSING_FFT_SIZE);

    // 4. Inverse FFT  →  cwt_result is complex interleaved
    //    Pass ifftFlag=1, bitReverseFlag=1
    arm_rfft_q15(&processing_ifft_instance, cwt_product, cwt_result);
    //    NOTE: CMSIS arm_rfft_q15 does not support in-place IFFT natively;
    //    if your version lacks IFFT, use arm_cfft_q15 instead (see note below)

    // 5. Complex magnitude of result → envelope of CWT at target frequency
    arm_cmplx_mag_q15(cwt_result, out_mag, PROCESSING_FFT_SIZE);
    //    out_mag[n] = sqrt(re^2 + im^2) — this is your time-domain energy envelope
}


// claude generated
void cwt_init(void)
{
	arm_rfft_init_q15(&processing_fft_instance,  PROCESSING_FFT_SIZE, 0, 1); // forward
	arm_rfft_init_q15(&processing_ifft_instance, PROCESSING_FFT_SIZE, 1, 1); // inverse
    cwt_build_morlet_kernel_q15((float32_t)TARGET_FREQUENCY_HZ, (float32_t)SAMPLING_FREQUENCY,5); // 30kHz target, 125kHz fs
    q15_t cwt_kernel_ifft[PROCESSING_FFT_SIZE];
    arm_rfft_q15(&processing_ifft_instance, cwt_kernel, cwt_kernel_ifft);
    // adjust fs ^ to match your AD7606 config
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == BUSY_INT)
    {
        // Kick next transfer — non-blocking, returns in ~5 cycles
    	ad7606_trigger_burst(MASTER_SPI);
    	//ad7606_fast_spi_run(MASTER_SPI);
    }
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

void HAL_SPI_TxRxCpltCallback(SPI_HandleTypeDef *hspi){

}

void HAL_SPI_RxCpltCallback(SPI_HandleTypeDef *hspi) {

}

void HAL_SPI_ErrorCallback(SPI_HandleTypeDef *hspi) {
    for (int i = 0; i < N_HYDROPHONES; i++) {
        if (hspi->Instance == dout_channel_handles[i]->Instance) {
            dma_channel_state[i] = DMA_SPI_ERROR;
            break;
        }
    }
}

void change_buffers_to_normal(void){
    for(int i = 0; i < N_HYDROPHONES; i++){
        HAL_SPI_DMAStop(dout_channel_handles[i]);
        dout_channel_handles[i]->hdmarx->Init.Mode = DMA_NORMAL;
        HAL_DMA_Init(dout_channel_handles[i]->hdmarx);
        dma_channel_state[i] = DMA_SPI_IDLE;
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


static void My_MDMA_SetConfig(MDMA_HandleTypeDef *hmdma, uint32_t SrcAddress, uint32_t DstAddress, uint32_t BlockDataLength, uint32_t BlockCount)
{
	  uint32_t addressMask;

	/* Configure the MDMA Channel data length */
	  MODIFY_REG(hmdma->Instance->CBNDTR ,MDMA_CBNDTR_BNDT, (BlockDataLength & MDMA_CBNDTR_BNDT));

	  /* Configure the MDMA block repeat count */
	  MODIFY_REG(hmdma->Instance->CBNDTR , MDMA_CBNDTR_BRC , ((BlockCount - 1U) << MDMA_CBNDTR_BRC_Pos) & MDMA_CBNDTR_BRC);

	  /* Clear all interrupt flags */
	  __HAL_MDMA_CLEAR_FLAG(hmdma, MDMA_FLAG_TE | MDMA_FLAG_CTC | MDMA_CISR_BRTIF | MDMA_CISR_BTIF | MDMA_CISR_TCIF);

	  /* Configure MDMA Channel destination address */
	  hmdma->Instance->CDAR = DstAddress;

	  /* Configure MDMA Channel Source address */
	  hmdma->Instance->CSAR = SrcAddress;

    addressMask = SrcAddress & 0xFF000000U;
    if((addressMask == 0x20000000U) || (addressMask == 0x00000000U))
    {
      /*The AHBSbus is used as source (read operation) on channel x */
      hmdma->Instance->CTBR |= MDMA_CTBR_SBUS;
    }
    else
    {
      /*The AXI bus is used as source (read operation) on channel x */
      hmdma->Instance->CTBR &= (~MDMA_CTBR_SBUS);
    }

    addressMask = DstAddress & 0xFF000000U;
    if((addressMask == 0x20000000U) || (addressMask == 0x00000000U))
    {
      /*The AHB bus is used as destination (write operation) on channel x */
      hmdma->Instance->CTBR |= MDMA_CTBR_DBUS;
    }
    else
    {
      /*The AXI bus is used as destination (write operation) on channel x */
      hmdma->Instance->CTBR &= (~MDMA_CTBR_DBUS);
    }

    /* Set the linked list register to the first node of the list */
    hmdma->Instance->CLAR = (uint32_t)hmdma->FirstLinkedListNodeAddress;
    // copy any other register writes from HAL source
}

HAL_StatusTypeDef MDMA_CopyBlock(q15_t *src, q15_t *dst)
{
		/* Process locked */
		__HAL_LOCK(&hmdma_mdma_channel0_sw_0);

		/* Change MDMA peripheral state */
		hmdma_mdma_channel0_sw_0.State = HAL_MDMA_STATE_BUSY;

	    /* Initialize the error code */
	    hmdma_mdma_channel0_sw_0.ErrorCode = HAL_MDMA_ERROR_NONE;

	    /* Disable the peripheral */
	    __HAL_MDMA_DISABLE(&hmdma_mdma_channel0_sw_0);

	    /* Configure the source, destination address and the data length */
	    My_MDMA_SetConfig(&hmdma_mdma_channel0_sw_0, (uint32_t)src, (uint32_t)dst, BLOCK_LEN*sizeof(q15_t), 1);

	    /* Enable Common interrupts i.e Transfer Error IT and Channel Transfer Complete IT*/
	    __HAL_MDMA_ENABLE_IT(&hmdma_mdma_channel0_sw_0, (MDMA_IT_TE | MDMA_IT_CTC));


	    /* Enable the Peripheral */
	    __HAL_MDMA_ENABLE(&hmdma_mdma_channel0_sw_0);

	    /* activate If SW request mode*/
	    hmdma_mdma_channel0_sw_0.Instance->CCR |=  MDMA_CCR_SWRQ;

	    return HAL_OK;
}

void MDMA_WaitComplete(void) {
    while (!(MDMA_Channel0->CISR & MDMA_CISR_CTCIF));      // wait for transfer complete
    MDMA_Channel0->CIFCR = MDMA_CIFCR_CCTCIF;              // clear the flag
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

//void MDMA_IRQHandler(void) {
//    mdma_transfer_cycles = DWT_CYCCNT - mdma_start_time;
//    MDMA_Channel0->CIFCR = 0x1F;  // clear flags
//    MyMDMA_TransferCompleteCallback(&hmdma_mdma_channel0_sw_0);
//}

bool signal_present(uint8_t half_idx) {
    arm_rfft_q15(&detection_fft_instance, detection_buffer[half_idx], detection_fft_output);
    arm_cmplx_mag_q15(detection_fft_output, magnitude_output, DETECTION_FFT_SIZE / 2);

    uint32_t sum = 0;
    for (int i = 0; i < 15; i++)
        sum += (uint32_t)(magnitude_output[i] >> 3) * (magnitude_output[i] >> 3);
    for (int i = 17; i < DETECTION_FFT_SIZE / 2; i++)
        sum += (uint32_t)(magnitude_output[i] >> 3) * (magnitude_output[i] >> 3);

    if (sum == 0) return false;

    uint32_t our_value = (uint32_t)(magnitude_output[16] >> 3) * (magnitude_output[16] >> 3)
                       + (uint32_t)(magnitude_output[15] >> 3) * (magnitude_output[15] >> 3);

    return (our_value * 15) > (sum * LINEAR_THRESHOLD);
    //                              ×15 accounts for averaging over 30 bins vs 2 bins
}

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

// --- Benchmark globals ---
volatile uint32_t mdma_cycles;
volatile uint32_t memcpy_cycles;
volatile uint32_t detection_cycles;
volatile uint32_t full_cycles;
volatile uint32_t mdma_transfer_cycles;

float mdma_us;
float memcpy_us;
float detection_us;
float full_us;
float mdma_transfer_us;

void Benchmark(void) {
    uint32_t t_start;
    uint64_t accumulator;
    uint8_t target_block = fast_get_detection_block_pos();
    uint8_t processing_half = !mdma_half;
    const uint32_t BENCHMARK_N = 512;

    accumulator = 0;
    uint64_t accumulator2 = 0;
    for(int i = 0; i < (int)BENCHMARK_N; i++){
        target_block = fast_get_detection_block_pos();
        processing_half = !mdma_half;
        // --- Test 1: MDMA ---
        t_start = DWT_CYCCNT;
        MDMA_CopyBlock(hydrophone_buffers[0][target_block],detection_buffer[mdma_half]);
        accumulator += (DWT_CYCCNT - t_start)/BENCHMARK_N;
		while(mdma_done_flag == false) __NOP();
		accumulator2 += (DWT_CYCCNT - t_start)/BENCHMARK_N;
		mdma_done_flag = false;
    }
    mdma_cycles = accumulator;
    mdma_transfer_cycles = accumulator2;


    // --- Test 2: memcpy ---
    // Wait for MDMA to finish first to avoid bus contention
    accumulator = 0;
    for(int i = 0; i < (int)BENCHMARK_N; i++){
        target_block = fast_get_detection_block_pos();
        processing_half = !mdma_half;
		t_start = DWT_CYCCNT;
		memcpy(detection_buffer[processing_half], hydrophone_buffers[0][target_block], 128);
		accumulator += (DWT_CYCCNT - t_start)/BENCHMARK_N;
    }
    memcpy_cycles = accumulator;

    accumulator = 0;
    for(int i = 0; i < (int)BENCHMARK_N; i++){
        target_block = fast_get_detection_block_pos();
        processing_half = !mdma_half;
		t_start = DWT_CYCCNT;
		signal_present(processing_half);
		accumulator += (DWT_CYCCNT - t_start)/BENCHMARK_N;
	}
	detection_cycles = accumulator;

    accumulator = 0;
    for(int i = 0; i < (int)BENCHMARK_N; i++){
    t_start = DWT_CYCCNT;
    {
		uint8_t target_block = fast_get_detection_block_pos();
		uint8_t processing_half = !mdma_half;
		MDMA_CopyBlock(hydrophone_buffers[0][target_block],detection_buffer[mdma_half]);
		signal_present(processing_half);
		while(mdma_done_flag == false) {
			__NOP();
		}
		mdma_done_flag = false;
    }
    accumulator += (DWT_CYCCNT - t_start)/BENCHMARK_N;
    }
    full_cycles = accumulator;

    mdma_us   = mdma_cycles   / 480.0f;
    memcpy_us = memcpy_cycles / 480.0f;
    detection_us = detection_cycles / 480.0f;
    full_us = full_cycles / 480.0f;
    mdma_transfer_us = mdma_transfer_cycles / 480.0f;

}


// claude generated
void dump_python_array(q15_t* arr, int len) {
    if (arr == NULL || len <= 0) return;

    fflush(stdout);
    char print_buf[PRINT_BUF_SIZE];
    int pos = 0;
    int written = 0;

    written = snprintf(print_buf + pos, sizeof(print_buf) - pos, "[");
    if (written < 0 || pos + written >= (int)sizeof(print_buf)) goto overflow;
    pos += written;

    for (int i = 0; i < len - 1; i++) {
        written = snprintf(print_buf + pos, sizeof(print_buf) - pos, "%d,", arr[i]);
        if (written < 0 || pos + written >= (int)sizeof(print_buf)) goto overflow;
        pos += written;
    }

    written = snprintf(print_buf + pos, sizeof(print_buf) - pos, "%d]", arr[len - 1]);
    if (written < 0 || pos + written >= (int)sizeof(print_buf)) goto overflow;
    pos += written;

    _write(0, print_buf, pos);
    fflush(stdout);
    return;

	overflow:
		__asm("BKPT #0");
}

#define DUMP_ARRAY_NAMED_DICT(name, arr, len) do { \
    printf("\t\"" name "\" : "); \
    dump_python_array((q15_t*)(arr), (len)); \
    printf("\r\n"); \
} while(0)

#define DUMP_ARRAY_NAMED(name, arr, len) do { \
    printf(name " = "); \
    dump_python_array((q15_t*)(arr), (len)); \
    printf("\r\n"); \
} while(0)

#define DUMP_ARRAY(arr, len) do { \
    printf(#arr " = "); \
    dump_python_array((q15_t*)(arr), (len)); \
    printf(",\r\n"); \
} while(0)

void dump_hydrophone_buffers(void){
    pos = 0;

    pos += sprintf(print_buf + pos, "\r\ndata = [\r\n");

    for(int i = 0; i < 5; i++){
        pos += sprintf(print_buf + pos, "\t[");
        for(int j = 0; j < N_BLOCKS; j++){
            for(int k = 0; k < BLOCK_LEN; k++){
                if(j == (N_BLOCKS-1) && k == (BLOCK_LEN-1)){
                    pos += sprintf(print_buf + pos, "%d", hydrophone_buffers[i][j][k]);
                } else {
                    pos += sprintf(print_buf + pos, "%d,", hydrophone_buffers[i][j][k]);
                }
            }
        }
        pos += sprintf(print_buf + pos, (i == 4) ? "]\r\n" : "],\r\n");
    }

    pos += sprintf(print_buf + pos, "]\r\n\r\n");

    // After building the buffer, verify pos before writing
    if (pos > sizeof(print_buf)) {
        // overflow happened — increase print_buf size
        __asm("BKPT #0");
    }

    // Single ITM write instead of thousands of printf calls
    _write(0, print_buf, pos);
}

void clear_buffer(q15_t* arr, int len){
	for(int i = 0; i < len; i++) arr[i] = 0;
}

//#define TEMPSENSOR_CAL1_ADDR  ((uint16_t*) 0x1FF1E820)
//#define TEMPSENSOR_CAL2_ADDR  ((uint16_t*) 0x1FF1E840)
//#define TEMPSENSOR_CAL1_TEMP  30.0f
//#define TEMPSENSOR_CAL2_TEMP  110.0f

PLACE_IN_D3_SRAM volatile uint16_t g_adc3_dma_buf;

volatile float g_die_temp = 0.0f;

float Temp_BDMA_GetLatest(void);

// Just start everything once at init
void TempSensor_Init(void)
{
    HAL_ADC_Start_DMA(&hadc3, (uint32_t*)&g_adc3_dma_buf, 1);
    HAL_TIM_Base_Start(&htim6);
}

// Read whenever you want - buffer updates at timer rate
float TempSensor_GetLatest(void)
{
    return Temp_BDMA_GetLatest();
}

// Optional - fires at 1Hz now instead of constantly
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
    if (hadc->Instance == ADC3)
    {
        g_die_temp = Temp_BDMA_GetLatest();
    }
}

// Call this whenever you want a fresh conversion of the buffer value
float Temp_BDMA_GetLatest(void)
{
	float32_t cal1 = (float32_t)(12490);
	float32_t cal2 = (float32_t)(16465);

	float32_t temp = (130.0 - 30.0) / (cal2 - cal1)
                 * ((float32_t)g_adc3_dma_buf - cal1)
                 + 30.0;

    return temp;
}


float stm_temp_sensor_convert(uint16_t raw_adc)
{
	float32_t cal1 = (float32_t)(12490);
	float32_t cal2 = (float32_t)(16465);

    float temperature = (130.0 - 30.0) /
                        (cal2 - cal1) *
                        (raw_adc - cal1) +
						30.0;
    return temperature;
}

uint16_t stm_Temp_ToRaw(float32_t target_temp)
{
	float32_t cal1 = (float32_t)(12490);
	float32_t cal2 = (float32_t)(16465);

    return (uint16_t)((target_temp - 30.0)
           * (cal2 - cal1) / (130.0f - 30.0)
           + cal1);
}

void HAL_ADC_LevelOutOfWindowCallback(ADC_HandleTypeDef *hadc)
{
    if (hadc->Instance == ADC3)
    {
        // Temperature out of range — take action
        // e.g. reduce clock, shut down peripherals, set a flag
    }
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

	HAL_ADC_Start(&hadc3);
	HAL_ADC_PollForConversion(&hadc3, 10);
	uint16_t raw = HAL_ADC_GetValue(&hadc3);
	utils_delay(10000);
	HAL_ADC_Stop(&hadc3);

	TempSensor_Init();

	init_adc_and_buffers();
	arm_rfft_init_q15(&detection_fft_instance, DETECTION_FFT_SIZE, 0, 1);
	cwt_init();

	HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_4);

//	HAL_GPIO_WritePin(YELLOW_LED, GPIO_PIN_SET);
//	utils_delay(1000000);
//	HAL_GPIO_WritePin(YELLOW_LED, GPIO_PIN_RESET);
//	Benchmark();
//	HAL_GPIO_WritePin(YELLOW_LED, GPIO_PIN_SET);

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
	utils_delay(1000000);
	HAL_GPIO_WritePin(YELLOW_LED, GPIO_PIN_SET);

	int count = SAMPLING_FREQUENCY;
    while (1) {
      uint8_t target_block = fast_get_detection_block_pos();
      uint8_t processing_half = !mdma_half;
      MDMA_CopyBlock(hydrophone_buffers[0][target_block], detection_buffer[mdma_half]);
      //process data ...
      //printf("%d\r\n",target_block);

      diagnostics_temp = (float32_t)ad7606_voltage_to_temp(ad7606_reading_to_voltage(&my_ADC,7,diagnostics_sample));
      stm32_temp = Temp_BDMA_GetLatest();

//      if(signal_present(processing_half)){
//    	  if(!((count++)%SAMPLING_FREQUENCY)){
//    		  printf("Signal detected\r\n");
//    	  }
//      }


    	if(signal_present(processing_half)){ // processing_half
			HAL_TIM_PWM_Stop(&htim1, TIM_CHANNEL_4);
			HAL_GPIO_WritePin(YELLOW_LED, GPIO_PIN_RESET);
			for (int i = 0; i < 5; i++) {
				HAL_SPI_DMAStop(my_ADC.spi_handles[i]);
			}

			printf("Processed half: %d\r\n",processing_half);
			if(!((count++)%SAMPLING_FREQUENCY)){
			  printf("Signal detected\r\n");
			}

			//dump_hydrophone_buffers();

			printf("dump = {\r\n");

			DUMP_ARRAY_NAMED_DICT("cwt kernel",cwt_kernel,DETECTION_FFT_SIZE);
			printf(",");

			DUMP_ARRAY_NAMED_DICT("detection",detection_buffer[0],BLOCK_LEN*2);
			printf(",");

			uint16_t workspace_idx = (target_block*BLOCK_LEN+BUFFER_LEN-WORKSPACE_LEN/2)%BUFFER_LEN;

			printf("\"raw\" : [\r\n\t");
			for(int i = 0; i < N_HYDROPHONES; i++){
				q15_t* buffer_flat = (q15_t*)hydrophone_buffers[i];
	    	    memcpy(processing_workspace[i], &buffer_flat[workspace_idx], WORKSPACE_LEN*sizeof(q15_t));
	    	    dump_python_array(processing_workspace[i], WORKSPACE_LEN);
	    	    if(i == N_HYDROPHONES - 1){
	    	    	printf("\r\n],\r\n");
	    	    }else{
	    	    	printf(",\r\n\t");
	    	    }
			}

			cwt_morlet_q15(processing_workspace[0], cwt_out);
			DUMP_ARRAY_NAMED_DICT("cwt",cwt_out,WORKSPACE_LEN);
			printf(",");

			// Find max absolute value in input
			q15_t max_val;
			uint32_t max_idx;
			arm_absmax_q15(processing_workspace[0], PROCESSING_FFT_SIZE, &max_val, &max_idx);

			// Left-shift input to fill Q15 headroom
			uint32_t headroom = __CLZ((uint32_t)max_val) - 17; // -17 because q15 is 16-bit, leave 1 bit for sign safety
			arm_shift_q15(processing_workspace[0], (int8_t)headroom, hilbert_working_buffer, PROCESSING_FFT_SIZE);

			hilbert_transform_q15(hilbert_working_buffer, hilbert_analytic_signal);
			arm_cmplx_mag_q15(hilbert_analytic_signal, envelope, PROCESSING_FFT_SIZE); // envelope = |analytic|
			DUMP_ARRAY_NAMED_DICT("envelope",envelope,PROCESSING_FFT_SIZE);
			printf(",");

			// Left-shift input to fill Q15 headroom
			uint32_t headroom = __CLZ((uint32_t)max_val) - 17; // -17 because q15 is 16-bit, leave 1 bit for sign safety
			arm_shift_q15(processing_workspace[0], (int8_t)headroom, hilbert_working_buffer, PROCESSING_FFT_SIZE);

			hilbert_imag_q15(envelope, envelope_edge);
			DUMP_ARRAY_NAMED_DICT("envelope_edge",envelope_edge,PROCESSING_FFT_SIZE);

			printf("}\r\n");

			uint32_t edge_idx;
			edge_idx = find_da_edge(envelope_edge, PROCESSING_FFT_SIZE);


			printf("edge_idx = %d\r\n",(int)edge_idx);

			__asm("BKPT #0");
    	    break;
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
  hadc3.Init.OversamplingMode = DISABLE;
  hadc3.Init.Oversampling.Ratio = 1;
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
  sConfig.SamplingTime = ADC_SAMPLETIME_1CYCLE_5;
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
  hfdcan1.Init.FrameFormat = FDCAN_FRAME_CLASSIC;
  hfdcan1.Init.Mode = FDCAN_MODE_NORMAL;
  hfdcan1.Init.AutoRetransmission = DISABLE;
  hfdcan1.Init.TransmitPause = DISABLE;
  hfdcan1.Init.ProtocolException = DISABLE;
  hfdcan1.Init.NominalPrescaler = 16;
  hfdcan1.Init.NominalSyncJumpWidth = 1;
  hfdcan1.Init.NominalTimeSeg1 = 1;
  hfdcan1.Init.NominalTimeSeg2 = 1;
  hfdcan1.Init.DataPrescaler = 1;
  hfdcan1.Init.DataSyncJumpWidth = 1;
  hfdcan1.Init.DataTimeSeg1 = 1;
  hfdcan1.Init.DataTimeSeg2 = 1;
  hfdcan1.Init.MessageRAMOffset = 0;
  hfdcan1.Init.StdFiltersNbr = 0;
  hfdcan1.Init.ExtFiltersNbr = 0;
  hfdcan1.Init.RxFifo0ElmtsNbr = 0;
  hfdcan1.Init.RxFifo0ElmtSize = FDCAN_DATA_BYTES_8;
  hfdcan1.Init.RxFifo1ElmtsNbr = 0;
  hfdcan1.Init.RxFifo1ElmtSize = FDCAN_DATA_BYTES_8;
  hfdcan1.Init.RxBuffersNbr = 0;
  hfdcan1.Init.RxBufferSize = FDCAN_DATA_BYTES_8;
  hfdcan1.Init.TxEventsNbr = 0;
  hfdcan1.Init.TxBuffersNbr = 0;
  hfdcan1.Init.TxFifoQueueElmtsNbr = 0;
  hfdcan1.Init.TxFifoQueueMode = FDCAN_TX_FIFO_OPERATION;
  hfdcan1.Init.TxElmtSize = FDCAN_DATA_BYTES_8;
  if (HAL_FDCAN_Init(&hfdcan1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN FDCAN1_Init 2 */

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
  hspi6.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_8;
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
  htim6.Init.Prescaler = 3999;
  htim6.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim6.Init.Period = 59999;
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
  hmdma_mdma_channel0_sw_0.Init.BufferTransferLength = 128;
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
static void init_adc_and_buffers(void)	{
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

	printf("ADC initialized. Status register:\t");
	print_binary(ad7606_check_status(&my_ADC),8);
	printf("\r\n");

	printf("Digital diagnostics error register:\t");
	print_binary(ad7606_check_digital_error(&my_ADC),8);
	printf("\r\n");

	uint8_t interface_check_result[8];
	ad7606_check_interface(&my_ADC, interface_check_result);
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
		clear_buffer(hydrophone_buffers[i][0], BUFFER_LEN);
	}
	clear_buffer(detection_buffer[0], BLOCK_LEN*2);


	ad7606_init_output_buffers_DMA(&my_ADC, buffers, lengths);
	HAL_MDMA_RegisterCallback(&hmdma_mdma_channel0_sw_0, HAL_MDMA_XFER_CPLT_CB_ID,  MyMDMA_TransferCompleteCallback);
	HAL_MDMA_RegisterCallback(&hmdma_mdma_channel0_sw_0, HAL_MDMA_XFER_ERROR_CB_ID, MyMDMA_ErrorCallback);
	//ad7606_fast_spi_init(&my_ADC);
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
    while(1){
    	HAL_GPIO_WritePin(YELLOW_LED, GPIO_PIN_SET);
    	utils_DWT_delay_ms(500);
    	HAL_GPIO_WritePin(YELLOW_LED, GPIO_PIN_RESET);
    	utils_DWT_delay_ms(500);
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
