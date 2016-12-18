#ifndef __USER_CONFIG_H__
#define __USER_CONFIG_H__

#endif

#define MQTT_BUF_SIZE   1024
#define MQTT_RECONNECT_TIMEOUT  5 /*second*/
#define PROTOCOL_NAMEv31  /*MQTT version 3.1 compatible with Mosquitto v0.15*/

#if defined(MQTT_DEBUG_ON)
#define INFO( format, ... ) os_printf( format, ## __VA_ARGS__ )
#else
#define INFO( format, ... )
#endif

