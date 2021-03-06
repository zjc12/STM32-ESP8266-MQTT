# STM32-ESP8266-MQTT
基于STM32+ESP8266实现MQTT客户端协议，可以远程控制开发板上的LED灯

----------------------------移植说明---------------------------------
可参考视频进行移植：https://www.bilibili.com/video/BV1J44y1z7WP

----------------------------------------------------------------------

本实例代码已在STM32F407ZGTx上测试通过，如在其它型号上运行，可以按以下方法进行简单的移植。

1、用STM32CubeMX配置工程，使能与ESP8266相连接的串口的接收DMA功能，并且使能全局中断。

2、复制esp8266.c、esp8266.h、mqtt.c、mqtt.h到工程相应目录。

3、在usart.c中增加以下代码：

/* USER CODE BEGIN 0 */

uint8_t dma_buff[BUFFER_SIZE];

volatile uint8_t dma_data_length=0;//避免编译器优化

/* USER CODE END 0 */



/* USER CODE BEGIN 1 */

void USER_UART_IDLECallback(UART_HandleTypeDef *huart)

{

    dma_data_length = (BUFFER_SIZE - __HAL_DMA_GET_COUNTER(&hdma_usart2_rx));   //计算接收到的数据长度
    
}

void USER_UART_IRQHandler(UART_HandleTypeDef *huart)

{

    if(USART2 == huart->Instance)                                   //判断是否是串口1（！此处应写(huart->Instance == USART1)
    {
        if(RESET != __HAL_UART_GET_FLAG(&huart2, UART_FLAG_IDLE))   //判断是否是空闲中断
        {
            __HAL_UART_CLEAR_IDLEFLAG(&huart2);                     //清楚空闲中断标志（否则会一直不断进入中断）
            USER_UART_IDLECallback(huart);                          //调用中断处理函数
        }
    }
    
}

/* USER CODE END 1 */


4、在usart.c中的void HAL_UART_MspInit(UART_HandleTypeDef* uartHandle)函数中增加使能空闲中断的代码，如下：

  /* USER CODE BEGIN USART2_MspInit 1 */
  
  __HAL_UART_ENABLE_IT(&huart2, UART_IT_IDLE);
  
  /* USER CODE END USART2_MspInit 1 */
  
5、在usart.h中增加以下代码：

/* USER CODE BEGIN Private defines */

#define BUFFER_SIZE 256

extern uint8_t dma_buff[BUFFER_SIZE];

extern volatile uint8_t dma_data_length;

/* USER CODE END Private defines */

6、在main.h中加入以下函数声明：

void USER_UART_IRQHandler(UART_HandleTypeDef *huart);

7、在stm32fxx.it.c中，修改中断服务函数void USART2_IRQHandler(void)，修改为：

void USART2_IRQHandler(void)

{

  /* USER CODE BEGIN USART2_IRQn 0 */
  

  /* USER CODE END USART2_IRQn 0 */
  
  HAL_UART_IRQHandler(&huart2);
  
  /* USER CODE BEGIN USART2_IRQn 1 */
  USER_UART_IRQHandler(&huart2);                                //此行为新增行
  
  /* USER CODE END USART2_IRQn 1 */
  
}



8、至此，代码可以正常调用了，其它业务实现可参考main()函数。

