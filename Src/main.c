/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2021 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under BSD 3-Clause license,
  * the "License"; You may not use this file except in compliance with the
  * License. You may obtain a copy of the License at:
  *                        opensource.org/licenses/BSD-3-Clause
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "dma.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "esp8266.h"
#include "string.h"
#include "mqtt.h"
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
/*收到主题后的回调函数，用户可以在这里进行对应主题的相关操作，如开灯，光灯等*/
void mqtt_publish_callback(uint8_t *topic, uint16_t topicLen, uint8_t *msg, uint16_t msgLen)
{
	HAL_UART_Transmit(&huart1, topic, topicLen, HAL_MAX_DELAY);
	HAL_UART_Transmit(&huart1, "\r\n", 2, HAL_MAX_DELAY);
	HAL_UART_Transmit(&huart1, msg, msgLen, HAL_MAX_DELAY);
	HAL_UART_Transmit(&huart1, "\r\n", 2, HAL_MAX_DELAY);
	if(msgLen==2 && !memcmp(msg,"ON",2))
		HAL_GPIO_WritePin(LED1_GPIO_Port,LED1_Pin,GPIO_PIN_RESET);
	if(msgLen==3 && !memcmp(msg,"OFF",3))
		HAL_GPIO_WritePin(LED1_GPIO_Port,LED1_Pin,GPIO_PIN_SET);
}
void mqtt_init(mqtt_client_t *client,uint8_t* clientId,uint8_t* mqtt_write_buff)
{
	HAL_UART_Receive_DMA(&huart2,dma_buff,BUFFER_SIZE);/*鍚姩涓插彛鎺ユ敹*/
	client->mqtt_client_id = clientId;
	client->mqtt_write_buf = mqtt_write_buff;
	client->mqtt_read_buf = dma_buff;
	client->mqtt_read_buf_size = BUFFER_SIZE;
	client->mqtt_keep_alive_interval = 10;
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* USER CODE BEGIN 1 */
	/*定义MQTT客户端及相关参数*/
	mqtt_client_t mClient;
  uint8_t mqtt_write_buff[128]={0};
	uint8_t *clientId=(uint8_t *)"zhaojianchuan";
	uint32_t curTick;
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
  MX_DMA_Init();
  MX_USART1_UART_Init();
  MX_USART2_UART_Init();
  /* USER CODE BEGIN 2 */

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
	
	/*设置ESP8622 WIFI模块*/
	HAL_UART_Transmit(&huart1,"init esp8266 start...\r\n",strlen("init esp8266 start...\r\n"),HAL_MAX_DELAY);
	if (!Esp8266_Init())
		HAL_UART_Transmit(&huart1,"init esp8266 ok\r\n",strlen("init esp8266 ok\r\n"),HAL_MAX_DELAY);
	else
		HAL_UART_Transmit(&huart1,"init esp8266 error\r\n",strlen("init esp8266 error\r\n"),HAL_MAX_DELAY);
	/*设置MQTT相关参数*/
	mqtt_init(&mClient,(uint8_t *)clientId,mqtt_write_buff);
	/*向服务器发送连接请求*/
	MQTT_connect(&mClient);
	HAL_Delay(100);
	/*定阅主题*/
	MQTT_subscribe(&mClient,"/zhaojc/sub",strlen("/zhaojc/sub"),0);
	curTick = HAL_GetTick();
  while (1)
  {
		MQTT_keep_alive(&mClient);//这一句一定要在while(1)中，用于定时发送PING请求以保持连接
		/*让指示灯闪烁，指示系统已进入正常工作*/
		if(curTick+1000 < HAL_GetTick()){
		  HAL_GPIO_TogglePin(LED0_GPIO_Port,LED0_Pin);
			curTick = HAL_GetTick();
		}
		/*检查串口是否有收到数据，如有，调用MQTT处理接口，进行MQTT协议处理*/
		if(dma_data_length)
		{
			//HAL_UART_Transmit(&huart1,"rcv\r\n",5,HAL_MAX_DELAY);
			HAL_UART_DMAStop(&huart2);
			
			/*进行MQTT协议处理，如果收到发布主题，此接口会回调mqtt_publish_callback接口，用户可以重写mqtt_publish_callback接口进相应操作*/
			mqtt_packet_handle(&mClient);
			dma_data_length = 0;
			/*开始下一次接收*/
			HAL_UART_Receive_DMA(&huart2,dma_buff,BUFFER_SIZE);
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

  /** Configure the main internal regulator output voltage 
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);
  /** Initializes the CPU, AHB and APB busses clocks 
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }
  /** Initializes the CPU, AHB and APB busses clocks 
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
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

  /* USER CODE END Error_Handler_Debug */
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
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     tex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
