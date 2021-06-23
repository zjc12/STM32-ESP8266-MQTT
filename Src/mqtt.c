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

/*构建好发送报文后，调用以下接口发送报文*/
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
  * @功能    按协议规定，填写固定头的剩余长度字段，1-4字节
  * @参数    buf：起始地址的指针
             length：剩余长度值
  * @返回值  无
  * @作者    赵剑川，广东机电职业技术学院
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
  * @功能    按协议规定，读取固定头的剩余长度字段的值
  * @参数    buf：缓冲区起始地址
             bufSize：缓冲区字节数
             lenSize：剩余长度字段的字节数
  * @返回值  剩余长度的值
  * @作者    赵剑川，广东机电职业技术学院
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
  * @功能    构建并发送MQTT连接请求消息
  * @参数    c：mqtt客户端句柄
  * @返回值  固定返回0，暂时没意义
  * @作者    赵剑川，广东机电职业技术学院
  */
int MQTT_connect(mqtt_client_t* c)
{
	unsigned char* ptr = c->mqtt_write_buf;
	unsigned int rem_len=0;
	MQTTHeader header = {0};
	MQTTConnectFlags flags = {0};
	
	header.bits.type = CONNECT;
	writeInt8(&ptr,header.byte);//写固定报文头的第一字节
	

	rem_len = CONNECT_HEAD_LEN+CLIENT_ID_LEN_SIZE+strlen(c->mqtt_client_id);
	MQTTrem_len(&ptr,rem_len);//写固定报文头的剩余长度字段
	
	//以下开以写连接报文的可变报文头
	writeInt16(&ptr,strlen(PROTO_NAME));//写协议名字的长度
	writeStr(&ptr,PROTO_NAME,strlen(PROTO_NAME));//写协议名字字符串

	writeInt8(&ptr,PROTO_VERSION);//协议版本
	
	/*填写连接FLAG*/
	flags.bits.cleansession =1;
	writeInt8(&ptr,flags.all);

	writeInt16(&ptr,c->mqtt_keep_alive_interval);//保活时间
	/*客户端ID*/
  writeInt16(&ptr,strlen(c->mqtt_client_id));
	writeStr(&ptr,c->mqtt_client_id,strlen(c->mqtt_client_id));
	
	c->mqtt_write_buf_len = ptr - c->mqtt_write_buf;
	MQTT_packet_tx(c);
	return 0;
}

/**
  * @功能    构建并发送MQTTPING请求消息
  * @参数    c：mqtt客户端句柄
  * @返回值  固定返回0，暂时没意义
  * @作者    赵剑川，广东机电职业技术学院
  */
int MQTT_ping_req(mqtt_client_t* c)
{
	unsigned char* ptr = c->mqtt_write_buf;
	MQTTHeader header = {0};
	header.bits.type = PINGREQ;
	
	writeInt8(&ptr,header.byte);
	MQTTrem_len(&ptr,0);//剩余长度为0

	c->mqtt_write_buf_len = ptr - c->mqtt_write_buf;
	MQTT_packet_tx(c);
	return 0;
}

/**
  * @功能    构建并发布MQTTY主题消息
  * @参数    c：mqtt客户端句柄
  *          topic：主题字符串的起始
  *          topic_len：主题字符串的长度
  *          msg：主题内容的起始地址
  *          msg_len：主题内容的长度
  * @返回值  固定返回0，暂时没意义
  * @作者    赵剑川，广东机电职业技术学院
  */
int MQTT_publish(mqtt_client_t* c,uint8_t *topic, int topic_len,uint8_t *msg, int msg_len)
{
	unsigned char* ptr = c->mqtt_write_buf;

	MQTTHeader header = {0};
	header.bits.type = PUBLISH;
	//header.bits.qos = 1;
	
	/*填固定报文头*/
	writeInt8(&ptr,header.byte);
	MQTTrem_len(&ptr,2+topic_len+msg_len);
	
	/*主题长度和主题*/
	writeInt16(&ptr,topic_len);
	writeStr(&ptr,(char *)topic,topic_len);
	/*消息内容*/
	writeStr(&ptr,(char *)msg,msg_len);
	
	c->mqtt_write_buf_len = ptr - c->mqtt_write_buf;
	MQTT_packet_tx(c);
	return 0;
}

/**
  * @功能    构建并订阅MQTTY主题
  * @参数    c：mqtt客户端句柄
  *          topic：主题字符串的起始
  *          topic_len：主题字符串的长度
  *          qos：QOS值
  * @返回值  固定返回0，暂时没意义
  * @作者    赵剑川，广东机电职业技术学院
  */
int MQTT_subscribe(mqtt_client_t* c,uint8_t *topic, int topic_len,uint8_t qos)
{
	
	static unsigned int subscribe_id=0;
	unsigned char* ptr = c->mqtt_write_buf;

	MQTTHeader header = {0};
	header.bits.type = SUBSCRIBE;
	header.bits.qos = 1;
	
	/*填固定报文头*/
	writeInt8(&ptr,header.byte);
	MQTTrem_len(&ptr,2+2+topic_len+1);//ID+LEN+TOPIC_STRING+QOS
	
	writeInt16(&ptr,subscribe_id++);
	/*订阅主题*/
	writeInt16(&ptr,topic_len);
	writeStr(&ptr,(char *)topic,topic_len);
	
	/*QOS*/
	writeInt8(&ptr,qos);
	
	c->mqtt_write_buf_len = ptr - c->mqtt_write_buf;
	MQTT_packet_tx(c);
	return 0;
}

/**
  * @功能    定时发送PING请求，以保持MQTT连接不被继开，注意，此函数必需要while(1)循环里被调用，也可以在定时器中断里被调用
  * @参数    c：mqtt客户端句柄
  * @返回值  无
  * @作者    赵剑川，广东机电职业技术学院
  */
void MQTT_keep_alive(mqtt_client_t* c)
{
	if((c->mqtt_last_alive_time+c->mqtt_keep_alive_interval*1000) < MQTT_GET_TICK()){
		MQTT_ping_req(c);
	}
}

/**
  * @功能    收到发布主题的回调函数，此函数可以重写，用户重写此函数，根据收到的主题消息进行相应操作
  * @参数    topic：主题名字
  *          topicLen：主题名字的长度
  *          msg：消息内容
  *          msgLen：消息内容的长度
  * @返回值  无
  * @作者    赵剑川，广东机电职业技术学院
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
  * @功能    处理接收到的MQTT报文，此函数可以在收到网络数据时调用,当收到发布消息时，会调用mqtt_publish_callback函数。
             注意:本函数只实现了对发布主题消息的处理，其它消息的处理待完善
  * @参数    c：mqtt客户端句柄
  * @返回值  0，暂时无任何意义
  * @作者    赵剑川，广东机电职业技术学院
  */
int mqtt_packet_handle(mqtt_client_t* c)
{
	MQTTHeader header = {0};
	int bufSize=c->mqtt_read_buf_size;
	int remLen=0;
	int remSize=0;
	int topic_len=0;
	uint8_t * ptr = c->mqtt_read_buf;
	
	/*读取固定头*/
	header.byte = *(ptr++);
	/*读取剩余字节数*/
	remLen = MQTTRemLen_decode(ptr, bufSize,&remSize);
	if(remLen<0 || remLen>(dma_data_length-1-remSize))
		return -1;
	ptr+=remSize;
	
	/*根据消息类型进行不同处理*/
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






