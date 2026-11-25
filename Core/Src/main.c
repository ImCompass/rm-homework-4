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
#include "i2c.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "oled.h"
#include "font.h"
#include <stdio.h>
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

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
/* 角度 → CCR
 * 0° = 500us，180° = 2500us
 * 因为 PSC/ARR 已配成 1 个计数 = 1us，所以 CCR 数值 = 脉宽(us)
 */
uint16_t AngleToCCR(float angle)
{
    if (angle < 0.0f)   angle = 0.0f;
    if (angle > 180.0f) angle = 180.0f;
    /* TODO 1：两端各留 2° 余量，把 0 和 180 换成 2 和 178 */
    if (angle < 2.0f)   angle = 2.0f;
    if (angle > 178.0f) angle = 178.0f;

    return (uint16_t)(500.0f + angle / 180.0f * 2000.0f);
}

void Servo_SetAngle(float angle)
{
    uint16_t ccr = AngleToCCR(angle);
    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, ccr);
}

int8_t Key_Scan(void)
{
    static uint8_t add_last = 1;   /* 加键上次稳定电平：1 = 松开（上拉） */
    static uint8_t sub_last = 1;   /* 减键上次稳定电平：1 = 松开（上拉） */

    uint8_t add_now = HAL_GPIO_ReadPin(KEY_ADD_GPIO_Port, KEY_ADD_Pin);
    uint8_t sub_now = HAL_GPIO_ReadPin(KEY_SUB_GPIO_Port, KEY_SUB_Pin);

    /* 加键：检测到变化 → 延时 20ms 消抖 → 再确认一次 */
    if (add_now != add_last)
    {
        HAL_Delay(20);
        if (HAL_GPIO_ReadPin(KEY_ADD_GPIO_Port, KEY_ADD_Pin) == add_now)
        {
            add_last = add_now;                    /* 记录新的稳定电平 */
            if (add_now == GPIO_PIN_RESET)         /* 松开→按下：生效一次 */
                return +1;
        }
    }

    // 减键
    if (sub_now != sub_last)
    {
        HAL_Delay(20);
        if (HAL_GPIO_ReadPin(KEY_SUB_GPIO_Port, KEY_SUB_Pin) == sub_now)
        {
            sub_last = sub_now;
            if (sub_now == GPIO_PIN_RESET)
                return -1;
        }
    }

    return 0;
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

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_I2C2_Init();
  MX_TIM2_Init();
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */
  HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1);

  HAL_Delay(20);
  OLED_Init(); 
  OLED_NewFrame();

  const float seq[5] = {0.0f, 45.0f, 90.0f, 135.0f, 180.0f};
  uint8_t  seq_index     = 1;   // 下一个自动序列点（上电已在 0°，所以从 45° 开始）
  float    target        = 0.0f;  // 目标角度
  uint16_t ccr           = AngleToCCR(target);
  char     line[20];
  char     message[50];
  uint32_t last_seq_tick = HAL_GetTick();  // 上一次自动步进的时间
  uint32_t manual_until  = 0;              // 手动操作后暂停自动序列的截止时间
  uint8_t  update        = 1;   // 1 = 需要刷新显示（上电先显示一次）

  Servo_SetAngle(target);       // 上电先转到 0°
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    uint32_t now = HAL_GetTick();

    int8_t step = Key_Scan();     // 0 = 无按键，+1 = 加键，-1 = 减键

    if (step != 0)
    {
      target += 15.0f * step;             // 每次 ±15°
      if (target < 0.0f)   target = 0.0f; // 限制在 0°~180°
      if (target > 180.0f) target = 180.0f;

      ccr = AngleToCCR(target);
      Servo_SetAngle(target);             // 角度一变立刻输出，不等按键松开
      manual_until = now + 3000;          // 手动操作后 3 秒内不跑自动序列
      update = 1;
    }

    // 自动序列：每满 1000ms 走一步（手动操作后 3 秒内暂停）
    if (now >= manual_until && now - last_seq_tick >= 1000)
    {
      last_seq_tick = now;
      target = seq[seq_index];
      seq_index = (seq_index + 1) % 5;

      ccr = AngleToCCR(target);
      Servo_SetAngle(target);
      update = 1;
    }

    if (update)
    {
      update = 0;

      // OLED信息显示
      OLED_NewFrame();
      sprintf(line, "Target:%03ddeg", (int)target);
      OLED_PrintString(0, 0, line, &font16x16, OLED_COLOR_NORMAL);

      sprintf(line, "CCR   :%4d", ccr);
      OLED_PrintString(0, 18, line, &font16x16, OLED_COLOR_NORMAL);

      sprintf(line, "Pulse :%d.%03dms", ccr / 1000, ccr % 1000);
      OLED_PrintString(0, 36, line, &font16x16, OLED_COLOR_NORMAL);
      OLED_ShowFrame();

      // 串口信息输出
      sprintf(message, "Target=%03ddeg  CCR=%4d  Pulse=%d.%03dms\r\n",
              (int)target, ccr, ccr / 1000, ccr % 1000);
      HAL_UART_Transmit(&huart1, (uint8_t*)message, strlen(message), HAL_MAX_DELAY);
    }

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

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
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
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
