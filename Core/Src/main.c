/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * 功能说明：
  * 1. USART1：连接舵机驱动板/总线舵机通信
  * 2. USART2：连接 HC-05 蓝牙
  * 3. 上电/复位后，先执行一套固定动作
  * 4. 固定动作执行结束后，回到初始位置
  * 5. 然后等待蓝牙指令控制动作
  *
  * 蓝牙指令：
  *   A / a : 动作组A
  *   B / b : 动作组B
  *   C / c : 动作组C
  *   R / r : 回初始位
  *   S / s : 停止所有舵机
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "gpio.h"
#include "usart.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <string.h>
#include <stdio.h>
#include "servo_bus.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* 如果你的 servo_bus.h 里已经定义过这些宏，这里就删掉，避免重复 */
#ifndef SERVO_ID_1
#define SERVO_ID_1                 1
#endif

#ifndef SERVO_ID_2
#define SERVO_ID_2                 2
#endif

#ifndef SERVO_ID_3
#define SERVO_ID_3                 3
#endif

#ifndef SERVO_BROADCAST_ID
#define SERVO_BROADCAST_ID         0xFE
#endif

/* PB5 为 LED */
#define LED_GPIO_Port              GPIOB
#define LED_Pin                    GPIO_PIN_5

/* 是否启用上电固定动作：1 启用，0 关闭 */
#define POWER_ON_ACTION_ENABLE     1

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
uint8_t bt_rx_byte = 0;                 /* 蓝牙接收单字节缓冲 */
volatile uint8_t bt_cmd = 0;            /* 最新收到的蓝牙命令 */
volatile uint8_t bt_cmd_flag = 0;       /* 蓝牙新命令标志 */
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
void Debug_Print(char *str);
void Action_Group_A(void);
void Action_Group_B(void);
void Action_Group_C(void);
void PowerOn_ActionSequence(void);
void Process_BT_Command(uint8_t cmd);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/**
  * @brief  蓝牙串口打印调试信息
  * @param  str: 需要发送的字符串
  * @retval None
  */
void Debug_Print(char *str)
{
    HAL_UART_Transmit(&huart2, (uint8_t *)str, strlen(str), 100);
}

/**
  * @brief  动作组A
  * @note   这里调用 servo_bus.c 中的底层舵机控制函数
  * @retval None
  */
void Action_Group_A(void)
{
    Servo_MoveTimeWaitWrite(SERVO_ID_1, 200, 800);
    Servo_MoveTimeWaitWrite(SERVO_ID_2, 600, 800);
    Servo_MoveTimeWaitWrite(SERVO_ID_3, 500, 800);
    Servo_MoveStart(SERVO_BROADCAST_ID);

    Debug_Print("Action A\r\n");
}

/**
  * @brief  动作组B
  * @retval None
  */

void Action_Group_B(void)
{
    Servo_MoveTimeWaitWrite(SERVO_ID_1, 800, 800);
    Servo_MoveTimeWaitWrite(SERVO_ID_2, 400, 800);
    Servo_MoveTimeWaitWrite(SERVO_ID_3, 300, 800);
    Servo_MoveStart(SERVO_BROADCAST_ID);

    Debug_Print("Action B\r\n");
}

/**
  * @brief  动作组C
  * @retval None
  */
void Action_Group_C(void)
{
    Servo_MoveTimeWaitWrite(SERVO_ID_1, 500, 700);
    Servo_MoveTimeWaitWrite(SERVO_ID_2, 500, 700);
    Servo_MoveTimeWaitWrite(SERVO_ID_3, 500, 700);
    Servo_MoveStart(SERVO_BROADCAST_ID);

    Debug_Print("Action C\r\n");
}

/**
  * @brief  上电/复位后执行固定动作
  * @note   如果你想改开机动作，主要改这里的目标位置和时间
  * @retval None
  */
void PowerOn_ActionSequence(void)
{
    Debug_Print("Power-on action start\r\n");

    /* 第1步 */
    Servo_MoveTimeWaitWrite(SERVO_ID_1, 485, 800);
    Servo_MoveTimeWaitWrite(SERVO_ID_2, 555, 800);
    Servo_MoveTimeWaitWrite(SERVO_ID_3, 415, 800);
    Servo_MoveStart(SERVO_BROADCAST_ID);    
    HAL_Delay(1000);

    /* 第2步 */
    Servo_MoveTimeWaitWrite(SERVO_ID_1, 795, 700);
    Servo_MoveTimeWaitWrite(SERVO_ID_2, 405, 700);
    Servo_MoveTimeWaitWrite(SERVO_ID_3, 565, 700);
    Servo_MoveStart(SERVO_BROADCAST_ID);
    HAL_Delay(900);

    /* 第3步 */
    Servo_MoveTimeWaitWrite(SERVO_ID_1, 485, 700);
    Servo_MoveTimeWaitWrite(SERVO_ID_2, 555, 700);
    Servo_MoveTimeWaitWrite(SERVO_ID_3, 415, 700);    
    Servo_MoveStart(SERVO_BROADCAST_ID);
    HAL_Delay(900);

    /* 固定动作结束后回到初始位
       这里调用的是 servo_bus.c 里的 Servo_InitPose() */
    Servo_InitPose();
    HAL_Delay(1200);

    Debug_Print("Power-on action end\r\n");
}

/**
  * @brief  处理蓝牙收到的命令
  * @param  cmd: 蓝牙命令字符
  * @retval None
  */
void Process_BT_Command(uint8_t cmd)
{
    switch (cmd)
    {
        case 'A':
        case 'a':
            HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);
            Action_Group_A();
            break;

        case 'B':
        case 'b':
            HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);
            Action_Group_B();
            break;

        case 'C':
        case 'c':
            HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);
            Action_Group_C();
            break;

        case 'R':
        case 'r':
            HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);
            Servo_InitPose();
            Debug_Print("Return init pose\r\n");
            break;

        case 'S':
        case 's':
            HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);
            Servo_MoveStop(SERVO_BROADCAST_ID);
            Debug_Print("Stop all servo\r\n");
            break;

        default:
            Debug_Print("Unknown cmd\r\n");
            break;
    }
}

/**
  * @brief  串口接收中断回调
  * @param  huart: 串口句柄
  * @retval None
  */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART2)
    {
        bt_cmd = bt_rx_byte;
        bt_cmd_flag = 1;

        /* 重新开启下一字节接收 */
        HAL_UART_Receive_IT(&huart2, &bt_rx_byte, 1);
    }
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* MCU Configuration--------------------------------------------------------*/
  HAL_Init();
  SystemClock_Config();

  MX_GPIO_Init();
  MX_USART1_UART_Init();
  MX_USART2_UART_Init();

  /* USER CODE BEGIN 2 */

  /* 启动蓝牙串口中断接收 */
  HAL_UART_Receive_IT(&huart2, &bt_rx_byte, 1);

  Debug_Print("System start\r\n");

  /* LED 启动闪烁提示 */
  HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_RESET);
  HAL_Delay(200);
  HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_SET);
  HAL_Delay(200);
  HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_RESET);
  HAL_Delay(200);
  HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_SET);

  /* 等待电源和舵机稳定 */
  HAL_Delay(1000);

  /* 先回一次初始位，保证动作起点一致
     这里调用 servo_bus.c 中的 Servo_InitPose() */
  Servo_InitPose();
  HAL_Delay(1200);

#if POWER_ON_ACTION_ENABLE
  /* 上电后执行一套固定动作 */
  PowerOn_ActionSequence();
#endif

  /* USER CODE END 2 */

  /* Infinite loop */
  while (1)
  {
    if (bt_cmd_flag == 1)
    {
        bt_cmd_flag = 0;
        Process_BT_Command(bt_cmd);
    }

    HAL_Delay(10);
  }
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

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

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK
                              | RCC_CLOCKTYPE_SYSCLK
                              | RCC_CLOCKTYPE_PCLK1
                              | RCC_CLOCKTYPE_PCLK2;
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
  __disable_irq();
  while (1)
  {
    HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);
    HAL_Delay(100);
  }
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* 用户可在这里添加自己的调试代码 */
}
#endif /* USE_FULL_ASSERT */