/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main_hardware_test.c   (use as Core/Src/main.c)
  * @brief          : STATE-CONTROLLED HARDWARE TEST - NUCLEO-G474RE (new_pcb.ioc)
  *
  * One test per value of the global variable 'state'. 'state' is written at
  * runtime by the user (debugger Live Expressions / Watch window; later maybe
  * UART or a MATLAB GUI). Nothing moves until 'state' is changed.
  *
  *   0        idle / everything stopped (value after reset)
  *   1        top motor (servo carrying the ultrasonic) -> MAX     (2100 us)
  *   2        top motor                                -> MIN     ( 900 us)
  *   3        top motor                                -> MIDDLE   (1500 us)
  *   4        ultrasonic sensor: 'distanceUS' = distance in millimetres
  *   5 / 6    wheel 1 forward / backward
  *   7 / 8    wheel 2 forward / backward
  *   9 / 10   wheel 3 forward / backward
  *   11 / 12  wheel 4 forward / backward
  *   13       all 8 RGB LEDs constant white
  *   14       all 8 RGB LEDs blink (0.5 s on, 0.5 s off -> 1 s period)
  *   15       all 8 RGB LEDs fade red -> green -> blue -> white
  *   other    top motor -> MIDDLE (same fall-through as the old servo test)
  *
  * IR sensor (PA7) is tested in every state: the moment it detects an
  * obstacle it forces 'state' = 0 by itself (edge triggered - removing the
  * obstacle does nothing, the user must command a new state). This doubles as
  * an emergency stop while the wheels are running.
  *
  * 'wCounter' is incremented every pass of the while(1) loop - a frozen
  * wCounter means the program is stuck (kept from the servo test file).
  *
  * Hardware (identical to the other main.c files of this project):
  *   Encoder 1..4 : TIM1 (PC0/PC1), TIM8 (PC6/PC7), TIM3 (PA6/PA4),
  *                  TIM4 (PA11/PA12)
  *   Wheel PWM    : TIM2 CH1=PA0, CH2=PA1, CH3=PB10, CH4=PA10 (17 kHz)
  *   Wheel DIR    : DIR1=PC2, DIR2=PB4, DIR3=PB11, DIR4=PB13
  *   HC-SR04      : TIM5 CH1 PWM = TRIG on PB2 (10 us every 100 ms),
  *                  TIM5 CH2 input capture = ECHO on PC12 (both edges)
  *   IR sensor    : PA7 digital input
  *   I2C1         : PB8=SCL, PB9=SDA - PCA9634 x3 (RGB LEDs),
  *                  PCA9685 (RC servos), PCA9538A (buttons, not tested here)
  *   Serial       : LPUART1 @115200 (ST-LINK VCP), LD2 = heartbeat
  *
  * All handles and MX_* init functions are the same as in the other main.c
  * files, so stm32g4xx_hal_msp.c and stm32g4xx_it.c stay unchanged. Only one
  * main.c may be in the build at a time.
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <string.h>
/* USER CODE END Includes */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* ---------------- I2C devices (7-bit addresses) --------------------------- */
#define PCA9634_RED_ADDR      0x01
#define PCA9634_GREEN_ADDR    0x02
#define PCA9634_BLUE_ADDR     0x07
#define PCA9685_ADDR          0x44
#define PCA9538A_ADDR         0x71

#define PCA9634_MODE1         0x00
#define PCA9634_MODE2         0x01
#define PCA9634_PWM0          0x02
#define PCA9634_LEDOUT0       0x0C
#define PCA9634_LEDOUT1       0x0D

#define PCA9685_MODE1         0x00
#define PCA9685_MODE2         0x01
#define PCA9685_LED0_ON_L     0x06
#define PCA9685_PRESCALE      0xFE

/* ---------------- top motor = RC servo holding the ultrasonic -------------- */
#define RC_PWM_MIN_US          900    /* minimum rotation  */
#define RC_PWM_CENTER_US      1500    /* middle, 0 degrees */
#define RC_PWM_MAX_US         2100    /* maximum rotation  */

#define PCA9685_FIRST_CHANNEL 0
#define PCA9685_CHANNEL_COUNT 4

#define TOP_INDEX_NONE        0xFFU   /* sentinel: nothing applied yet */

/* ---------------- wheels --------------------------------------------------- */
#define WHEEL_COUNT              4U
#define MOTOR_PWM_DEFAULT       97U   /* of 255, ~38 % - as in the car test  */
#define TIM2_PERIOD           9999U   /* must match MX_TIM2_Init             */

/* ---------------- RGB LEDs ------------------------------------------------- */
#define LED_COUNT                8U
#define LED_ON_LEVEL           255U   /* PCA9634 PWM value for "fully on"    */
#define LED_BLINK_HALF_MS      500UL  /* toggle every 500 ms -> 1 s period   */
#define RGB_FADE_INTERVAL_MS    30UL
#define RGB_SERVICE_INTERVAL_US 1200UL /* one I2C register per service call  */

/* ---------------- ultrasonic (TIM5) --------------------------------------- */
#define US_TIM_PERIOD       1699999UL  /* must match MX_TIM5_Init (100 ms)   */
#define US_TICKS_PER_US          17UL  /* 170 MHz / (PSC 9 + 1)              */
#define US_TRIG_TICKS           170UL  /* 10 us trigger pulse                */
#define ULTRASONIC_STALE_MS     300UL  /* no echo for this long -> no echo   */

/* ---------------- IR sensor ------------------------------------------------ */
/* Most of these modules pull their output LOW when they see an obstacle.
 * If state does NOT jump to 0 when you block the sensor, but does jump when
 * you clear it, change this to GPIO_PIN_SET. 'irLevel' shows the raw pin. */
#define IR_OBSTACLE_LEVEL     GPIO_PIN_RESET

/* ---------------- timing --------------------------------------------------- */
#define STATUS_PRINT_INTERVAL_MS 500UL

/* ---------------- state numbers ------------------------------------------- */
#define ST_IDLE                0
#define ST_TOP_MAX             1
#define ST_TOP_MIN             2
#define ST_TOP_MIDDLE          3
#define ST_US_DISTANCE         4
#define ST_WHEEL_FIRST         5      /* 5..12: wheel 1 fwd, 1 bwd, 2 fwd... */
#define ST_WHEEL_LAST         12
#define ST_LED_CONSTANT       13
#define ST_LED_BLINK          14
#define ST_LED_FADE           15
#define ST_LAST_DEFINED       15

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

/* ---- variables the user watches / writes in the debugger ----------------- */

/* Commanded by the user at runtime (see ST_* above). volatile: it changes
 * asynchronously w.r.t. the code that reads it (a debugger write now, the IR
 * sensor routine below, maybe a UART Rx interrupt later). */
volatile uint8_t  state = 0;

/* Incremented every loop pass - the simplest "is it alive" indicator. */
uint32_t wCounter = 0;

/* Ultrasonic distance in MILLIMETRES, updated only in state 4.
 * 0 means "no echo" (nothing in range / sensor not answering). */
volatile uint32_t distanceUS = 0;

/* Wheel PWM duty in the 0...255 scale. May be changed live in the debugger
 * while a wheel test is running (e.g. lower it for a wheel that slips). */
volatile uint8_t  motorPwm = MOTOR_PWM_DEFAULT;

/* IR sensor: raw pin level and the decoded obstacle flag. */
volatile uint8_t  irLevel    = 0;
volatile uint8_t  irObstacle = 0;

/* Wheel encoders, 32-bit accumulation of the 16-bit hardware counters.
 * encoderPos[0..3] belong to wheels 1..4. */
int32_t encoderPos[WHEEL_COUNT] = {0};

/* ---- internal state ------------------------------------------------------ */
static const uint16_t topPositionsUs[3] = {
  RC_PWM_MAX_US,      /* maximum rotation */
  RC_PWM_CENTER_US,   /* middle (0 deg)   */
  RC_PWM_MIN_US       /* minimum rotation */
};
static const char *topPositionNames[3] = {
  "MAX (+, 2100 us)",
  "MIDDLE (0 deg, 1500 us)",
  "MIN (-, 900 us)"
};

/* Position currently applied to the top motor (index into topPositionsUs[]). */
static uint8_t  topPositionIndex = TOP_INDEX_NONE;
static uint16_t topPulseUs       = 0;

/* Last value of 'state' that was actually acted upon (-1 = nothing yet). */
static int16_t  appliedState = -1;

/* Wheel hardware tables, index 0..3 = wheel 1..4 */
static const uint32_t wheelPwmChannel[WHEEL_COUNT] = {
  TIM_CHANNEL_1, TIM_CHANNEL_2, TIM_CHANNEL_3, TIM_CHANNEL_4
};
static GPIO_TypeDef * const wheelDirPort[WHEEL_COUNT] = {
  DIR1_GPIO_Port, DIR2_GPIO_Port, DIR3_GPIO_Port, DIR4_GPIO_Port
};
static const uint16_t wheelDirPin[WHEEL_COUNT] = {
  DIR1_Pin, DIR2_Pin, DIR3_Pin, DIR4_Pin
};
/* DIR level that drives each wheel "forward". The two sides of the car are
 * wired mirrored, hence the SET/RESET/RESET/SET pattern (same as the car
 * test). What the test really checks is that the wheel turns one way in the
 * "forward" state and the other way in the "backward" state. */
static const GPIO_PinState wheelForwardLevel[WHEEL_COUNT] = {
  GPIO_PIN_SET, GPIO_PIN_RESET, GPIO_PIN_RESET, GPIO_PIN_SET
};

static uint16_t encLast[WHEEL_COUNT] = {0};

/* RGB LEDs: the target values are written to the PCA9634s one register per
 * service call, so LED I2C traffic never blocks the UART or the sensors. */
static uint8_t  rgbTargetRed[LED_COUNT]   = {0};
static uint8_t  rgbTargetGreen[LED_COUNT] = {0};
static uint8_t  rgbTargetBlue[LED_COUNT]  = {0};
static uint8_t  rgbServiceIndex  = 0;
static uint32_t lastRgbServiceUs = 0;

static uint8_t  ledBlinkOn      = 0;
static uint32_t lastLedBlinkMs  = 0;
static uint8_t  rgbStage        = 0;
static int16_t  rgbBrightness   = 0;
static int8_t   rgbStep         = 3;
static uint32_t lastRgbFadeMs   = 0;

/* Ultrasonic - written by the TIM5 capture ISR */
static volatile uint32_t usEchoRiseTicks = 0;
static volatile uint8_t  usEchoRising    = 0;
static volatile uint32_t usPulseUs       = 0;
static volatile uint8_t  usEverValid     = 0;
static volatile uint32_t usLastValidTick = 0;

static uint32_t lastStatusPrintMs = 0;
static char     printBuf[256];

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
static uint32_t micros(void);
static void     serialPrint(const char *s);
static uint8_t  checkI2CDevice(uint8_t address);
static uint8_t  writeRegister(uint8_t address, uint8_t reg, uint8_t value);
/* top motor (PCA9685) */
static void     pca9685SetPwm(uint8_t channel, uint16_t onCount, uint16_t offCount);
static void     pca9685SetPulseUs(uint8_t channel, uint16_t pulseUs);
static void     setAllTopMotors(uint16_t pulseUs);
static void     pca9685Init50Hz(void);
static void     applyTopMotorPosition(uint8_t index);
static int8_t   mapStateToTopIndex(uint8_t stateValue);
/* RGB LEDs (PCA9634 x3) */
static void     pca9634Init(uint8_t address);
static void     setLedChannel(uint8_t address, uint8_t channel, uint8_t brightness);
static void     setAllChannels(uint8_t address, uint8_t brightness);
static void     setAllRgbTargets(uint8_t red, uint8_t green, uint8_t blue);
static void     allLedsOffNow(void);
static void     serviceRgbOutputs(void);
static void     ledStartupTest(void);
static void     updateLedBlink(void);
static void     updateLedFade(void);
/* wheels */
static void     setWheelPwm(uint8_t wheel, uint8_t value255);
static void     stopAllWheels(void);
static void     startWheel(uint8_t wheel, uint8_t forward);
static void     updateEncoders(void);
/* sensors */
static void     updateIrSensor(void);
static void     updateDistanceUS(void);
/* state machine */
static void     enterState(uint8_t stateValue);
static void     runStateTick(uint8_t stateValue);
static void     printStateTable(void);
static void     printPeriodicStatus(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* ------------------------------------------------------------------ */
/* micro-second time base (DWT cycle counter), used only by the LED    */
/* I2C traffic scheduler                                               */
/* ------------------------------------------------------------------ */
static void dwtInit(void)
{
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CYCCNT = 0U;
  DWT->CTRL  |= DWT_CTRL_CYCCNTENA_Msk;
}

static uint32_t micros(void)
{
  return DWT->CYCCNT / (SystemCoreClock / 1000000U);
}

static void serialPrint(const char *s)
{
  HAL_UART_Transmit(&hlpuart1, (uint8_t *)s, (uint16_t)strlen(s), 100);
}

/* ------------------------------------------------------------------ */
/* I2C helpers                                                         */
/* ------------------------------------------------------------------ */
static uint8_t checkI2CDevice(uint8_t address)
{
  HAL_StatusTypeDef status = HAL_I2C_IsDeviceReady(&hi2c1, (uint16_t)(address << 1), 2, 10);
  snprintf(printBuf, sizeof(printBuf), "I2C 0x%02X : %s\r\n",
           address, (status == HAL_OK) ? "FOUND" : "NOT FOUND");
  serialPrint(printBuf);
  return (status == HAL_OK);
}

static uint8_t writeRegister(uint8_t address, uint8_t reg, uint8_t value)
{
  if (HAL_I2C_Mem_Write(&hi2c1, (uint16_t)(address << 1),
                        reg, I2C_MEMADD_SIZE_8BIT, &value, 1, 10) != HAL_OK) {
    snprintf(printBuf, sizeof(printBuf),
             "I2C write error: addr=0x%02X reg=0x%02X\r\n", address, reg);
    serialPrint(printBuf);
    return 0;
  }
  return 1;
}

/* ------------------------------------------------------------------ */
/* Top motor: RC servos on the PCA9685, CH0...CH3                      */
/* ------------------------------------------------------------------ */
static void pca9685SetPwm(uint8_t channel, uint16_t onCount, uint16_t offCount)
{
  if (channel >= 16) return;
  uint8_t firstRegister = PCA9685_LED0_ON_L + 4 * channel;
  uint8_t data[4];
  data[0] = (uint8_t)(onCount & 0xFF);
  data[1] = (uint8_t)((onCount >> 8) & 0x0F);
  data[2] = (uint8_t)(offCount & 0xFF);
  data[3] = (uint8_t)((offCount >> 8) & 0x0F);
  if (HAL_I2C_Mem_Write(&hi2c1, (uint16_t)(PCA9685_ADDR << 1),
                        firstRegister, I2C_MEMADD_SIZE_8BIT, data, 4, 10) != HAL_OK) {
    serialPrint("PCA9685 write error\r\n");
  }
}

static void pca9685SetPulseUs(uint8_t channel, uint16_t pulseUs)
{
  if (pulseUs < RC_PWM_MIN_US) pulseUs = RC_PWM_MIN_US;
  if (pulseUs > RC_PWM_MAX_US) pulseUs = RC_PWM_MAX_US;
  /* 50 Hz -> 20000 us frame, 4096 counts per frame */
  uint16_t ticks = (uint16_t)(((uint32_t)pulseUs * 4096UL) / 20000UL);
  pca9685SetPwm(channel, 0, ticks);
}

static void setAllTopMotors(uint16_t pulseUs)
{
  topPulseUs = pulseUs;
  for (uint8_t channel = PCA9685_FIRST_CHANNEL;
       channel < PCA9685_FIRST_CHANNEL + PCA9685_CHANNEL_COUNT;
       channel++) {
    pca9685SetPulseUs(channel, pulseUs);
  }
}

static void pca9685Init50Hz(void)
{
  writeRegister(PCA9685_ADDR, PCA9685_MODE1, 0x00);
  HAL_Delay(10);
  writeRegister(PCA9685_ADDR, PCA9685_MODE1, 0x10);   /* sleep            */
  HAL_Delay(5);
  writeRegister(PCA9685_ADDR, PCA9685_PRESCALE, 121); /* 50 Hz            */
  writeRegister(PCA9685_ADDR, PCA9685_MODE1, 0x00);   /* wake             */
  HAL_Delay(10);
  writeRegister(PCA9685_ADDR, PCA9685_MODE1, 0xA1);   /* restart, AI      */
  writeRegister(PCA9685_ADDR, PCA9685_MODE2, 0x04);   /* totem-pole       */
}

static void applyTopMotorPosition(uint8_t index)
{
  setAllTopMotors(topPositionsUs[index]);
  snprintf(printBuf, sizeof(printBuf), "top motor CH0...CH%d -> %s\r\n",
           PCA9685_CHANNEL_COUNT - 1, topPositionNames[index]);
  serialPrint(printBuf);
}

/* Translates 'state' into an index of topPositionsUs[], or -1 when this state
 * must not touch the top motor (idle, or one of the other hardware tests).
 * Values above the last defined test behave as in the original servo test
 * file: they are treated as MIDDLE. */
static int8_t mapStateToTopIndex(uint8_t stateValue)
{
  switch (stateValue)
  {
    case ST_IDLE:       return -1;  /* do nothing                         */
    case ST_TOP_MAX:    return 0;   /* -> topPositionsUs[0] MAX            */
    case ST_TOP_MIN:    return 2;   /* -> topPositionsUs[2] MIN            */
    case ST_TOP_MIDDLE: return 1;   /* -> topPositionsUs[1] MIDDLE         */
    default:
      if (stateValue <= ST_LAST_DEFINED) return -1;  /* another test       */
      return 1;                                      /* unknown -> MIDDLE  */
  }
}

/* ------------------------------------------------------------------ */
/* RGB LEDs: 8 LEDs, one PCA9634 per colour                            */
/* ------------------------------------------------------------------ */
static void pca9634Init(uint8_t address)
{
  writeRegister(address, PCA9634_MODE1, 0x00);
  writeRegister(address, PCA9634_MODE2, 0x04);
  writeRegister(address, PCA9634_LEDOUT0, 0xAA);
  writeRegister(address, PCA9634_LEDOUT1, 0xAA);
  for (uint8_t channel = 0; channel < LED_COUNT; channel++) {
    writeRegister(address, PCA9634_PWM0 + channel, 0);
  }
}

static void setLedChannel(uint8_t address, uint8_t channel, uint8_t brightness)
{
  if (channel >= LED_COUNT) return;
  writeRegister(address, PCA9634_PWM0 + channel, brightness);
}

static void setAllChannels(uint8_t address, uint8_t brightness)
{
  for (uint8_t channel = 0; channel < LED_COUNT; channel++) {
    setLedChannel(address, channel, brightness);
  }
}

static void setAllRgbTargets(uint8_t red, uint8_t green, uint8_t blue)
{
  for (uint8_t channel = 0; channel < LED_COUNT; channel++) {
    rgbTargetRed[channel]   = red;
    rgbTargetGreen[channel] = green;
    rgbTargetBlue[channel]  = blue;
  }
}

static void allLedsOffNow(void)
{
  setAllRgbTargets(0, 0, 0);
  setAllChannels(PCA9634_RED_ADDR, 0);
  setAllChannels(PCA9634_GREEN_ADDR, 0);
  setAllChannels(PCA9634_BLUE_ADDR, 0);
}

/* Only one PCA9634 register is written per call, so the LED traffic never
   blocks the UART or the sensors. 24 registers -> full refresh in ~29 ms. */
static void serviceRgbOutputs(void)
{
  uint32_t nowUs = micros();

  if ((uint32_t)(nowUs - lastRgbServiceUs) < RGB_SERVICE_INTERVAL_US) return;
  lastRgbServiceUs = nowUs;

  uint8_t channel = rgbServiceIndex % LED_COUNT;
  uint8_t bank    = rgbServiceIndex / LED_COUNT;

  if (bank == 0) {
    setLedChannel(PCA9634_RED_ADDR, channel, rgbTargetRed[channel]);
  } else if (bank == 1) {
    setLedChannel(PCA9634_GREEN_ADDR, channel, rgbTargetGreen[channel]);
  } else {
    setLedChannel(PCA9634_BLUE_ADDR, channel, rgbTargetBlue[channel]);
  }

  rgbServiceIndex++;
  if (rgbServiceIndex >= 3 * LED_COUNT) rgbServiceIndex = 0;
}

/* Short red/green/blue flash of LED 0 at power-up: proves the three drivers
   answer on the I2C bus before any state is commanded. */
static void ledStartupTest(void)
{
  allLedsOffNow();
  setLedChannel(PCA9634_RED_ADDR, 0, LED_ON_LEVEL);
  HAL_Delay(200);
  setLedChannel(PCA9634_RED_ADDR, 0, 0);
  setLedChannel(PCA9634_GREEN_ADDR, 0, LED_ON_LEVEL);
  HAL_Delay(200);
  setLedChannel(PCA9634_GREEN_ADDR, 0, 0);
  setLedChannel(PCA9634_BLUE_ADDR, 0, LED_ON_LEVEL);
  HAL_Delay(200);
  allLedsOffNow();
}

/* state 14: all LEDs white, on/off every LED_BLINK_HALF_MS */
static void updateLedBlink(void)
{
  uint32_t now = HAL_GetTick();
  if (now - lastLedBlinkMs < LED_BLINK_HALF_MS) return;
  lastLedBlinkMs = now;

  ledBlinkOn = !ledBlinkOn;
  uint8_t value = ledBlinkOn ? LED_ON_LEVEL : 0;
  setAllRgbTargets(value, value, value);
}

/* state 15: all LEDs fade up and down in red, then green, then blue,
   then white, and start over */
static void updateLedFade(void)
{
  uint32_t now = HAL_GetTick();
  if (now - lastRgbFadeMs < RGB_FADE_INTERVAL_MS) return;
  lastRgbFadeMs = now;

  rgbBrightness += rgbStep;

  if (rgbBrightness >= (int16_t)LED_ON_LEVEL) {
    rgbBrightness = (int16_t)LED_ON_LEVEL;
    rgbStep = -3;
  } else if (rgbBrightness <= 0) {
    rgbBrightness = 0;
    rgbStep = 3;
    rgbStage++;
    if (rgbStage > 3) rgbStage = 0;
  }

  uint8_t value = (uint8_t)rgbBrightness;

  switch (rgbStage) {
    case 0:  setAllRgbTargets(value, 0, 0);         break;
    case 1:  setAllRgbTargets(0, value, 0);         break;
    case 2:  setAllRgbTargets(0, 0, value);         break;
    default: setAllRgbTargets(value, value, value); break;
  }
}

/* ------------------------------------------------------------------ */
/* Wheels: TIM2 PWM (0...255 scale) + one DIR pin per wheel            */
/* ------------------------------------------------------------------ */
static void setWheelPwm(uint8_t wheel, uint8_t value255)
{
  if (wheel >= WHEEL_COUNT) return;
  uint32_t compare = ((uint32_t)value255 * (TIM2_PERIOD + 1U)) / 255U;
  __HAL_TIM_SET_COMPARE(&htim2, wheelPwmChannel[wheel], compare);
}

static void stopAllWheels(void)
{
  for (uint8_t wheel = 0; wheel < WHEEL_COUNT; wheel++) {
    setWheelPwm(wheel, 0);
  }
}

static void startWheel(uint8_t wheel, uint8_t forward)
{
  if (wheel >= WHEEL_COUNT) return;
  GPIO_PinState level = wheelForwardLevel[wheel];
  if (!forward) {
    level = (level == GPIO_PIN_SET) ? GPIO_PIN_RESET : GPIO_PIN_SET;
  }
  HAL_GPIO_WritePin(wheelDirPort[wheel], wheelDirPin[wheel], level);
  setWheelPwm(wheel, motorPwm);
}

/* Hardware quadrature counters, accumulated to 32 bits */
static void updateEncoders(void)
{
  static TIM_HandleTypeDef * const encoderTimer[WHEEL_COUNT] = { &htim1, &htim8, &htim3, &htim4 };

  for (uint8_t wheel = 0; wheel < WHEEL_COUNT; wheel++) {
    uint16_t c = (uint16_t)__HAL_TIM_GET_COUNTER(encoderTimer[wheel]);
    encoderPos[wheel] += (int16_t)(uint16_t)(c - encLast[wheel]);
    encLast[wheel] = c;
  }
}

/* ------------------------------------------------------------------ */
/* IR sensor (PA7): a NEW obstacle forces state = 0                    */
/* ------------------------------------------------------------------ */
static void updateIrSensor(void)
{
  static uint8_t previousObstacle = 0;

  /* read the pin once, so the raw level and the decoded flag always agree */
  GPIO_PinState pin = HAL_GPIO_ReadPin(IR_SENSOR_GPIO_Port, IR_SENSOR_Pin);
  irLevel    = (pin == GPIO_PIN_SET);
  irObstacle = (pin == IR_OBSTACLE_LEVEL);

  /* Edge triggered on purpose: removing the obstacle does nothing, so the
     user must command a new state to continue testing. */
  if (irObstacle && !previousObstacle) {
    state = ST_IDLE;
    serialPrint("IR sensor: OBSTACLE detected -> state = 0 (all stopped)\r\n");
  }
  previousObstacle = irObstacle;
}

/* ------------------------------------------------------------------ */
/* HC-SR04: TIM5_CH2 input capture callback (both edges of ECHO, PC12) */
/* ------------------------------------------------------------------ */
void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim)
{
  if (htim->Instance == TIM5 && htim->Channel == HAL_TIM_ACTIVE_CHANNEL_2) {
    uint32_t capture = HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_2);

    if (HAL_GPIO_ReadPin(US_ECHO_GPIO_Port, US_ECHO_Pin) == GPIO_PIN_SET) {
      /* rising edge of echo */
      usEchoRiseTicks = capture;
      usEchoRising = 1;
    } else if (usEchoRising) {
      /* falling edge of echo */
      uint32_t ticks = (capture >= usEchoRiseTicks)
                     ? (capture - usEchoRiseTicks)
                     : (capture + (US_TIM_PERIOD + 1UL) - usEchoRiseTicks);
      usPulseUs = ticks / US_TICKS_PER_US;
      usEverValid = 1;
      usLastValidTick = HAL_GetTick();
      usEchoRising = 0;
    }
  }
}

/* state 4: distanceUS [mm], 0 = no echo. The measurement itself runs in
   hardware all the time (TIM5 PWM trigger + capture interrupt). */
static void updateDistanceUS(void)
{
  uint32_t now = HAL_GetTick();

  if (usEverValid && ((now - usLastValidTick) <= ULTRASONIC_STALE_MS)) {
    distanceUS = (usPulseUs * 343UL) / 2000UL;   /* us * 0.343 / 2 -> mm */
  } else {
    distanceUS = 0;                              /* no echo               */
  }
}

/* ------------------------------------------------------------------ */
/* State machine                                                       */
/* ------------------------------------------------------------------ */

/* Called once, whenever the user (or the IR sensor) changed 'state'.
   Everything from the previous test is stopped first; the top motor keeps
   the position it was last commanded to. */
static void enterState(uint8_t stateValue)
{
  stopAllWheels();
  setAllRgbTargets(0, 0, 0);

  int8_t topIndex = mapStateToTopIndex(stateValue);
  if ((topIndex >= 0) && ((uint8_t)topIndex != topPositionIndex)) {
    topPositionIndex = (uint8_t)topIndex;
    applyTopMotorPosition(topPositionIndex);
  }

  if ((stateValue >= ST_WHEEL_FIRST) && (stateValue <= ST_WHEEL_LAST)) {
    uint8_t wheel   = (uint8_t)((stateValue - ST_WHEEL_FIRST) / 2U);
    uint8_t forward = (uint8_t)(((stateValue - ST_WHEEL_FIRST) % 2U) == 0U);
    startWheel(wheel, forward);
    snprintf(printBuf, sizeof(printBuf),
             "state=%u : WHEEL %u %s, PWM=%u/255\r\n",
             stateValue, (unsigned)(wheel + 1U),
             forward ? "FORWARD" : "BACKWARD", (unsigned)motorPwm);
    serialPrint(printBuf);
    return;
  }

  switch (stateValue)
  {
    case ST_IDLE:
      serialPrint("state=0 : IDLE - wheels stopped, LEDs off\r\n");
      break;

    case ST_US_DISTANCE:
      distanceUS = 0;
      serialPrint("state=4 : ULTRASONIC - watch 'distanceUS' [mm], 0 = no echo\r\n");
      break;

    case ST_LED_CONSTANT:
      setAllRgbTargets(LED_ON_LEVEL, LED_ON_LEVEL, LED_ON_LEVEL);
      serialPrint("state=13 : LEDs constant white (all 8)\r\n");
      break;

    case ST_LED_BLINK:
      ledBlinkOn     = 1;
      lastLedBlinkMs = HAL_GetTick();
      setAllRgbTargets(LED_ON_LEVEL, LED_ON_LEVEL, LED_ON_LEVEL);
      serialPrint("state=14 : LEDs blinking, 1 s period\r\n");
      break;

    case ST_LED_FADE:
      rgbStage      = 0;
      rgbBrightness = 0;
      rgbStep       = 3;
      lastRgbFadeMs = HAL_GetTick();
      serialPrint("state=15 : LEDs fading red -> green -> blue -> white\r\n");
      break;

    default:
      /* states 1, 2, 3 were already handled by the top motor code above;
         anything above 15 fell through to MIDDLE. */
      if (stateValue > ST_LAST_DEFINED) {
        snprintf(printBuf, sizeof(printBuf),
                 "state=%u : not a test - top motor -> MIDDLE\r\n", stateValue);
        serialPrint(printBuf);
      }
      break;
  }
}

/* Called every loop pass for the state that is currently active. */
static void runStateTick(uint8_t stateValue)
{
  if ((stateValue >= ST_WHEEL_FIRST) && (stateValue <= ST_WHEEL_LAST)) {
    /* re-applied every pass so a live change of 'motorPwm' takes effect */
    setWheelPwm((uint8_t)((stateValue - ST_WHEEL_FIRST) / 2U), motorPwm);
    return;
  }

  switch (stateValue)
  {
    case ST_US_DISTANCE:  updateDistanceUS(); break;
    case ST_LED_BLINK:    updateLedBlink();   break;
    case ST_LED_FADE:     updateLedFade();    break;
    default:                                  break;  /* nothing to do */
  }
}

static void printStateTable(void)
{
  serialPrint("state values:\r\n");
  serialPrint("  0        idle / stop everything\r\n");
  serialPrint("  1  2  3  top motor MAX / MIN / MIDDLE\r\n");
  serialPrint("  4        ultrasonic -> variable distanceUS [mm]\r\n");
  serialPrint("  5  6     wheel 1 forward / backward\r\n");
  serialPrint("  7  8     wheel 2 forward / backward\r\n");
  serialPrint("  9  10    wheel 3 forward / backward\r\n");
  serialPrint("  11 12    wheel 4 forward / backward\r\n");
  serialPrint("  13       LEDs constant white\r\n");
  serialPrint("  14       LEDs blinking (1 s)\r\n");
  serialPrint("  15       LEDs fading colours\r\n");
  serialPrint("  other    top motor -> MIDDLE\r\n");
  serialPrint("IR obstacle at any time -> state = 0\r\n");
}

static void printPeriodicStatus(void)
{
  uint32_t now = HAL_GetTick();
  if (now - lastStatusPrintMs < STATUS_PRINT_INTERVAL_MS) return;
  lastStatusPrintMs = now;

  HAL_GPIO_TogglePin(LD2_GPIO_Port, LD2_Pin);   /* heartbeat */

  snprintf(printBuf, sizeof(printBuf),
           "state=%u wCounter=%lu | IR pin=%u obst=%u | distanceUS=%lu mm"
           " | top=%u us | ENC1..4 = %ld %ld %ld %ld | pwm=%u\r\n",
           (unsigned)state, (unsigned long)wCounter,
           (unsigned)irLevel, (unsigned)irObstacle,
           (unsigned long)distanceUS, (unsigned)topPulseUs,
           (long)encoderPos[0], (long)encoderPos[1],
           (long)encoderPos[2], (long)encoderPos[3],
           (unsigned)motorPwm);
  serialPrint(printBuf);
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

  serialPrint("\r\n======================================\r\n");
  serialPrint("NUCLEO-G474RE HARDWARE TEST - STATE CONTROLLED\r\n");
  serialPrint("======================================\r\n");
  printStateTable();
  serialPrint("======================================\r\n");

  /* --- encoders: hardware quadrature counters --- */
  HAL_TIM_Encoder_Start(&htim1, TIM_CHANNEL_ALL);
  HAL_TIM_Encoder_Start(&htim8, TIM_CHANNEL_ALL);
  HAL_TIM_Encoder_Start(&htim3, TIM_CHANNEL_ALL);
  HAL_TIM_Encoder_Start(&htim4, TIM_CHANNEL_ALL);
  encLast[0] = (uint16_t)__HAL_TIM_GET_COUNTER(&htim1);
  encLast[1] = (uint16_t)__HAL_TIM_GET_COUNTER(&htim8);
  encLast[2] = (uint16_t)__HAL_TIM_GET_COUNTER(&htim3);
  encLast[3] = (uint16_t)__HAL_TIM_GET_COUNTER(&htim4);

  /* --- wheels: PWM channels running at duty 0, DIR pins low --- */
  /* If no wheel ever turns, the motor driver enable may need to be driven:
     HAL_GPIO_WritePin(GPIOC, GPIO_PIN_3, GPIO_PIN_SET);   (M_EN = PC3)
     It is left as configured by MX_GPIO_Init, exactly like the car test. */
  stopAllWheels();
  HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1);
  HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_2);
  HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_3);
  HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_4);

  /* --- HC-SR04: capture first, then the auto-repeating 10 us trigger --- */
  HAL_TIM_IC_Start_IT(&htim5, TIM_CHANNEL_2);              /* echo, PC12 */
  __HAL_TIM_SET_COMPARE(&htim5, TIM_CHANNEL_1, US_TRIG_TICKS);
  HAL_TIM_PWM_Start(&htim5, TIM_CHANNEL_1);                /* trig, PB2  */

  /* --- I2C devices --- */
  serialPrint("I2C started: SDA=PB9, SCL=PB8\r\n");
  checkI2CDevice(PCA9634_RED_ADDR);
  checkI2CDevice(PCA9634_GREEN_ADDR);
  checkI2CDevice(PCA9634_BLUE_ADDR);
  checkI2CDevice(PCA9685_ADDR);
  checkI2CDevice(PCA9538A_ADDR);

  pca9634Init(PCA9634_RED_ADDR);
  pca9634Init(PCA9634_GREEN_ADDR);
  pca9634Init(PCA9634_BLUE_ADDR);
  pca9685Init50Hz();
  ledStartupTest();
  lastRgbServiceUs = micros();

  /* state == 0 at reset -> nothing is commanded here. The top motor is not
   * moved either: it keeps whatever position it powered up in, exactly like
   * the original servo test. */
  serialPrint("======================================\r\n");
  serialPrint("PROGRAM STARTED - set 'state' to run a test\r\n");
  serialPrint("======================================\r\n");
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    wCounter++;

    updateEncoders();
    updateIrSensor();          /* may set state = 0 by itself */

    /* React only when the commanded state actually changed, so a running
       test is not restarted on every loop pass. */
    if ((int16_t)state != appliedState) {
      appliedState = (int16_t)state;
      enterState((uint8_t)appliedState);
    }

    runStateTick((uint8_t)appliedState);
    serviceRgbOutputs();
    printPeriodicStatus();
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

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  if (GPIO_Pin == B1_Pin) {
    /* blue user button - unused in this test */
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
