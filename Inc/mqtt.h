#ifndef __MQTT_H__
#define __MQTT_H__
#include <stdint.h>
#define MQTT_OK 0

enum mqttClientStat
{
	QMTT_CLIENT_DISCONNECT = 0, 
	QMTT_CLIENT_CONNECTED
};
#define MQTT_SEND(buf,len) HAL_UART_Transmit(&huart2,buf,len,HAL_MAX_DELAY);
#define MQTT_GET_TICK() HAL_GetTick()

/**
 * Bitfields for the MQTT header byte.
 */
typedef union
{
	unsigned char byte;	                
	struct
	{
		unsigned int retain : 1;		
		unsigned int qos : 2;				
		unsigned int dup : 1;				
		unsigned int type : 4;			
	} bits;
} MQTTHeader;
typedef union
{
	unsigned char all;	/**< all connect flags */
	struct
	{
		unsigned int : 1;	     					
		unsigned int cleansession : 1;	  
		unsigned int will : 1;			    
		unsigned int willQoS : 2;				
		unsigned int willRetain : 1;		
		unsigned int password : 1; 			
		unsigned int username : 1;			
	} bits;
} MQTTConnectFlags;	/**< connect flags byte */
typedef struct
{
	uint16_t len;
	char* data;
} MQTTString;
typedef struct mqtt_client {
	uint8_t                     mqtt_stat;
	char                        *mqtt_client_id;
	uint8_t                     *mqtt_read_buf;
  uint8_t                     *mqtt_write_buf;
	uint8_t                     *mqtt_rec_topic_buf;
	uint16_t                    mqtt_keep_alive_interval;
	uint32_t                    mqtt_last_alive_time;
	uint32_t                    mqtt_read_buf_size;
  uint32_t                    mqtt_write_buf_size;
	uint32_t                    mqtt_write_buf_len;
  uint8_t                     mqtt_rec_topic_size;
}mqtt_client_t;


int MQTT_connect(mqtt_client_t* c);
int MQTT_publish(mqtt_client_t* c,uint8_t *topic, int topic_len,uint8_t *msg, int msg_len);
int MQTT_ping_req(mqtt_client_t* c);
int mqtt_packet_handle(mqtt_client_t* c);
int MQTT_subscribe(mqtt_client_t* c,uint8_t *topic, int topic_len,uint8_t qos);
void MQTT_keep_alive(mqtt_client_t* c);

#endif

