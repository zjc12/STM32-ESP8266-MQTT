#include "mqtt.h"
#include "usart.h"
#include "stdio.h"
#include "string.h"
enum msgTypes
{
	CONNECT = 1, CONNACK, PUBLISH, PUBACK, PUBREC, PUBREL,
	PUBCOMP, SUBSCRIBE, SUBACK, UNSUBSCRIBE, UNSUBACK,
	PINGREQ, PINGRESP, DISCONNECT
};

#define send(pData, Size) HAL_UART_Transmit(&huart2,pData, Size,100)

#define PROTO_NAME "MQTT"
#define PROTO_VERSION 4	
#define PROTO_NAME_LEN_SIZE 2
#define PROTO_VERSION_SIZE 1
#define KEEP_ALIVE_SIZE 2
#define CONNECT_FLAG_SIZE 1
#define CONNECT_HEAD_LEN PROTO_NAME_LEN_SIZE+strlen(PROTO_NAME)+PROTO_VERSION_SIZE+CONNECT_FLAG_SIZE+PROTO_NAME_LEN_SIZE
#define CLIENT_ID_LEN_SIZE 2

/*�����÷��ͱ��ĺ󣬵������½ӿڷ��ͱ���*/
#define MQTT_packet_tx(c) \
do{ \
	MQTT_SEND(c->mqtt_write_buf,c->mqtt_write_buf_len); \
	c->mqtt_write_buf_len=0; \
	c->mqtt_last_alive_time = MQTT_GET_TICK(); \
}while(0)


void writeInt16(unsigned char** pptr, int anInt)
{
	**pptr = (unsigned char)(anInt / 256);
	(*pptr)++;
	**pptr = (unsigned char)(anInt % 256);
	(*pptr)++;
}
void writeInt8(unsigned char** pptr, uint8_t b)
{
	**pptr = b;
	(*pptr)++;
}
void writeStr(unsigned char** pptr, char *buf, int num)
{
	memcpy(*pptr,buf,num);
	(*pptr) +=num;
}
/**
  * @����    ��Э��涨����д�̶�ͷ��ʣ�೤���ֶΣ�1-4�ֽ�
  * @����    buf����ʼ��ַ��ָ��
             length��ʣ�೤��ֵ
  * @����ֵ  ��
  * @����    �Խ������㶫����ְҵ����ѧԺ
  */
void MQTTrem_len(unsigned char** buf, int length)
{
	int rc = 0;

	do
	{
		char d = length % 128;
		length /= 128;
		/*  */
		if (length > 0)
			d |= 0x80;
		(*buf)[rc++] = d;
	} while (length > 0);

	(*buf) += rc;
}

int readInt(unsigned char** pptr)
{
	unsigned char* ptr = *pptr;
	int len = 256*(*ptr) + (*(ptr+1));
	*pptr += 2;
	return len;
}
/**
  * @����    ��Э��涨����ȡ�̶�ͷ��ʣ�೤���ֶε�ֵ
  * @����    buf����������ʼ��ַ
             bufSize���������ֽ���
             lenSize��ʣ�೤���ֶε��ֽ���
  * @����ֵ  ʣ�೤�ȵ�ֵ
  * @����    �Խ������㶫����ְҵ����ѧԺ
  */
int MQTTRemLen_decode(unsigned char* buf, int bufSize, int *lenSize)
{
    int multiplier = 1;
    int rem_len = 0;
    const int MAX_REMAINING_LENGTH = 4;
    *lenSize=0;
	  do
	  {
			if(*lenSize>bufSize || *lenSize>MAX_REMAINING_LENGTH)
				return -1;
			rem_len += (buf[*lenSize] & 127) * multiplier;
			multiplier *= 128;
		}while((buf[(*lenSize)++] & 128) != 0);

		return rem_len;
}

/**
  * @����    ����������MQTT����������Ϣ
  * @����    c��mqtt�ͻ��˾��
  * @����ֵ  �̶�����0����ʱû����
  * @����    �Խ������㶫����ְҵ����ѧԺ
  */
int MQTT_connect(mqtt_client_t* c)
{
	unsigned char* ptr = c->mqtt_write_buf;
	unsigned int rem_len=0;
	MQTTHeader header = {0};
	MQTTConnectFlags flags = {0};
	
	header.bits.type = CONNECT;
	writeInt8(&ptr,header.byte);//д�̶�����ͷ�ĵ�һ�ֽ�
	

	rem_len = CONNECT_HEAD_LEN+CLIENT_ID_LEN_SIZE+strlen(c->mqtt_client_id);
	MQTTrem_len(&ptr,rem_len);//д�̶�����ͷ��ʣ�೤���ֶ�
	
	//���¿���д���ӱ��ĵĿɱ䱨��ͷ
	writeInt16(&ptr,strlen(PROTO_NAME));//дЭ�����ֵĳ���
	writeStr(&ptr,PROTO_NAME,strlen(PROTO_NAME));//дЭ�������ַ���

	writeInt8(&ptr,PROTO_VERSION);//Э��汾
	
	/*��д����FLAG*/
	flags.bits.cleansession =1;
	writeInt8(&ptr,flags.all);

	writeInt16(&ptr,c->mqtt_keep_alive_interval);//����ʱ��
	/*�ͻ���ID*/
  writeInt16(&ptr,strlen(c->mqtt_client_id));
	writeStr(&ptr,c->mqtt_client_id,strlen(c->mqtt_client_id));
	
	c->mqtt_write_buf_len = ptr - c->mqtt_write_buf;
	MQTT_packet_tx(c);
	return 0;
}

/**
  * @����    ����������MQTTPING������Ϣ
  * @����    c��mqtt�ͻ��˾��
  * @����ֵ  �̶�����0����ʱû����
  * @����    �Խ������㶫����ְҵ����ѧԺ
  */
int MQTT_ping_req(mqtt_client_t* c)
{
	unsigned char* ptr = c->mqtt_write_buf;
	MQTTHeader header = {0};
	header.bits.type = PINGREQ;
	
	writeInt8(&ptr,header.byte);
	MQTTrem_len(&ptr,0);//ʣ�೤��Ϊ0

	c->mqtt_write_buf_len = ptr - c->mqtt_write_buf;
	MQTT_packet_tx(c);
	return 0;
}

/**
  * @����    ����������MQTTY������Ϣ
  * @����    c��mqtt�ͻ��˾��
  *          topic�������ַ�������ʼ
  *          topic_len�������ַ����ĳ���
  *          msg���������ݵ���ʼ��ַ
  *          msg_len���������ݵĳ���
  * @����ֵ  �̶�����0����ʱû����
  * @����    �Խ������㶫����ְҵ����ѧԺ
  */
int MQTT_publish(mqtt_client_t* c,uint8_t *topic, int topic_len,uint8_t *msg, int msg_len)
{
	unsigned char* ptr = c->mqtt_write_buf;

	MQTTHeader header = {0};
	header.bits.type = PUBLISH;
	//header.bits.qos = 1;
	
	/*��̶�����ͷ*/
	writeInt8(&ptr,header.byte);
	MQTTrem_len(&ptr,2+topic_len+msg_len);
	
	/*���ⳤ�Ⱥ�����*/
	writeInt16(&ptr,topic_len);
	writeStr(&ptr,(char *)topic,topic_len);
	/*��Ϣ����*/
	writeStr(&ptr,(char *)msg,msg_len);
	
	c->mqtt_write_buf_len = ptr - c->mqtt_write_buf;
	MQTT_packet_tx(c);
	return 0;
}

/**
  * @����    ����������MQTTY����
  * @����    c��mqtt�ͻ��˾��
  *          topic�������ַ�������ʼ
  *          topic_len�������ַ����ĳ���
  *          qos��QOSֵ
  * @����ֵ  �̶�����0����ʱû����
  * @����    �Խ������㶫����ְҵ����ѧԺ
  */
int MQTT_subscribe(mqtt_client_t* c,uint8_t *topic, int topic_len,uint8_t qos)
{
	
	static unsigned int subscribe_id=0;
	unsigned char* ptr = c->mqtt_write_buf;

	MQTTHeader header = {0};
	header.bits.type = SUBSCRIBE;
	header.bits.qos = 1;
	
	/*��̶�����ͷ*/
	writeInt8(&ptr,header.byte);
	MQTTrem_len(&ptr,2+2+topic_len+1);//ID+LEN+TOPIC_STRING+QOS
	
	writeInt16(&ptr,subscribe_id++);
	/*��������*/
	writeInt16(&ptr,topic_len);
	writeStr(&ptr,(char *)topic,topic_len);
	
	/*QOS*/
	writeInt8(&ptr,qos);
	
	c->mqtt_write_buf_len = ptr - c->mqtt_write_buf;
	MQTT_packet_tx(c);
	return 0;
}

/**
  * @����    ��ʱ����PING�����Ա���MQTT���Ӳ����̿���ע�⣬�˺�������Ҫwhile(1)ѭ���ﱻ���ã�Ҳ�����ڶ�ʱ���ж��ﱻ����
  * @����    c��mqtt�ͻ��˾��
  * @����ֵ  ��
  * @����    �Խ������㶫����ְҵ����ѧԺ
  */
void MQTT_keep_alive(mqtt_client_t* c)
{
	if((c->mqtt_last_alive_time+c->mqtt_keep_alive_interval*1000) < MQTT_GET_TICK()){
		MQTT_ping_req(c);
	}
}

/**
  * @����    �յ���������Ļص��������˺���������д���û���д�˺����������յ���������Ϣ������Ӧ����
  * @����    topic����������
  *          topicLen���������ֵĳ���
  *          msg����Ϣ����
  *          msgLen����Ϣ���ݵĳ���
  * @����ֵ  ��
  * @����    �Խ������㶫����ְҵ����ѧԺ
  */
__weak void mqtt_publish_callback(uint8_t *topic, uint16_t topicLen, uint8_t *msg, uint16_t msgLen)
{
	HAL_UART_Transmit(&huart1, "recv publish:\r\n", 15, HAL_MAX_DELAY);
	HAL_UART_Transmit(&huart1, topic, topicLen, HAL_MAX_DELAY);
	HAL_UART_Transmit(&huart1, "\r\n", 2, HAL_MAX_DELAY);
	HAL_UART_Transmit(&huart1, msg, msgLen, HAL_MAX_DELAY);
	HAL_UART_Transmit(&huart1, "\r\n", 2, HAL_MAX_DELAY);
}

/**
  * @����    ������յ���MQTT���ģ��˺����������յ���������ʱ����,���յ�������Ϣʱ�������mqtt_publish_callback������
             ע��:������ֻʵ���˶Է���������Ϣ�Ĵ���������Ϣ�Ĵ��������
  * @����    c��mqtt�ͻ��˾��
  * @����ֵ  0����ʱ���κ�����
  * @����    �Խ������㶫����ְҵ����ѧԺ
  */
int mqtt_packet_handle(mqtt_client_t* c)
{
	MQTTHeader header = {0};
	int bufSize=c->mqtt_read_buf_size;
	int remLen=0;
	int remSize=0;
	int topic_len=0;
	uint8_t * ptr = c->mqtt_read_buf;
	
	/*��ȡ�̶�ͷ*/
	header.byte = *(ptr++);
	/*��ȡʣ���ֽ���*/
	remLen = MQTTRemLen_decode(ptr, bufSize,&remSize);
	if(remLen<0 || remLen>(dma_data_length-1-remSize))
		return -1;
	ptr+=remSize;
	
	/*������Ϣ���ͽ��в�ͬ����*/
	switch(header.bits.type)
	{
		case PUBLISH:
		{
			topic_len = readInt(&ptr);
			mqtt_publish_callback(ptr,topic_len,ptr+topic_len,remLen-2-topic_len);
			break;
		}
		case PINGRESP:
		{
			HAL_UART_Transmit(&huart1,"PINGRESP\r\n", strlen("PINGRESP\r\n"),100);
			break;
		}
		case CONNACK:
		{
			c->mqtt_stat = QMTT_CLIENT_CONNECTED;
			HAL_UART_Transmit(&huart1,"CONNACK\r\n", strlen("CONNACK\r\n"),100);
			break;
		}
		case DISCONNECT:
		{
			HAL_UART_Transmit(&huart1,"DISCONNECT\r\n", strlen("DISCONNECT\r\n"),100);
			break;
		}
		case PUBACK:
		{
			HAL_UART_Transmit(&huart1,"PUBACK\r\n", strlen("PUBACK\r\n"),100);
			break;
		}
		case SUBACK:
		{
			HAL_UART_Transmit(&huart1,"SUBACK\r\n", strlen("SUBACK\r\n"),100);
			break;
		}
		default:{
			char type[32];
			sprintf(type,"mqtt rec error type%d\r\n",header.bits.type);
			HAL_UART_Transmit(&huart1,type, strlen(type),100);
		}
	}
	return 0;
}






