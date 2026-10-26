/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   NUCLEO-G474RE integrated hardware test (HAL translation
  *                   of the Arduino sketch "g474_full_car_test").
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32g4xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

void HAL_TIM_MspPostInit(TIM_HandleTypeDef *htim);

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */
/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define B1_Pin GPIO_PIN_13
#define B1_GPIO_Port GPIOC
#define B1_EXTI_IRQn EXTI15_10_IRQn
#define LPUART1_TX_Pin GPIO_PIN_2
#define LPUART1_TX_GPIO_Port GPIOA
#define LPUART1_RX_Pin GPIO_PIN_3
#define LPUART1_RX_GPIO_Port GPIOA
#define LD2_Pin GPIO_PIN_5
#define LD2_GPIO_Port GPIOA
#define T_SWDIO_Pin GPIO_PIN_13
#define T_SWDIO_GPIO_Port GPIOA
#define T_SWCLK_Pin GPIO_PIN_14
#define T_SWCLK_GPIO_Port GPIOA
#define T_SWO_Pin GPIO_PIN_3
#define T_SWO_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* IR digital sensor (Arduino: PA7) */
#define IR_SENSOR_Pin       GPIO_PIN_7
#define IR_SENSOR_GPIO_Port GPIOA

/* Motor direction outputs (Arduino: Dir_1..Dir_4) */
#define DIR1_Pin            GPIO_PIN_2      /* PC2  */
#define DIR1_GPIO_Port      GPIOC
#define DIR2_Pin            GPIO_PIN_4      /* PB4  */
#define DIR2_GPIO_Port      GPIOB
#define DIR3_Pin            GPIO_PIN_11     /* PB11 */
#define DIR3_GPIO_Port      GPIOB
#define DIR4_Pin            GPIO_PIN_13     /* PB13 */
#define DIR4_GPIO_Port      GPIOB

/* !!! Motor enable. Arduino used PC12, but PC12 is now the TIM5_CH2
   ultrasonic trigger. PC3 is assumed here - CHANGE if your PCB differs.
   Free GPIO outputs in the .ioc: PC3, PC4, PB5, PB6, PB7, PB14. */
//#define M_EN_Pin            GPIO_PIN_12
#define M_EN_GPIO_Port      GPIOC

/* HC-SR04:
   ECHO   = PB2  -> TIM5_CH1 input capture, both edges (AF2)
   TRIGGER= PC12 -> TIM5_CH2 PWM output, 10 us pulse every 100 ms (AF1) */
#define US_ECHO_Pin         GPIO_PIN_12
#define US_ECHO_GPIO_Port   GPIOC

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
