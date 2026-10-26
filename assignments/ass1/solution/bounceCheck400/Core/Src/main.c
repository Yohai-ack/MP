/* USER CODE BEGIN Header *//**
  ******************************************************************************
  * @file    main.c   (project bounceCheck400)
  * @brief   Bounce test of I2C button 0 with the I2C bus at 400 kHz, for
  *          Assignment 1, Part C. Hardware: NUCLEO-G474RE + the I2C IO board on
  *          the cart. Read bounceCheck400_instructions.txt (same folder).
  *
  * The question: can Part C show both kinds of wrong count?
  *   - UNWANTED events: bounce makes one real press count as several
  *   - MISSING events : a press shorter than one loop pass is never seen
  *
  * Button 0 (PCA9538A at 0x71) is read as fast as the bus allows, or with a
  * delay of loopDelayMs after every read. For every raw edge the program
  * counts presses and releases separately, measures the gap from the previous
  * edge, and logs its time. A debounced count (level stable for debounceMs)
  * runs alongside as a check of the real number of presses.
  *
  * The bus speed can be switched at run time: i2cFast = 1 (400 kHz, the
  * default here) or 0 (100 kHz, the course setting), to compare the two in
  * the same session.
  *
  * LD2 shows the raw level of button 0 (LD2 on = held).
  *
  * All inputs and outputs are global variables, for Live Expressions:
  *   INPUTS  (write): resetCounts, i2cFast, loopDelayMs, dumpLog, shortGapUs,
  *                    debounceMs
  *   OUTPUTS (read) : everything else listed in the instructions file
  *
  * Nothing moves: the wheels, RGB LEDs and servo are never commanded.
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
/* USER CODE END Includes */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define PCA9538A_ADDR        0x71
#define PCA9538A_INPUT       0x00
#define PCA9538A_CONFIG      0x03
#define GAP_NONE             0xFFFFFFFFUL  /* "no gap measured yet"              */
#define GAP_DEFAULT_US       30000UL       /* 30 ms: shorter gaps are bounce     */
#define DEBOUNCE_DEFAULT_MS  20UL          /* stable time for the debounced count */
#define LOG_SIZE             512U          /* edges kept in the timing log       */
#define I2C_TIMING_100K      0x30A0A7FBUL  /* 100 kHz @ 170 MHz (CubeMX value)   */
#define I2C_TIMING_400K      0x40621236UL  /* 400 kHz @ 170 MHz, CubeMX GUI value (was 0x30C22737) */
#define RAW_EDGE             0x01U         /* updateInput() result bits          */
#define DEB_EDGE             0x02U
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
volatile uint8_t  resetCounts = 0;                    /* write 1: zero all results */
volatile uint8_t  i2cFast     = 1;                    /* 1 = 400 kHz, 0 = 100 kHz  */
volatile uint32_t loopDelayMs = 0;                    /* HAL_Delay after each read */
volatile uint8_t  dumpLog     = 0;                    /* write 1: print the log    */
volatile uint32_t shortGapUs  = GAP_DEFAULT_US;       /* bounce threshold [us]     */
volatile uint32_t debounceMs  = DEBOUNCE_DEFAULT_MS;  /* debounce stable time [ms] */

/* ============================ OUTPUTS (read) ============================== */
volatile uint32_t loopsPerSec = 0;     /* main-loop passes (= reads) per second */
volatile uint32_t samplePeriodUs = 0;  /* time between two reads, 1e6 / loops  */
volatile uint32_t i2cKHz      = 100;   /* I2C speed in use                      */
volatile uint8_t  i2cOk       = 0;     /* 1 = the last button read worked       */
volatile uint32_t i2cErrors   = 0;     /* failed button reads since the reset   */
volatile uint8_t  i2cHeld     = 0;     /* bit n = 1 while button n is held      */

volatile uint32_t b0Presses    = 0;    /* raw edges released -> held            */
volatile uint32_t b0Releases   = 0;    /* raw edges held -> released            */
volatile uint32_t b0ShortGaps  = 0;    /* raw edges after a short gap           */
volatile uint32_t b0MinGapUs   = GAP_NONE;
volatile uint32_t b0DebPresses = 0;    /* debounced presses                     */
volatile uint32_t b0MinHeldUs  = GAP_NONE; /* shortest raw press seen [us]     */

volatile uint32_t logCount     = 0;    /* edges in the log (max LOG_SIZE)       */

/* ---- internal ---- */
typedef struct {          /* one input: raw edge timing and debounced state */
  uint8_t  raw;           /* last raw level                                 */
  uint8_t  seen;          /* 0 until the first raw edge after a reset       */
  uint32_t lastEdgeUs;    /* time of the previous raw edge                  */
  uint8_t  stable;        /* debounced level                                */
  uint32_t changeMs;      /* HAL tick of the last raw change                */
} Input;

typedef struct {          /* one logged raw edge */
  uint32_t tUs;           /* time since the reset [us] */
  uint8_t  level;         /* level after the edge      */
} LogEntry;

static Input    button0;
static LogEntry edgeLog[LOG_SIZE];
static uint8_t  lastFast    = 0;   /* forces one i2cSetSpeed() at start-up  */
static uint64_t clockCycles = 0;   /* DWT cycles since boot, 64-bit         */
static uint32_t lastCyccnt  = 0;
static uint32_t resetUs     = 0;   /* nowUs() at the last reset             */
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
static uint32_t nowUs(void);
static void     i2cSetSpeed(uint8_t fast);
static uint8_t  readI2cButtons(uint8_t *held);
static void     resetResults(void);
static void     logEdge(uint32_t tUs, uint8_t level);
static uint8_t  updateInput(Input *in, uint8_t level);
static void     pollButton0(void);
static void     printLog(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
/* route printf to LPUART1 (PuTTY) */
int __io_putchar(int ch)
{
  HAL_UART_Transmit(&hlpuart1, (uint8_t *)&ch, 1, HAL_MAX_DELAY);
  return ch;
}

/* DWT cycle counter: 170 counts per microsecond */
static void dwtInit(void)
{
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CYCCNT = 0U;
  DWT->CTRL  |= DWT_CTRL_CYCCNTENA_Msk;
}

/* Microseconds since boot. The 32-bit cycle counter wraps every 25 s, so it is
   extended to 64 bits here; the main loop calls this far more often than that. */
static uint32_t nowUs(void)
{
  uint32_t c = DWT->CYCCNT;
  clockCycles += (uint32_t)(c - lastCyccnt);
  lastCyccnt   = c;
  return (uint32_t)(clockCycles / (SystemCoreClock / 1000000U));
}

/* Re-initialise I2C1 at 100 kHz (fast = 0) or 400 kHz (fast = 1).
   The 400 kHz value is the one the CubeMX GUI computes for Fast mode,
   I2CCLK = 170 MHz, PRESC = 4, so one step = 29.4 ns:
     SCLL   = 0x36: SCL low  55 steps = 1.62 us (Fast mode min 1.3 us)
     SCLH   = 0x12: SCL high 19 steps = 0.56 us, plus the synchronisation
                    and analog filter delay (min 0.6 us on the bus)
     SCLDEL = 0x6 : data setup 7 steps = 206 ns (min 100 ns + rise time)
     SDADEL = 0x2 : data hold  2 steps =  59 ns (min 0)
   The first tests used a hand-computed 0x30C22737 (also within the rules). */
static void i2cSetSpeed(uint8_t fast)
{
  HAL_I2C_DeInit(&hi2c1);
  hi2c1.Init.Timing = fast ? I2C_TIMING_400K : I2C_TIMING_100K;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK) Error_Handler();
  if (HAL_I2CEx_ConfigAnalogFilter(&hi2c1, I2C_ANALOGFILTER_ENABLE) != HAL_OK) Error_Handler();
  if (HAL_I2CEx_ConfigDigitalFilter(&hi2c1, 0) != HAL_OK) Error_Handler();
  i2cKHz = fast ? 400U : 100U;
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

/* zero every count and the log; the debounced level is kept */
static void resetResults(void)
{
  i2cErrors = 0;
  b0Presses = 0; b0Releases = 0; b0ShortGaps = 0; b0MinGapUs = GAP_NONE;
  b0DebPresses = 0; b0MinHeldUs = GAP_NONE;
  button0.seen = 0;
  logCount     = 0;
  resetUs      = nowUs();
}

/* keep one raw edge in the log, while there is room */
static void logEdge(uint32_t tUs, uint8_t level)
{
  if (logCount < LOG_SIZE) {
    edgeLog[logCount].tUs   = tUs;
    edgeLog[logCount].level = level;
    logCount++;
  }
}

/* Feed one new sample of button 0. Times the raw edge (short gaps, minimum
   gap, shortest press, log) and runs the debounce: the debounced level
   follows the raw level only after it has been stable for debounceMs.
   Returns RAW_EDGE and/or DEB_EDGE for what changed in this sample. */
static uint8_t updateInput(Input *in, uint8_t level)
{
  uint8_t  events = 0;
  uint32_t tUs    = nowUs() - resetUs;
  uint32_t ms     = HAL_GetTick();

  if (level != in->raw) {                          /* raw edge */
    if (in->seen) {
      uint32_t gap = tUs - in->lastEdgeUs;
      if (gap < shortGapUs)  b0ShortGaps++;
      if (gap < b0MinGapUs)  b0MinGapUs = gap;
      if (!level && gap < b0MinHeldUs) b0MinHeldUs = gap;  /* press -> release */
    }
    in->seen       = 1;
    in->lastEdgeUs = tUs;
    in->raw        = level;
    in->changeMs   = ms;
    logEdge(tUs, level);
    events |= RAW_EDGE;
  }
  if (in->stable != in->raw && (ms - in->changeMs) >= debounceMs) {
    in->stable = in->raw;                          /* debounced edge */
    events |= DEB_EDGE;
  }
  return events;
}

/* button 0: raw presses/releases and debounced presses; LD2 = raw level */
static void pollButton0(void)
{
  uint8_t held;
  if (!readI2cButtons(&held)) { i2cOk = 0; i2cErrors++; return; }
  i2cOk   = 1;
  i2cHeld = held;

  uint8_t level = held & 0x01U;
  HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, level ? GPIO_PIN_SET : GPIO_PIN_RESET);

  uint8_t ev = updateInput(&button0, level);
  if (ev & RAW_EDGE) {
    if (level) b0Presses++; else b0Releases++;
  }
  if ((ev & DEB_EDGE) && button0.stable) b0DebPresses++;
}

/* print the edge log and the counts to PuTTY (blocks for a few seconds) */
static void printLog(void)
{
  printf("\r\n=== LOG: I2C button 0, I2C %lu kHz, loop delay %lu ms, %lu edges%s ===\r\n",
         i2cKHz, loopDelayMs, logCount,
         (logCount >= LOG_SIZE) ? " (log full, later edges not kept)" : "");
  printf("    #      t [ms]   gap [us]  new level\r\n");
  for (uint32_t i = 0; i < logCount; i++) {
    uint32_t t = edgeLog[i].tUs;
    printf("%5lu %7lu.%03lu ", i, t / 1000U, t % 1000U);
    uint8_t isShort = 0;
    if (i == 0) {
      printf("%10s", "-");
    } else {
      uint32_t gap = t - edgeLog[i - 1U].tUs;
      printf("%10lu", gap);
      isShort = (gap < shortGapUs);
    }
    printf("  %s", edgeLog[i].level ? "PRESS  " : "release");
    printf("%s\r\n", isShort ? "  <short" : "");
  }
  printf("button 0: presses %lu, releases %lu, short gaps %lu, min gap %lu us, "
         "debounced presses %lu, shortest press %lu us, I2C errors %lu\r\n",
         b0Presses, b0Releases, b0ShortGaps, b0MinGapUs, b0DebPresses,
         b0MinHeldUs, i2cErrors);
  printf("loops per second %lu, sample period %lu us\r\n=== END OF LOG ===\r\n",
         loopsPerSec, samplePeriodUs);
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
  (void)nowUs();

  /* button chip: all 8 pins as inputs (also its power-on default) */
  uint8_t config = 0xFF;
  HAL_I2C_Mem_Write(&hi2c1, (uint16_t)(PCA9538A_ADDR << 1), PCA9538A_CONFIG,
                    I2C_MEMADD_SIZE_8BIT, &config, 1, 10);

  /* start from the current level, so power-up is not an edge */
  uint8_t held = 0;
  i2cOk = readI2cButtons(&held);
  button0.raw = button0.stable = held & 0x01U;
  resetResults();

  printf("\r\nbounceCheck400 running. Write dumpLog = 1 to print the log.\r\n");

  uint32_t loops = 0, loopWindowStart = HAL_GetTick();
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* inputs written from Live Expressions */
    uint8_t fast = (i2cFast != 0U);
    if (fast != lastFast) {                    /* new bus speed: start over */
      lastFast = fast;
      i2cSetSpeed(fast);
      printf("I2C now %lu kHz, counts reset.\r\n", i2cKHz);
      resetCounts = 1;
    }
    if (resetCounts) {
      resetResults();
      resetCounts = 0;
    }
    if (dumpLog) {
      printLog();
      dumpLog = 0;
    }

    pollButton0();
    if (loopDelayMs) HAL_Delay(loopDelayMs);   /* stands for "other work" */

    loops++;
    uint32_t now = HAL_GetTick();
    if (now - loopWindowStart >= 1000U) {
      loopsPerSec    = loops;
      samplePeriodUs = loops ? (1000000U / loops) : 0U;
      loops = 0;
      loopWindowStart = now;
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
  hi2c1.Init.Timing           = 0x40621236;   /* 400 kHz @ 170 MHz (CubeMX value) */
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
