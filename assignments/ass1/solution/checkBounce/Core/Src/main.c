/* USER CODE BEGIN Header *//**
  ******************************************************************************
  * @file    main.c   (project checkBounce)
  * @brief   Bounce test for Assignment 1, Part C. Hardware: NUCLEO-G474RE + the
  *          I2C IO board on the cart. Read the instructions in
  *          checkBounce_instructions.txt (same folder as this project).
  *
  * Four inputs are watched at the same time, with NO debouncing:
  *   1. I2C buttons 0..3 (PCA9538A at 0x71), polled as fast as the bus allows
  *   2. Nucleo blue button B1 (PC13), through its EXTI interrupt (rising edges)
  *   3. Nucleo blue button B1 (PC13), polled in the main loop (rising edges)
  *   4. IR obstacle sensor (PA7), polled in the main loop
  *
  * For every input, a "short gap" is an edge that comes less than shortGapUs
  * after the previous edge of the same input. A person cannot press twice
  * within a few tens of milliseconds, so short gaps are bounce (or chatter).
  *
  * All inputs and outputs are global variables, for Live Expressions:
  *   INPUTS  (write): resetCounts, shortGapUs
  *   OUTPUTS (read) : everything else listed in the instructions file
  *
  * Nothing moves: the wheels, LEDs and servo are never commanded.
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
/* USER CODE END Includes */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define PCA9538A_ADDR        0x71
#define PCA9538A_INPUT       0x00
#define PCA9538A_CONFIG      0x03
#define BUTTON_COUNT         4U
#define GAP_NONE             0xFFFFFFFFUL  /* "no gap measured yet"            */
#define GAP_DEFAULT_US       30000UL       /* 30 ms                            */
#define HEARTBEAT_MS         250UL         /* LD2 blinks at 2 Hz while running */
/* USER CODE END PD */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;
ADC_HandleTypeDef hadc5;

I2C_HandleTypeDef hi2c1;

UART_HandleTypeDef hlpuart1;
UART_HandleTypeDef huart3;

TIM_HandleTypeDef htim1;
TIM_HandleTypeDef htim2;
TIM_HandleTypeDef htim3;
TIM_HandleTypeDef htim4;
TIM_HandleTypeDef htim5;
TIM_HandleTypeDef htim8;
TIM_HandleTypeDef htim15;
TIM_HandleTypeDef htim20;

/* USER CODE BEGIN PV */
/* ============================ INPUTS (write) ============================== */
volatile uint8_t  resetCounts = 0;               /* write 1: zero all results */
volatile uint32_t shortGapUs  = GAP_DEFAULT_US;  /* bounce threshold [us]     */

/* ============================ OUTPUTS (read) ============================== */
volatile uint32_t loopsPerSec    = 0;   /* main-loop passes per second        */

/* 1. I2C buttons, polled */
volatile uint8_t  i2cOk          = 0;   /* 1 = button chip answers            */
volatile uint8_t  i2cHeld        = 0;   /* bit n = 1 while button n is held   */
volatile uint32_t i2cReadsPerSec = 0;   /* successful I2C reads per second    */
volatile uint32_t i2cPresses[BUTTON_COUNT]   = {0};   /* press edges          */
volatile uint32_t i2cShortGaps[BUTTON_COUNT] = {0};   /* edges after a short gap */
volatile uint32_t i2cMinGapUs[BUTTON_COUNT]  = {GAP_NONE, GAP_NONE, GAP_NONE, GAP_NONE};

/* 2. B1 through the EXTI interrupt (rising edges only) */
volatile uint32_t b1IrqRises     = 0;
volatile uint32_t b1IrqShortGaps = 0;
volatile uint32_t b1IrqMinGapUs  = GAP_NONE;

/* 3. B1 polled in the main loop */
volatile uint8_t  b1Level        = 0;
volatile uint32_t b1PollRises    = 0;
volatile uint32_t b1PollShortGaps = 0;
volatile uint32_t b1PollMinGapUs = GAP_NONE;

/* 4. IR sensor polled (active-low: 0 = obstacle) */
volatile uint8_t  irLevel        = 1;
volatile uint32_t irObstacles    = 0;   /* edges into "obstacle"              */
volatile uint32_t irShortGaps    = 0;   /* edges (either way) after a short gap */
volatile uint32_t irMinGapUs     = GAP_NONE;

/* ---- internal ---- */
typedef struct {
  uint32_t lastCycles;   /* DWT cycle count of the previous edge */
  uint32_t lastMs;       /* HAL tick of the previous edge        */
  uint8_t  seen;         /* 0 until the first edge               */
} EdgeTimer;

static EdgeTimer i2cAnyEdge[BUTTON_COUNT];
static EdgeTimer b1IrqEdge, b1PollEdge, irEdge;
static uint8_t   i2cLast = 0, b1Last = 0, irLast = 1;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_TIM1_Init(void);
static void MX_TIM2_Init(void);
static void MX_TIM3_Init(void);
static void MX_TIM4_Init(void);
static void MX_TIM5_Init(void);
static void MX_TIM8_Init(void);
static void MX_TIM15_Init(void);
static void MX_ADC5_Init(void);
static void MX_TIM20_Init(void);
static void MX_ADC1_Init(void);
static void MX_LPUART1_UART_Init(void);
static void MX_USART3_UART_Init(void);
static void MX_I2C1_Init(void);
void HAL_TIM_MspPostInit(TIM_HandleTypeDef *htim);

/* USER CODE BEGIN PFP */
static void     dwtInit(void);
static uint32_t edgeGapUs(EdgeTimer *e);
static void     noteGap(uint32_t gap, volatile uint32_t *shortGaps, volatile uint32_t *minGap);
static uint8_t  readI2cButtons(uint8_t *held);
static void     resetResults(void);
static void     pollI2cButtons(void);
static void     pollB1(void);
static void     pollIr(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
/* DWT cycle counter: 170 counts per microsecond */
static void dwtInit(void)
{
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CYCCNT = 0U;
  DWT->CTRL  |= DWT_CTRL_CYCCNTENA_Msk;
}

/* Time since the previous edge of the same input, in microseconds, and
   remember now as the new "previous". GAP_NONE for the first edge and for
   gaps over 1 s (the 32-bit cycle counter wraps every 25 s, so long gaps are
   decided with the millisecond tick instead). */
static uint32_t edgeGapUs(EdgeTimer *e)
{
  uint32_t cycles = DWT->CYCCNT;
  uint32_t ms     = HAL_GetTick();
  uint32_t gap    = GAP_NONE;

  if (e->seen && (ms - e->lastMs) < 1000U) {
    gap = (cycles - e->lastCycles) / (SystemCoreClock / 1000000U);
  }
  e->lastCycles = cycles;
  e->lastMs     = ms;
  e->seen       = 1;
  return gap;
}

static void noteGap(uint32_t gap, volatile uint32_t *shortGaps, volatile uint32_t *minGap)
{
  if (gap == GAP_NONE) return;
  if (gap < shortGapUs) (*shortGaps)++;
  if (gap < *minGap)    *minGap = gap;
}

/* bit n of *held = 1 while button n is held (the chip is active-low) */
static uint8_t readI2cButtons(uint8_t *held)
{
  uint8_t raw;
  if (HAL_I2C_Mem_Read(&hi2c1, (uint16_t)(PCA9538A_ADDR << 1), PCA9538A_INPUT,
                       I2C_MEMADD_SIZE_8BIT, &raw, 1, 10) != HAL_OK) {
    return 0;
  }
  *held = (uint8_t)(~raw) & 0x0FU;
  return 1;
}

static void resetResults(void)
{
  __disable_irq();                       /* B1's interrupt writes its results */
  for (uint8_t i = 0; i < BUTTON_COUNT; i++) {
    i2cPresses[i] = 0; i2cShortGaps[i] = 0; i2cMinGapUs[i] = GAP_NONE;
    i2cAnyEdge[i].seen = 0;
  }
  b1IrqRises = 0;  b1IrqShortGaps = 0;  b1IrqMinGapUs = GAP_NONE;  b1IrqEdge.seen = 0;
  b1PollRises = 0; b1PollShortGaps = 0; b1PollMinGapUs = GAP_NONE; b1PollEdge.seen = 0;
  irObstacles = 0; irShortGaps = 0;     irMinGapUs = GAP_NONE;     irEdge.seen = 0;
  __enable_irq();
}

/* 1. I2C buttons: every edge (press or release) is timed; presses are counted */
static void pollI2cButtons(void)
{
  static uint32_t reads = 0, windowStart = 0;
  uint8_t held;

  if (!readI2cButtons(&held)) { i2cOk = 0; return; }
  i2cOk   = 1;
  i2cHeld = held;
  reads++;

  uint8_t changed = (uint8_t)(held ^ i2cLast);
  for (uint8_t i = 0; i < BUTTON_COUNT; i++) {
    if (changed & (1U << i)) {
      noteGap(edgeGapUs(&i2cAnyEdge[i]), &i2cShortGaps[i], &i2cMinGapUs[i]);
      if (held & (1U << i)) i2cPresses[i]++;
    }
  }
  i2cLast = held;

  if (HAL_GetTick() - windowStart >= 1000U) {
    i2cReadsPerSec = reads;
    reads = 0;
    windowStart = HAL_GetTick();
  }
}

/* 3. B1 polled: rising edges, so the same edges the EXTI interrupt counts */
static void pollB1(void)
{
  uint8_t level = (HAL_GPIO_ReadPin(B1_GPIO_Port, B1_Pin) == GPIO_PIN_SET);
  b1Level = level;
  if (level && !b1Last) {
    b1PollRises++;
    noteGap(edgeGapUs(&b1PollEdge), &b1PollShortGaps, &b1PollMinGapUs);
  }
  b1Last = level;
}

/* 4. IR sensor: every change is timed; changes into "obstacle" are counted */
static void pollIr(void)
{
  uint8_t level = (HAL_GPIO_ReadPin(IR_SENSOR_GPIO_Port, IR_SENSOR_Pin) == GPIO_PIN_SET);
  irLevel = level;
  if (level != irLast) {
    noteGap(edgeGapUs(&irEdge), &irShortGaps, &irMinGapUs);
    if (!level) irObstacles++;
  }
  irLast = level;
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  */
int main(void)
{
  HAL_Init();
  SystemClock_Config();

  MX_GPIO_Init();
  MX_TIM1_Init();
  MX_TIM2_Init();
  MX_TIM3_Init();
  MX_TIM4_Init();
  MX_TIM5_Init();
  MX_TIM8_Init();
  MX_TIM15_Init();
  MX_ADC5_Init();
  MX_TIM20_Init();
  MX_ADC1_Init();
  MX_LPUART1_UART_Init();
  MX_USART3_UART_Init();
  MX_I2C1_Init();

  /* USER CODE BEGIN 2 */
  dwtInit();

  /* button chip: all 8 pins as inputs (also its power-on default) */
  uint8_t config = 0xFF;
  HAL_I2C_Mem_Write(&hi2c1, (uint16_t)(PCA9538A_ADDR << 1), PCA9538A_CONFIG,
                    I2C_MEMADD_SIZE_8BIT, &config, 1, 10);
  uint8_t held = 0;
  i2cOk   = readI2cButtons(&held);
  i2cLast = held;
  b1Last  = (HAL_GPIO_ReadPin(B1_GPIO_Port, B1_Pin) == GPIO_PIN_SET);
  irLast  = (HAL_GPIO_ReadPin(IR_SENSOR_GPIO_Port, IR_SENSOR_Pin) == GPIO_PIN_SET);
  resetResults();

  uint32_t loops = 0, loopWindowStart = HAL_GetTick(), lastBeat = HAL_GetTick();
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    if (resetCounts) {
      resetResults();
      resetCounts = 0;
    }

    pollI2cButtons();
    pollB1();
    pollIr();

    loops++;
    uint32_t now = HAL_GetTick();
    if (now - loopWindowStart >= 1000U) {
      loopsPerSec = loops;
      loops = 0;
      loopWindowStart = now;
    }
    if (now - lastBeat >= HEARTBEAT_MS) {
      lastBeat = now;
      HAL_GPIO_TogglePin(LD2_GPIO_Port, LD2_Pin);   /* alive */
    }
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration (170 MHz from HSI via PLL)
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1_BOOST);

  RCC_OscInitStruct.OscillatorType      = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState            = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState        = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource       = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM            = RCC_PLLM_DIV4;
  RCC_OscInitStruct.PLL.PLLN            = 85;
  RCC_OscInitStruct.PLL.PLLP            = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ            = RCC_PLLQ_DIV2;
  RCC_OscInitStruct.PLL.PLLR            = RCC_PLLR_DIV2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) Error_Handler();

  RCC_ClkInitStruct.ClockType      = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                                   | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider  = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK) Error_Handler();
}

/* ------------------------------------------------------------------------- */
/* MX peripheral init functions - identical to the other main.c files, so    */
/* stm32g4xx_hal_msp.c and stm32g4xx_it.c are shared without changes.        */
/* ------------------------------------------------------------------------- */

static void MX_ADC1_Init(void)
{
  ADC_MultiModeTypeDef multimode = {0};
  ADC_ChannelConfTypeDef sConfig = {0};

  hadc1.Instance = ADC1;
  hadc1.Init.ClockPrescaler        = ADC_CLOCK_SYNC_PCLK_DIV4;
  hadc1.Init.Resolution            = ADC_RESOLUTION_12B;
  hadc1.Init.DataAlign             = ADC_DATAALIGN_RIGHT;
  hadc1.Init.GainCompensation      = 0;
  hadc1.Init.ScanConvMode          = ADC_SCAN_DISABLE;
  hadc1.Init.EOCSelection          = ADC_EOC_SINGLE_CONV;
  hadc1.Init.LowPowerAutoWait      = DISABLE;
  hadc1.Init.ContinuousConvMode    = DISABLE;
  hadc1.Init.NbrOfConversion       = 1;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConv      = ADC_SOFTWARE_START;
  hadc1.Init.ExternalTrigConvEdge  = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc1.Init.DMAContinuousRequests = DISABLE;
  hadc1.Init.Overrun               = ADC_OVR_DATA_PRESERVED;
  hadc1.Init.OversamplingMode      = DISABLE;
  if (HAL_ADC_Init(&hadc1) != HAL_OK) Error_Handler();

  multimode.Mode = ADC_MODE_INDEPENDENT;
  if (HAL_ADCEx_MultiModeConfigChannel(&hadc1, &multimode) != HAL_OK) Error_Handler();

  sConfig.Channel      = ADC_CHANNEL_12;
  sConfig.Rank         = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLETIME_2CYCLES_5;
  sConfig.SingleDiff   = ADC_SINGLE_ENDED;
  sConfig.OffsetNumber = ADC_OFFSET_NONE;
  sConfig.Offset       = 0;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK) Error_Handler();
}

static void MX_ADC5_Init(void)
{
  ADC_ChannelConfTypeDef sConfig = {0};

  hadc5.Instance = ADC5;
  hadc5.Init.ClockPrescaler        = ADC_CLOCK_SYNC_PCLK_DIV4;
  hadc5.Init.Resolution            = ADC_RESOLUTION_12B;
  hadc5.Init.DataAlign             = ADC_DATAALIGN_RIGHT;
  hadc5.Init.GainCompensation      = 0;
  hadc5.Init.ScanConvMode          = ADC_SCAN_DISABLE;
  hadc5.Init.EOCSelection          = ADC_EOC_SINGLE_CONV;
  hadc5.Init.LowPowerAutoWait      = DISABLE;
  hadc5.Init.ContinuousConvMode    = DISABLE;
  hadc5.Init.NbrOfConversion       = 1;
  hadc5.Init.DiscontinuousConvMode = DISABLE;
  hadc5.Init.ExternalTrigConv      = ADC_SOFTWARE_START;
  hadc5.Init.ExternalTrigConvEdge  = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc5.Init.DMAContinuousRequests = DISABLE;
  hadc5.Init.Overrun               = ADC_OVR_DATA_PRESERVED;
  hadc5.Init.OversamplingMode      = DISABLE;
  if (HAL_ADC_Init(&hadc5) != HAL_OK) Error_Handler();

  sConfig.Channel      = ADC_CHANNEL_1;
  sConfig.Rank         = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLETIME_2CYCLES_5;
  sConfig.SingleDiff   = ADC_SINGLE_ENDED;
  sConfig.OffsetNumber = ADC_OFFSET_NONE;
  sConfig.Offset       = 0;
  if (HAL_ADC_ConfigChannel(&hadc5, &sConfig) != HAL_OK) Error_Handler();
}

static void MX_I2C1_Init(void)
{
  hi2c1.Instance = I2C1;
  hi2c1.Init.Timing           = 0x30A0A7FB;   /* 100 kHz @ 170 MHz */
  hi2c1.Init.OwnAddress1      = 0;
  hi2c1.Init.AddressingMode   = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode  = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2      = 0;
  hi2c1.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
  hi2c1.Init.GeneralCallMode  = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode    = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK) Error_Handler();
  if (HAL_I2CEx_ConfigAnalogFilter(&hi2c1, I2C_ANALOGFILTER_ENABLE) != HAL_OK) Error_Handler();
  if (HAL_I2CEx_ConfigDigitalFilter(&hi2c1, 0) != HAL_OK) Error_Handler();
}

static void MX_LPUART1_UART_Init(void)
{
  hlpuart1.Instance = LPUART1;
  hlpuart1.Init.BaudRate       = 115200;
  hlpuart1.Init.WordLength     = UART_WORDLENGTH_8B;
  hlpuart1.Init.StopBits       = UART_STOPBITS_1;
  hlpuart1.Init.Parity         = UART_PARITY_NONE;
  hlpuart1.Init.Mode           = UART_MODE_TX_RX;
  hlpuart1.Init.HwFlowCtl      = UART_HWCONTROL_NONE;
  hlpuart1.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  hlpuart1.Init.ClockPrescaler = UART_PRESCALER_DIV1;
  hlpuart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&hlpuart1) != HAL_OK) Error_Handler();
  if (HAL_UARTEx_SetTxFifoThreshold(&hlpuart1, UART_TXFIFO_THRESHOLD_1_8) != HAL_OK) Error_Handler();
  if (HAL_UARTEx_SetRxFifoThreshold(&hlpuart1, UART_RXFIFO_THRESHOLD_1_8) != HAL_OK) Error_Handler();
  if (HAL_UARTEx_DisableFifoMode(&hlpuart1) != HAL_OK) Error_Handler();
}

static void MX_USART3_UART_Init(void)
{
  huart3.Instance = USART3;
  huart3.Init.BaudRate       = 115200;
  huart3.Init.WordLength     = UART_WORDLENGTH_8B;
  huart3.Init.StopBits       = UART_STOPBITS_1;
  huart3.Init.Parity         = UART_PARITY_NONE;
  huart3.Init.Mode           = UART_MODE_TX_RX;
  huart3.Init.HwFlowCtl      = UART_HWCONTROL_NONE;
  huart3.Init.OverSampling   = UART_OVERSAMPLING_16;
  huart3.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart3.Init.ClockPrescaler = UART_PRESCALER_DIV1;
  huart3.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart3) != HAL_OK) Error_Handler();
  if (HAL_UARTEx_SetTxFifoThreshold(&huart3, UART_TXFIFO_THRESHOLD_1_8) != HAL_OK) Error_Handler();
  if (HAL_UARTEx_SetRxFifoThreshold(&huart3, UART_RXFIFO_THRESHOLD_1_8) != HAL_OK) Error_Handler();
  if (HAL_UARTEx_DisableFifoMode(&huart3) != HAL_OK) Error_Handler();
}

static void MX_TIM1_Init(void)
{
  TIM_Encoder_InitTypeDef sConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  htim1.Instance = TIM1;
  htim1.Init.Prescaler         = 0;
  htim1.Init.CounterMode       = TIM_COUNTERMODE_UP;
  htim1.Init.Period            = 65535;
  htim1.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
  htim1.Init.RepetitionCounter = 0;
  htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  sConfig.EncoderMode  = TIM_ENCODERMODE_TI1;
  sConfig.IC1Polarity  = TIM_ICPOLARITY_RISING;
  sConfig.IC1Selection = TIM_ICSELECTION_DIRECTTI;
  sConfig.IC1Prescaler = TIM_ICPSC_DIV1;
  sConfig.IC1Filter    = 0;
  sConfig.IC2Polarity  = TIM_ICPOLARITY_RISING;
  sConfig.IC2Selection = TIM_ICSELECTION_DIRECTTI;
  sConfig.IC2Prescaler = TIM_ICPSC_DIV1;
  sConfig.IC2Filter    = 0;
  if (HAL_TIM_Encoder_Init(&htim1, &sConfig) != HAL_OK) Error_Handler();

  sMasterConfig.MasterOutputTrigger  = TIM_TRGO_RESET;
  sMasterConfig.MasterOutputTrigger2 = TIM_TRGO2_RESET;
  sMasterConfig.MasterSlaveMode      = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim1, &sMasterConfig) != HAL_OK) Error_Handler();
}

static void MX_TIM2_Init(void)
{
  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};

  htim2.Instance = TIM2;
  htim2.Init.Prescaler         = 0;
  htim2.Init.CounterMode       = TIM_COUNTERMODE_UP;
  htim2.Init.Period            = 9999;
  htim2.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim2) != HAL_OK) Error_Handler();

  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim2, &sClockSourceConfig) != HAL_OK) Error_Handler();
  if (HAL_TIM_PWM_Init(&htim2) != HAL_OK) Error_Handler();

  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode     = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK) Error_Handler();

  sConfigOC.OCMode     = TIM_OCMODE_PWM1;
  sConfigOC.Pulse      = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_PWM_ConfigChannel(&htim2, &sConfigOC, TIM_CHANNEL_1) != HAL_OK) Error_Handler();
  if (HAL_TIM_PWM_ConfigChannel(&htim2, &sConfigOC, TIM_CHANNEL_2) != HAL_OK) Error_Handler();
  if (HAL_TIM_PWM_ConfigChannel(&htim2, &sConfigOC, TIM_CHANNEL_3) != HAL_OK) Error_Handler();
  if (HAL_TIM_PWM_ConfigChannel(&htim2, &sConfigOC, TIM_CHANNEL_4) != HAL_OK) Error_Handler();

  HAL_TIM_MspPostInit(&htim2);
}

static void MX_TIM3_Init(void)
{
  TIM_Encoder_InitTypeDef sConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  htim3.Instance = TIM3;
  htim3.Init.Prescaler         = 0;
  htim3.Init.CounterMode       = TIM_COUNTERMODE_UP;
  htim3.Init.Period            = 65535;
  htim3.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
  htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  sConfig.EncoderMode  = TIM_ENCODERMODE_TI1;
  sConfig.IC1Polarity  = TIM_ICPOLARITY_RISING;
  sConfig.IC1Selection = TIM_ICSELECTION_DIRECTTI;
  sConfig.IC1Prescaler = TIM_ICPSC_DIV1;
  sConfig.IC1Filter    = 0;
  sConfig.IC2Polarity  = TIM_ICPOLARITY_RISING;
  sConfig.IC2Selection = TIM_ICSELECTION_DIRECTTI;
  sConfig.IC2Prescaler = TIM_ICPSC_DIV1;
  sConfig.IC2Filter    = 0;
  if (HAL_TIM_Encoder_Init(&htim3, &sConfig) != HAL_OK) Error_Handler();

  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode     = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim3, &sMasterConfig) != HAL_OK) Error_Handler();
}

static void MX_TIM4_Init(void)
{
  TIM_Encoder_InitTypeDef sConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  htim4.Instance = TIM4;
  htim4.Init.Prescaler         = 0;
  htim4.Init.CounterMode       = TIM_COUNTERMODE_UP;
  htim4.Init.Period            = 65535;
  htim4.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
  htim4.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  sConfig.EncoderMode  = TIM_ENCODERMODE_TI1;
  sConfig.IC1Polarity  = TIM_ICPOLARITY_RISING;
  sConfig.IC1Selection = TIM_ICSELECTION_DIRECTTI;
  sConfig.IC1Prescaler = TIM_ICPSC_DIV1;
  sConfig.IC1Filter    = 0;
  sConfig.IC2Polarity  = TIM_ICPOLARITY_RISING;
  sConfig.IC2Selection = TIM_ICSELECTION_DIRECTTI;
  sConfig.IC2Prescaler = TIM_ICPSC_DIV1;
  sConfig.IC2Filter    = 0;
  if (HAL_TIM_Encoder_Init(&htim4, &sConfig) != HAL_OK) Error_Handler();

  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode     = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim4, &sMasterConfig) != HAL_OK) Error_Handler();
}

/* TIM5: CH1 = PWM trigger for the HC-SR04 (PB2), CH2 = input capture of the
   echo (PC12). Do not swap the channels: the capture callback above and the
   MSP post-init depend on this assignment. */
static void MX_TIM5_Init(void)
{
  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};
  TIM_IC_InitTypeDef sConfigIC = {0};

  htim5.Instance = TIM5;
  htim5.Init.Prescaler         = 9;
  htim5.Init.CounterMode       = TIM_COUNTERMODE_UP;
  htim5.Init.Period            = 1699999;
  htim5.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
  htim5.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim5) != HAL_OK) Error_Handler();

  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim5, &sClockSourceConfig) != HAL_OK) Error_Handler();
  if (HAL_TIM_PWM_Init(&htim5) != HAL_OK) Error_Handler();
  if (HAL_TIM_IC_Init(&htim5) != HAL_OK) Error_Handler();

  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode     = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim5, &sMasterConfig) != HAL_OK) Error_Handler();

  sConfigOC.OCMode     = TIM_OCMODE_PWM1;
  sConfigOC.Pulse      = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_PWM_ConfigChannel(&htim5, &sConfigOC, TIM_CHANNEL_1) != HAL_OK) Error_Handler();

  sConfigIC.ICPolarity  = TIM_INPUTCHANNELPOLARITY_BOTHEDGE;
  sConfigIC.ICSelection = TIM_ICSELECTION_DIRECTTI;
  sConfigIC.ICPrescaler = TIM_ICPSC_DIV1;
  sConfigIC.ICFilter    = 0;
  if (HAL_TIM_IC_ConfigChannel(&htim5, &sConfigIC, TIM_CHANNEL_2) != HAL_OK) Error_Handler();

  HAL_TIM_MspPostInit(&htim5);
}

static void MX_TIM8_Init(void)
{
  TIM_Encoder_InitTypeDef sConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  htim8.Instance = TIM8;
  htim8.Init.Prescaler         = 0;
  htim8.Init.CounterMode       = TIM_COUNTERMODE_UP;
  htim8.Init.Period            = 65535;
  htim8.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
  htim8.Init.RepetitionCounter = 0;
  htim8.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  sConfig.EncoderMode  = TIM_ENCODERMODE_TI1;
  sConfig.IC1Polarity  = TIM_ICPOLARITY_RISING;
  sConfig.IC1Selection = TIM_ICSELECTION_DIRECTTI;
  sConfig.IC1Prescaler = TIM_ICPSC_DIV1;
  sConfig.IC1Filter    = 0;
  sConfig.IC2Polarity  = TIM_ICPOLARITY_RISING;
  sConfig.IC2Selection = TIM_ICSELECTION_DIRECTTI;
  sConfig.IC2Prescaler = TIM_ICPSC_DIV1;
  sConfig.IC2Filter    = 0;
  if (HAL_TIM_Encoder_Init(&htim8, &sConfig) != HAL_OK) Error_Handler();

  sMasterConfig.MasterOutputTrigger  = TIM_TRGO_RESET;
  sMasterConfig.MasterOutputTrigger2 = TIM_TRGO2_RESET;
  sMasterConfig.MasterSlaveMode      = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim8, &sMasterConfig) != HAL_OK) Error_Handler();
}

static void MX_TIM15_Init(void)
{
  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  htim15.Instance = TIM15;
  htim15.Init.Prescaler         = 67;
  htim15.Init.CounterMode       = TIM_COUNTERMODE_UP;
  htim15.Init.Period            = 49999;
  htim15.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
  htim15.Init.RepetitionCounter = 0;
  htim15.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim15) != HAL_OK) Error_Handler();

  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim15, &sClockSourceConfig) != HAL_OK) Error_Handler();

  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode     = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim15, &sMasterConfig) != HAL_OK) Error_Handler();
}

static void MX_TIM20_Init(void)
{
  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};
  TIM_BreakDeadTimeConfigTypeDef sBreakDeadTimeConfig = {0};

  htim20.Instance = TIM20;
  htim20.Init.Prescaler         = 0;
  htim20.Init.CounterMode       = TIM_COUNTERMODE_UP;
  htim20.Init.Period            = 65535;
  htim20.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
  htim20.Init.RepetitionCounter = 0;
  htim20.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_PWM_Init(&htim20) != HAL_OK) Error_Handler();

  sMasterConfig.MasterOutputTrigger  = TIM_TRGO_RESET;
  sMasterConfig.MasterOutputTrigger2 = TIM_TRGO2_RESET;
  sMasterConfig.MasterSlaveMode      = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim20, &sMasterConfig) != HAL_OK) Error_Handler();

  sConfigOC.OCMode       = TIM_OCMODE_PWM1;
  sConfigOC.Pulse        = 0;
  sConfigOC.OCPolarity   = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCNPolarity  = TIM_OCNPOLARITY_HIGH;
  sConfigOC.OCFastMode   = TIM_OCFAST_DISABLE;
  sConfigOC.OCIdleState  = TIM_OCIDLESTATE_RESET;
  sConfigOC.OCNIdleState = TIM_OCNIDLESTATE_RESET;
  if (HAL_TIM_PWM_ConfigChannel(&htim20, &sConfigOC, TIM_CHANNEL_3) != HAL_OK) Error_Handler();

  sBreakDeadTimeConfig.OffStateRunMode  = TIM_OSSR_DISABLE;
  sBreakDeadTimeConfig.OffStateIDLEMode = TIM_OSSI_DISABLE;
  sBreakDeadTimeConfig.LockLevel        = TIM_LOCKLEVEL_OFF;
  sBreakDeadTimeConfig.DeadTime         = 0;
  sBreakDeadTimeConfig.BreakState       = TIM_BREAK_DISABLE;
  sBreakDeadTimeConfig.BreakPolarity    = TIM_BREAKPOLARITY_HIGH;
  sBreakDeadTimeConfig.BreakFilter      = 0;
  sBreakDeadTimeConfig.BreakAFMode      = TIM_BREAK_AFMODE_INPUT;
  sBreakDeadTimeConfig.Break2State      = TIM_BREAK2_DISABLE;
  sBreakDeadTimeConfig.Break2Polarity   = TIM_BREAK2POLARITY_HIGH;
  sBreakDeadTimeConfig.Break2Filter     = 0;
  sBreakDeadTimeConfig.Break2AFMode     = TIM_BREAK_AFMODE_INPUT;
  sBreakDeadTimeConfig.AutomaticOutput  = TIM_AUTOMATICOUTPUT_DISABLE;
  if (HAL_TIMEx_ConfigBreakDeadTime(&htim20, &sBreakDeadTimeConfig) != HAL_OK) Error_Handler();

  HAL_TIM_MspPostInit(&htim20);
}

static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOF_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /* PC2 = DIR1, PC3 = motor driver enable, PC4 */
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_2 | GPIO_PIN_3 | GPIO_PIN_4, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_RESET);
  /* PB4 = DIR2, PB11 = DIR3, PB13 = DIR4 */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_11 | GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_4
                         | GPIO_PIN_5  | GPIO_PIN_6  | GPIO_PIN_7, GPIO_PIN_RESET);

  GPIO_InitStruct.Pin  = B1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(B1_GPIO_Port, &GPIO_InitStruct);

  GPIO_InitStruct.Pin   = GPIO_PIN_2 | GPIO_PIN_3 | GPIO_PIN_4;
  GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull  = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  GPIO_InitStruct.Pin   = LD2_Pin;
  GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull  = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LD2_GPIO_Port, &GPIO_InitStruct);

  /* PA7 = IR sensor */
  GPIO_InitStruct.Pin  = GPIO_PIN_7;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  GPIO_InitStruct.Pin   = GPIO_PIN_11 | GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_4
                        | GPIO_PIN_5  | GPIO_PIN_6  | GPIO_PIN_7;
  GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull  = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  HAL_NVIC_SetPriority(EXTI15_10_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);
}

/* USER CODE BEGIN 4 */
/* 2. B1 through EXTI: called by HAL on every rising edge of PC13 */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  if (GPIO_Pin == B1_Pin) {
    b1IrqRises++;
    noteGap(edgeGapUs(&b1IrqEdge), &b1IrqShortGaps, &b1IrqMinGapUs);
  }
}
/* USER CODE END 4 */

void Error_Handler(void)
{
  __disable_irq();
  while (1)
  {
  }
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
}
#endif /* USE_FULL_ASSERT */
