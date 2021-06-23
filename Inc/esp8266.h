#ifndef __ESP_8266_H__
#define __ESP_8266_H__
#include "usart.h"
#define ESP8266_ERR (-1)
#define ESP8266_OK 0
#define ESP8266_UART huart2
#define DEBUG_UART huart1
int Esp8266_Exc_Cmd(char cmd[]);
int Esp8266_Init(void);

#endif
