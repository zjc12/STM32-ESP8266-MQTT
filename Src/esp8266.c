#include <stdint.h>
#include "esp8266.h"
#include <string.h>
#include <stdio.h>

struct esp8266_cmd
{
	uint8_t cmdName[16];//命令别名
	uint8_t cmdStr[64];//AT指令
};


struct esp8266_cmd  Cmd[]=
{
	{"test","AT\r\n"},
	{"tcp_start","AT+CIPSTART=\"TCP\",\"120.25.213.14\",1883\r\n"},
	//{"tcp_start","AT+CIPSTART=\"TCP\",\"192.168.43.164\",1883\r\n"},
	{"tcp_send","AT+CIPSEND\r\n"},
	{"trans_mode","AT+CIPMODE=1\r\n"},
	{"sta_mode","AT+CWMODE=1\r\n"},
	{"join_wifi","AT+CWJAP=\"zhao-\",\"13430295064\"\r\n"},
	{"quit_wifi","AT+CWQAP\r\n"}
};
#define CMD_NUM sizeof(Cmd)/sizeof(struct esp8266_cmd)
	
/**
  * @功能    发送一个AT指令，并等待返回值，从调试串口（串口1）输出返回信息，此函数不会阻塞，如果等待返回值时间超过ESP8266_TIME_OUT ms,返回失败并退出
  * @参数    命令别名
  * @返回值  ESP8266_OK：表示设置成功  
  *          ESP8266_ERR：表示设置失败
  * @作者    赵剑川，广东机电职业技术学院
  */
int Esp8266_Exc_Cmd(char cmd[])
{
	#define ESP8266_TIME_OUT 10000 //超时时间
	int cmdLen;
	
	int respondLen;
	int res=ESP8266_ERR;
	uint8_t *pos;
	int time=ESP8266_TIME_OUT;
	
	for(int id=0;id<CMD_NUM;id++)//遍历命令数组
	{
		if(!strcmp((char *)Cmd[id].cmdName,(char *)cmd)){//找到相应的AT指令
			cmdLen = strlen((char *)Cmd[id].cmdStr);
			//发送AT指令
			if(HAL_UART_Transmit(&ESP8266_UART,Cmd[id].cmdStr,cmdLen,HAL_MAX_DELAY)!=HAL_OK){
				return ESP8266_ERR;
			}
			//清除DMA缓存，准备接收返回信息
			memset(dma_buff,0,BUFFER_SIZE);
			HAL_UART_Receive_DMA(&ESP8266_UART,dma_buff,BUFFER_SIZE);
			
			do
			{
				if(pos=strstr((char*)dma_buff,"OK"))
          res = ESP8266_OK;
				else if(pos=strstr((char*)dma_buff,"ERROR"))
          res = ESP8266_ERR;
				if(!pos){
				  HAL_Delay(1);
					respondLen = dma_data_length;
				}else{
					respondLen = (ESP8266_OK==res)? (pos-dma_buff+4):(pos-dma_buff+7);
					break;
				}
			}while(!pos && time--);//等待返回直到超时退出
			
			HAL_UART_Transmit(&DEBUG_UART,dma_buff,respondLen,20);
			HAL_UART_DMAStop(&ESP8266_UART);
			break;
	  }
	}
	return res;
}

/**
  * @功能    设置WIFI模块，使得WIFI模块连接路由器并与服务器建立TCP连接
  * @返回值  ESP8266_OK：表示设置成功  
  *          ESP8266_ERR：表示设置失败，如果设置失败，可以重复调用此函数再次偿试设置，直到设置成功
  * @参数    无
  * @作者    赵剑川，广东机电职业技术学院
  */
int Esp8266_Init(void)
{
	uint8_t rc;
  static	int status=0;//当前初始化进度
	char *cmd[]={"test","sta_mode","join_wifi","trans_mode","tcp_start","tcp_send"};
	
	//单片机重启，WIFI模块不会重启，需要加以下代码退出send模式
	if(!status){
		HAL_UART_Transmit(&ESP8266_UART,(uint8_t *)"+++",3,HAL_MAX_DELAY);//发送"+++"以退出send模式
		HAL_Delay(10);//等待退出
		HAL_UART_Receive(&ESP8266_UART,&rc,1,10);//接收响应中的最后一个字符，避免影响下一次接收
	}
	
	//按顺序发送命令，命令提前存储在cmd数组中
	for(status=0;status<sizeof(cmd)/sizeof(char *);status++){
		if(ESP8266_OK!=Esp8266_Exc_Cmd(cmd[status]))
			return ESP8266_ERR;
  }
	return ESP8266_OK;
}


