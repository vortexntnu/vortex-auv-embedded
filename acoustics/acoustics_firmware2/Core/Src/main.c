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
#include "spi6_dma.h"

#include "stm32h7xx_hal.h"
#include "stm32h7xx_hal_gpio.h"
#include "stm32h7xx_hal_gpio_ex.h"
#include "stm32h7xx_hal_spi.h"
#include "stm32h7xx_hal_fdcan.h"

#include "arm_math_types.h"
#include "arm_math.h"

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define FFT_SIZE BLOCK_LEN  // match this to your buffer size
#define SAMPLE_RATE_HZ 62500
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

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
DMA_HandleTypeDef hdma_spi6_tx;

TIM_HandleTypeDef htim1;

UART_HandleTypeDef huart1;

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
void PeriphCommonClock_Config(void);
static void MPU_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_BDMA_Init(void);
static void MX_FDCAN1_Init(void);
static void MX_SPI1_Init(void);
static void MX_SPI2_Init(void);
static void MX_SPI3_Init(void);
static void MX_SPI4_Init(void);
static void MX_SPI5_Init(void);
static void MX_SPI6_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_RTC_Init(void);
static void MX_TIM1_Init(void);
static void MX_CRC_Init(void);
/* USER CODE BEGIN PFP */
static inline void RearmBDMA_Fast(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
q15_t hydrophone_buffers[N_HYDROPHONES][N_BLOCKS][BLOCK_LEN] = {0};
q15_t diagnostics_sample __attribute__((section(".SRAM4")));

SPI_HandleTypeDef* const spi_handle_array[6] = {&hspi1, &hspi2, &hspi3, &hspi4, &hspi5, &hspi6};
SPI_HandleTypeDef* const dout_channels_array[6] = {DOUTA, DOUTB, DOUTC, DOUTD, DOUTE, DOUTH};

static struct ad7606_device my_ADC;
static struct ad7606_registers ADC_regs;

volatile DMA_SPI_ChannelState dma_channel_state[N_HYDROPHONES + 1] = {DMA_SPI_IDLE};

__attribute__((section(".DTCM"))) q15_t fft_output[FFT_SIZE * 2];
__attribute__((section(".DTCM"))) q15_t fft_input[FFT_SIZE];
__attribute__((section(".DTCM"))) q15_t mag[FFT_SIZE / 2];
__attribute__((section(".DTCM"))) arm_rfft_instance_q15 fft_instance;
const float bin_resolution = SAMPLE_RATE_HZ / (float)FFT_SIZE;


uint16_t buffer_remaining = BUFFER_LEN;
uint16_t buffer_current_idx = 0;
uint16_t buffer_latest_idx = BUFFER_LEN;
uint16_t buffer_current_block = 0;
uint16_t buffer_latest_block = N_BLOCKS;
uint16_t buffer_current_block_idx = 0;
uint16_t buffer_latest_block_idx = BLOCK_LEN;

double reading_to_voltage(int reading){
	return reading * 381.5 / 1000000;
}

void start_convst(void){
	HAL_GPIO_WritePin(CONVST, GPIO_PIN_SET);
	HAL_GPIO_WritePin(CONVST, GPIO_PIN_RESET);
}

void update_buffer_idx(void){
	buffer_remaining = __HAL_DMA_GET_COUNTER(&hdma_spi2_rx);

	buffer_current_idx = BUFFER_LEN - buffer_remaining;
	buffer_latest_idx = (buffer_current_idx == 0) ? (BUFFER_LEN - 1) : (buffer_current_idx - 1);

	buffer_current_block = buffer_current_idx/BLOCK_LEN;
	buffer_latest_block = buffer_latest_idx/BLOCK_LEN;

	buffer_current_block_idx = buffer_current_idx%BLOCK_LEN;
	buffer_latest_block_idx = buffer_latest_idx%BLOCK_LEN;

}

void read_hydrophone_buffers_at_idx(q15_t data_array[N_HYDROPHONES], uint16_t idx){
	update_buffer_idx();
	if(idx == buffer_current_idx){
		idx = buffer_latest_idx;
	}
	for(int i = 0; i < N_HYDROPHONES; i++){
		data_array[i] = ((q15_t*)hydrophone_buffers[i])[idx];
	}
}

void read_hydrophone_block_at_idx(q15_t data_array[N_HYDROPHONES],uint16_t block, uint16_t idx){
	update_buffer_idx();
	if(block == buffer_current_block){
		block = buffer_latest_block;
		if(idx == buffer_current_block_idx){
			idx = buffer_latest_block_idx;
		}
	}
	for(int i = 0; i < N_HYDROPHONES; i++){
		data_array[i] = hydrophone_buffers[i][block][idx];
	}
}

void read_newest_hydrophone_data(q15_t data_array[N_HYDROPHONES]){
	update_buffer_idx();
	for(int i = 0; i < N_HYDROPHONES; i++){
		data_array[i] = hydrophone_buffers[i][buffer_latest_block][buffer_latest_block_idx];
	}
}

void init_hyrdophone_buffers(void){
	for(int i = 0; i < N_HYDROPHONES; i++){
		if(HAL_SPI_GetState(dout_channels_array[i]) != HAL_SPI_STATE_READY){
		    HAL_SPI_DMAStop(dout_channels_array[i]);
		}
		dout_channels_array[i]->hdmarx->Init.Mode = DMA_CIRCULAR;
		HAL_DMA_Init(dout_channels_array[i]->hdmarx);

		HAL_SPI_Receive_DMA(dout_channels_array[i], (uint8_t*)&hydrophone_buffers[i], BUFFER_LEN);
		dma_channel_state[i] = DMA_SPI_CIRCULAR;
	}
}

bool all_dma_complete(void){
	bool all_complete = true;
	for(int i = 0; i < N_HYDROPHONES; i++){
		all_complete &= (dma_channel_state[i] == DMA_SPI_COMPLETE);
	}
	return all_complete;
}

bool all_dma_idle(void){
	bool all_idle = true;
	for(int i = 0; i < N_HYDROPHONES; i++){
		all_idle &= (dma_channel_state[i] == DMA_SPI_IDLE);
	}
	return all_idle;
}

bool dma_busy(void){
	bool busy = false;
	for(int i = 0; i < N_HYDROPHONES; i++){
		busy |= (dma_channel_state[i] == DMA_SPI_RUNNING);
	}
	return busy;
}

bool dma_error(void){
	bool error = false;
	for(int i = 0; i < N_HYDROPHONES; i++){
		error |= (dma_channel_state[i] == DMA_SPI_ERROR);
	}
	return error;
}

int _write(int file, char *ptr, int len)
{
    HAL_UART_Transmit(&huart1, (uint8_t*)ptr, len, HAL_MAX_DELAY);
    return len;
}

volatile uint8_t new_data = false;

// __attribute__((used))
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == BUSY_Pin)
    {
//        HAL_GPIO_WritePin(CS, GPIO_PIN_RESET);
//        SET_BIT(MASTER_SPI->Instance->CR1, SPI_CR1_CSTART);

        // In your ISR or wherever you trigger a transfer:
        if (!SPI6_IsBusy()) {
            SPI6_Transfer16(0x1234);
        }
//        HAL_StatusTypeDef status = HAL_SPI_TransmitReceive_IT(
//            MASTER_SPI,
//            (uint8_t*)&READ_CONVST,
//            (uint8_t*)&diagnostics_sample,
//            1
//        );
//
//        if (status != HAL_OK)
//        {
//            // Deassert CS so we don't leave the bus in a bad state
//            HAL_GPIO_WritePin(CS, GPIO_PIN_SET);
//        }
    }
}

void HAL_SPI_TxRxCpltCallback(SPI_HandleTypeDef *hspi){
    if(hspi->Instance == MASTER_SPI->Instance){
        HAL_GPIO_WritePin(CS, GPIO_PIN_SET);
        //CLEAR_BIT(MASTER_SPI->Instance->CR1, SPI_CR1_CSTART);
    }
}

void HAL_SPI_RxCpltCallback(SPI_HandleTypeDef *hspi) {
	if(hspi->Instance == DOUTA->Instance){
		new_data = true;
	}

//    for (int i = 0; i < N_HYDROPHONES; i++) {
//        if ((hspi->Instance == dout_channels_array[i]->Instance) && (dma_channel_state[i] != DMA_SPI_CIRCULAR)) {
//            dma_channel_state[i] = DMA_SPI_COMPLETE;
//            break;
//        }
//    }
}

void DMA_TxCplt(DMA_HandleTypeDef *hdma)
{
  if(hdma == MASTER_SPI->hdmatx){
    // CS high — transfer done
        HAL_GPIO_WritePin(CS, GPIO_PIN_SET);

        // Clear SPI EOT + TXTF so next CSTART works
        WRITE_REG(MASTER_SPI->Instance->IFCR,
                  SPI_IFCR_EOTC | SPI_IFCR_TXTFC);

        new_data = true;

        // Fast re-arm: ~5 register writes, no HAL overhead
        RearmBDMA_Fast();
  }
    //CLEAR_BIT(MASTER_SPI->Instance->CR1, SPI_CR1_CSTART);
    // TSIZE=1 already stopped SPI
    // BDMA reloads automatically in circular mode
}

void DMA_Error(DMA_HandleTypeDef *hdma)
{
    // Handle error — at minimum deassert CS
    HAL_GPIO_WritePin(CS, GPIO_PIN_SET);
    Error_Handler();
    // Optionally log hdma->ErrorCode
}

void HAL_SPI_ErrorCallback(SPI_HandleTypeDef *hspi) {
    for (int i = 0; i < N_HYDROPHONES; i++) {
        if (hspi->Instance == dout_channels_array[i]->Instance) {
            dma_channel_state[i] = DMA_SPI_ERROR;
            break;
        }
    }
}

void change_buffers_to_normal(void){
    for(int i = 0; i < N_HYDROPHONES; i++){
        HAL_SPI_DMAStop(dout_channels_array[i]);
        dout_channels_array[i]->hdmarx->Init.Mode = DMA_NORMAL;
        HAL_DMA_Init(dout_channels_array[i]->hdmarx);
        dma_channel_state[i] = DMA_SPI_IDLE;
    }
}

static inline void RearmBDMA_Fast(void)
{
    BDMA_Channel_TypeDef *rx_ch = (BDMA_Channel_TypeDef *)MASTER_SPI->hdmarx->Instance;
    BDMA_Channel_TypeDef *tx_ch = (BDMA_Channel_TypeDef *)MASTER_SPI->hdmatx->Instance;

    // Disable both BDMA channels briefly to reload
    rx_ch->CCR &= ~BDMA_CCR_EN;
    tx_ch->CCR &= ~BDMA_CCR_EN;

    // Clear all BDMA interrupt flags for both channels
    {
        uint32_t rx_idx = ((uint32_t)rx_ch - (uint32_t)BDMA_Channel0) /
                          ((uint32_t)BDMA_Channel1 - (uint32_t)BDMA_Channel0);
        uint32_t tx_idx = ((uint32_t)tx_ch - (uint32_t)BDMA_Channel0) /
                          ((uint32_t)BDMA_Channel1 - (uint32_t)BDMA_Channel0);
        BDMA->IFCR = (0xFUL << (4U * rx_idx)) | (0xFUL << (4U * tx_idx));
    }

    // Reload transfer count = 1
    rx_ch->CNDTR = 1;
    tx_ch->CNDTR = 1;

    // Re-enable both channels (TX first so data is ready when SPI clocks)
    tx_ch->CCR |= BDMA_CCR_EN;
    rx_ch->CCR |= BDMA_CCR_EN;
}

static void MasterSpi_Setup(void)
{
	if(HAL_SPI_GetState(MASTER_SPI) != HAL_SPI_STATE_READY){
		HAL_SPI_DMAStop(MASTER_SPI);
	}
    // Use NORMAL mode, not circular
    MASTER_SPI->hdmarx->Init.Mode   = DMA_NORMAL;
    MASTER_SPI->hdmarx->Init.MemInc = DMA_MINC_DISABLE;
    HAL_DMA_Init(MASTER_SPI->hdmarx);


    MASTER_SPI->hdmatx->Init.Mode   = DMA_NORMAL;
    MASTER_SPI->hdmatx->Init.MemInc = DMA_MINC_DISABLE;
    HAL_DMA_Init(MASTER_SPI->hdmatx);

    // Register callbacks
    MASTER_SPI->hdmarx->XferCpltCallback     = NULL; //DMA_RxCplt
    MASTER_SPI->hdmarx->XferErrorCallback    = DMA_Error;
    MASTER_SPI->hdmarx->XferHalfCpltCallback = NULL;
    MASTER_SPI->hdmarx->XferAbortCallback    = NULL;

    MASTER_SPI->hdmatx->XferCpltCallback     = DMA_TxCplt;
    MASTER_SPI->hdmatx->XferHalfCpltCallback = NULL;
    MASTER_SPI->hdmatx->XferErrorCallback    = DMA_Error;
    MASTER_SPI->hdmatx->XferAbortCallback    = NULL;

    // Set TSIZE = 1
    MODIFY_REG(MASTER_SPI->Instance->CR2, SPI_CR2_TSIZE, 1U);

    // Initial DMA arm (first time uses HAL)
    HAL_DMA_Start_IT(MASTER_SPI->hdmarx,
                     (uint32_t)&MASTER_SPI->Instance->RXDR,
                     (uint32_t)&diagnostics_sample,
                     1U);

    HAL_DMA_Start_IT(MASTER_SPI->hdmatx,
                     (uint32_t)&READ_CONVST,
                     (uint32_t)&MASTER_SPI->Instance->TXDR,
                     1U);

    // Enable SPI DMA requests
    SET_BIT(MASTER_SPI->Instance->CFG1, SPI_CFG1_RXDMAEN | SPI_CFG1_TXDMAEN);

    // Enable SPI
    __HAL_SPI_ENABLE(MASTER_SPI);
}



// In your application file — implement the callback:
//void SPI6_TransferCpltCallback(uint16_t rx_data) {
//    // rx_data is the received word — CS is already high by this point
//
//}
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

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* Configure the peripherals common clocks */
  PeriphCommonClock_Config();

  /* USER CODE BEGIN SysInit */
  utils_DWT_init();

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_BDMA_Init();
  MX_FDCAN1_Init();
  MX_SPI1_Init();
  MX_SPI2_Init();
  MX_SPI3_Init();
  MX_SPI4_Init();
  MX_SPI5_Init();
  MX_SPI6_Init();
  MX_USART1_UART_Init();
  MX_RTC_Init();
  MX_TIM1_Init();
  MX_CRC_Init();
  /* USER CODE BEGIN 2 */

	HAL_GPIO_WritePin(CS, GPIO_PIN_SET);   // CS High


	uint8_t msg[] = "USART1 OK\r\n";
	HAL_UART_Transmit(&huart1, msg, sizeof(msg) - 1, 100);
	HAL_GPIO_WritePin(GREEN_LED, GPIO_PIN_SET);

	{
		struct ad7606_pins pins = {
				.cs = {CS},
				.busy = {BUSY},
				.frstdata = {FRSTDATA},
				.convst = {CONVST},
		};

		struct ad7606_spi spi = {
				.douta = DOUTA,
				.doutb = DOUTB,
				.doutc = DOUTC,
				.doutd = DOUTD,
				.doute = DOUTE,
				.doutf = NULL,
				.doutg = NULL,
				.douth = DOUTH,
				.sdi   = MASTER_SPI,
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
			switch(i){
			case(2):
				mux_ctrl = AD7606_MUX_CTRL_2V5_REF;
			break;
			case(3):
				mux_ctrl = AD7606_MUX_CTRL_AV_CC;
			break;
			case(4):
				mux_ctrl = AD7606_MUX_CTRL_V_DRIVE;
			break;
			case(7):
				mux_ctrl = AD7606_MUX_CTRL_TEMP;
			break;

			}
		    struct ad7606_channel ch = {
		        .open_detect    = false,
		        .high_bandwidth = true,
		        .range          = AD7606_RANGE_SE_PM_12_5V,
				.gain 			= 0,
				.phase 			= 0,
				.offset 		= 0x80,
				.mux_ctrl 		= mux_ctrl,
		    };
		    channels[i] = ch;
		}

		struct ad7606_oversampling oversampling = {
				.oversampling_ratio = 3,
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

		struct ad7606_settings settings = {
				.config = config,
				.digital_diagnostics = digital_diagnostics,
				.oversampling = oversampling,
		};

		for(int i = 0; i < 8; i++){
			settings.channels[i] = channels[i];
		}

		ad7606_init(&my_ADC, &ADC_regs, pins, spi, settings);
	}
	arm_rfft_init_q15(&fft_instance, FFT_SIZE, 0, 1);
	EXIT_REGISTER_MODE = 0x0000;
	EXIT_ADC_MODE = 0x4100;

	init_hyrdophone_buffers();

//	// Arm BDMA directly
//	HAL_DMA_Start_IT(&hdma_spi6_rx, (uint32_t)&hspi6.Instance->RXDR,
//	                  (uint32_t)&diagnostics_sample, 1);
//	HAL_DMA_Start_IT(&hdma_spi6_tx, (uint32_t)&EXIT_REGISTER_MODE,
//	                  (uint32_t)&hspi6.Instance->TXDR, 1);
//
//	// Enable SPI DMA requests
//	SET_BIT(hspi6.Instance->CFG1, SPI_CFG1_TXDMAEN | SPI_CFG1_RXDMAEN);
//
//	// Enable SPI error interrupts so HAL error handling still works
//	__HAL_SPI_ENABLE_IT(&hspi6, (SPI_IT_OVR | SPI_IT_UDR | SPI_IT_FRE | SPI_IT_MODF));
//
//	// Enable SPI — but don't set CSTART yet
//	__HAL_SPI_ENABLE(&hspi6);
//
//	// Set SPI handle state so HAL doesn't think it's uninitialized
//	hspi6.State = HAL_SPI_STATE_BUSY_TX_RX;

	MasterSpi_Setup();

	// Call your modified version — sets up BDMA, enables SPI, but doesn't set CSTART
//	SPI_TransmitReceive_DMA_NoStart(
//	    MASTER_SPI,
//	    (uint8_t*)&READ_CONVST,
//	    (uint8_t*)&diagnostics_sample,
//	    1
//	);

	__HAL_TIM_SET_AUTORELOAD(&htim1, 3838);
	HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_4);


  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
    while (1) {
    	//q15_t received_data[8] = {0};
    	//start_convst();
    	while(!new_data) __NOP();
    	q15_t max_val;
    	uint32_t max_idx;
    	//float dominant_freq;
    	update_buffer_idx();
    	arm_rfft_q15(&fft_instance, hydrophone_buffers[0][buffer_latest_block_idx], fft_output);
    	arm_cmplx_mag_q15(fft_output, mag, FFT_SIZE / 2);
    	arm_max_q15(mag, FFT_SIZE / 2, &max_val, &max_idx);
    	printf("Peak_bin:%lu\tFrequency:%d\r\n", max_idx, (int)(max_idx * bin_resolution));

//    	read_newest_hydrophone_data(received_data);
//		received_data[7] = diagnostics_sample;
//		printf("\r\nIDX:%d\t",buffer_latest_idx);
//		for(int i = 0; i < 8; i++){
//			double voltage = reading_to_voltage(received_data[i]);
//			int whole = (int)voltage;
//			int frac  = (int)((voltage - whole) * 1000);  // 3 decimal places
//			printf("\tV%d:%d.%03d",i, whole, frac);
//
//		}
		new_data = false;

		//printf("IDX:%d,DOUTA:%d,DOUTB:%d,DOUTC:%d,DOUTD:%d,DOUTE:%d,DOUTF:%d,DOUTG:%d,DOUTH:%d\r\n",buffer_latest_idx,received_data[0],received_data[1],received_data[2],received_data[3],received_data[4],received_data[5],received_data[6],received_data[7]);

        //HAL_Delay(100);
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    }
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
  PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_SPI6|RCC_PERIPHCLK_SPI3
                              |RCC_PERIPHCLK_SPI2|RCC_PERIPHCLK_SPI1
                              |RCC_PERIPHCLK_SPI4|RCC_PERIPHCLK_SPI5
                              |RCC_PERIPHCLK_FDCAN;
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
  PeriphClkInitStruct.Spi6ClockSelection = RCC_SPI6CLKSOURCE_PLL2;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
  {
    Error_Handler();
  }
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
  hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
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
  hspi2.Init.CLKPhase = SPI_PHASE_1EDGE;
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
  hspi3.Init.CLKPhase = SPI_PHASE_1EDGE;
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
  hspi4.Init.CLKPhase = SPI_PHASE_1EDGE;
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
  hspi5.Init.CLKPhase = SPI_PHASE_1EDGE;
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
  hspi6.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_2;
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
  htim1.Init.Period = 1919; //3838;//
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
  sConfigOC.Pulse = 3;
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
  huart1.Init.BaudRate = 115200;
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
  HAL_GPIO_WritePin(CS_GPIO_Port, CS_Pin, GPIO_PIN_RESET);

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
  HAL_NVIC_SetPriority(BUSY_EXTI_IRQn, 2, 0);
  HAL_NVIC_EnableIRQ(BUSY_EXTI_IRQn);

  /* USER CODE BEGIN MX_GPIO_Init_2 */
    GPIO_InitStruct.Pin = GPIO_PIN_13 | GPIO_PIN_14;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF5_SPI6;
    HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_6;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF5_SPI6;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

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
