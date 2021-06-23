#include <stdint.h>
#include "esp8266.h"
#include <string.h>
#include <stdio.h>

struct esp8266_cmd
{
	uint8_t cmdName[16];//�������
	uint8_t cmdStr[64];//ATָ��
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
  * @����    ����һ��ATָ����ȴ�����ֵ���ӵ��Դ��ڣ�����1�����������Ϣ���˺�����������������ȴ�����ֵʱ�䳬��ESP8266_TIME_OUT ms,����ʧ�ܲ��˳�
  * @����    �������
  * @����ֵ  ESP8266_OK����ʾ���óɹ�  
  *          ESP8266_ERR����ʾ����ʧ��
  * @����    �Խ������㶫����ְҵ����ѧԺ
  */
int Esp8266_Exc_Cmd(char cmd[])
{
	#define ESP8266_TIME_OUT 10000 //��ʱʱ��
	int cmdLen;
	
	int respondLen;
	int res=ESP8266_ERR;
	uint8_t *pos;
	int time=ESP8266_TIME_OUT;
	
	for(int id=0;id<CMD_NUM;id++)//������������
	{
		if(!strcmp((char *)Cmd[id].cmdName,(char *)cmd)){//�ҵ���Ӧ��ATָ��
			cmdLen = strlen((char *)Cmd[id].cmdStr);
			//����ATָ��
			if(HAL_UART_Transmit(&ESP8266_UART,Cmd[id].cmdStr,cmdLen,HAL_MAX_DELAY)!=HAL_OK){
				return ESP8266_ERR;
			}
			//���DMA���棬׼�����շ�����Ϣ
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
			}while(!pos && time--);//�ȴ�����ֱ����ʱ�˳�
			
			HAL_UART_Transmit(&DEBUG_UART,dma_buff,respondLen,20);
			HAL_UART_DMAStop(&ESP8266_UART);
			break;
	  }
	}
	return res;
}

/**
  * @����    ����WIFIģ�飬ʹ��WIFIģ������·�����������������TCP����
  * @����ֵ  ESP8266_OK����ʾ���óɹ�  
  *          ESP8266_ERR����ʾ����ʧ�ܣ��������ʧ�ܣ������ظ����ô˺����ٴγ������ã�ֱ�����óɹ�
  * @����    ��
  * @����    �Խ������㶫����ְҵ����ѧԺ
  */
int Esp8266_Init(void)
{
	uint8_t rc;
  static	int status=0;//��ǰ��ʼ������
	char *cmd[]={"test","sta_mode","join_wifi","trans_mode","tcp_start","tcp_send"};
	
	//��Ƭ��������WIFIģ�鲻����������Ҫ�����´����˳�sendģʽ
	if(!status){
		HAL_UART_Transmit(&ESP8266_UART,(uint8_t *)"+++",3,HAL_MAX_DELAY);//����"+++"���˳�sendģʽ
		HAL_Delay(10);//�ȴ��˳�
		HAL_UART_Receive(&ESP8266_UART,&rc,1,10);//������Ӧ�е����һ���ַ�������Ӱ����һ�ν���
	}
	
	//��˳�������������ǰ�洢��cmd������
	for(status=0;status<sizeof(cmd)/sizeof(char *);status++){
		if(ESP8266_OK!=Esp8266_Exc_Cmd(cmd[status]))
			return ESP8266_ERR;
  }
	return ESP8266_OK;
}


