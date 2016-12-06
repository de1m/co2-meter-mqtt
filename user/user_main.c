#include <ets_sys.h>
#include <osapi.h>
#include <ets_sys.h>
#include <os_type.h>
#include <gpio.h>
#include <mem.h>
#include <spi_flash.h>
#include "user_interface.h"
#include "config.h"
#include "../mqtt/include/mqtt.h"
#include "wifi.h"
#include "driver/uart.h"
#include <espconn.h>
#include "eagle_soc.h"
#include "fast_pwm.c"
#include "http.h"
#include "dnsAns.c"

#define BTN_CONFIG_GPIO 0
#define DEBUG

#define PPMMAX 2400
#define PPMMIN 400

#define PWM_2_OUT_IO_MUX PERIPHS_IO_MUX_GPIO4_U //R - Red
#define PWM_2_OUT_IO_NUM 4
#define PWM_2_OUT_IO_FUNC FUNC_GPIO4

#define PWM_3_OUT_IO_MUX PERIPHS_IO_MUX_GPIO5_U //G - Green
#define PWM_3_OUT_IO_NUM 5
#define PWM_3_OUT_IO_FUNC FUNC_GPIO5

#define PWM_CHANNEL 2
int ledinvert = 1; //1 = (+); 0 = (-)

uint32 duty[] = {0};
uint32 freq = 65536;
uint32 pwmcurr[2] = {0,0};

uint32 io_info[][3] = {
  {PWM_2_OUT_IO_MUX, PWM_2_OUT_IO_FUNC, PWM_2_OUT_IO_NUM},
  {PWM_3_OUT_IO_MUX, PWM_3_OUT_IO_FUNC, PWM_3_OUT_IO_NUM}
};

MQTT_Client mqttClient;
MQTT_Client *clientptr;

static struct espconn esp_conn;

static esp_tcp esptcp;
struct wCLIENTCFG clientConf;
struct MQTTServer mqConf;
struct co2sen co2Conf;
struct ledcnf ledConf;

uint32 sendinterval;
char topic[128];
int onlyCO2 = 0;
int mIsConnected = 0;
int setLEDRed = 0;
int ledRED = 0;

//for receive value from co2 sensor
const char cmd[9] = {0xFF,0x01,0x86,0x00,0x00,0x00,0x00,0x00,0x79}; //request
char ans[9]; //answer
uint8 counter = 0;

//times
os_timer_t DebounceTimer; //reset button debounce timer
os_timer_t SendDataTimer; //data send (MQTT) timer
os_timer_t ppmMaxLed; //if ppm > max value, then start this timer( for blink red led)
os_timer_t errorInputRed; //if error in html config input - red led on for 3 sec

#define user_procTaskPrio        0
#define user_procTaskQueueLen    1
os_event_t user_procTaskQueue[user_procTaskQueueLen];

//Main code function
static void ICACHE_FLASH_ATTR loop(os_event_t *events) {

  int c = uart0_rx_one_char();

  if(c != -1 ) {

	  ans[counter] = c;

	  //reset counter (after first boot)
	  if(ans[counter] == 0x86 && ans[counter-1] == 0xFF){
		  ans[0] = 0xFF;
		  ans[1] = 0x86;
		  counter = 1;
	  }
	  #ifdef DEBUG
		os_printf("counter: %d, ans: %02x \n", counter, ans[counter]);
	  #endif

	  counter++;
	  if(counter == 9){
		  //os_printf("ACT COUNTER: %d \n", counter);
		  counter = 0;
		  //os_printf("SET COUNTER TO 0, value: %d\n", counter);
		  resolvCO2Value();
	  }
  }

  os_delay_us(100);
  system_os_post(user_procTaskPrio, 0, 0 );
}

void ICACHE_FLASH_ATTR resolvCO2Value(void){
	uint8 y;
	uint8 crc = 0;
	uint8 err = 0;
	uint32 co2value = 0;

    //unsigned int getMqttSett = readMqttSetting(&mqttInfo);
    unsigned int getMqttSett = spi_flash_read((LEDCONFADDR*SPI_FLASH_SEC_SIZE), (uint32*)&ledConf, sizeof(struct ledcnf));
	#ifdef DEBUG
		os_printf("LED PPMMMIN: %d\n", ledConf.ppmmin);
		os_printf("LED PPMMAX: %d\n", ledConf.ppmmax);
		os_printf("LED Brithness Co: %d\n", ledConf.brit);
	#endif

	counter = 0;

	#ifdef DEBUG
	uint8 i;
		//os_printf("Bytes from CO2 Sensor (9Bytes)\n");
		for(i = 0; i < 9; i++){
			//os_printf("%d: %02x \n",i,ans[i]);
		}
	#endif

	//build checksum
	for(y = 1; y < 8; y++){
		crc += ans[y];
	}
	crc = 255 - crc;
	crc++;

	#ifdef DEBUG
		os_printf("Compare checksum from CO2(CRC) and calculated(CRCR)\n");
		os_printf("CRC: %d, CRCR: %d\n", ans[8], crc);
	#endif

	if(!(ans[0] == 0xFF && ans[1] == 0x86 && ans[8] == crc)){
		//error
		err = 1;

		//write to display - E001;
		writeNum(0b10011110, 1); // - E
		writeNum(0,0);
		writeNum(0,0);
		writeNum(1,0);
		latch();
	} else {

		uint32 ledR, ledG;

		co2value = ans[2]*256 + ans[3];

		os_printf("CO2: %d\n", co2value);

		sendNumToDisp(co2value);

		//calculate value RG (Red/Green) for RGB LED
	    if(co2value <= ledConf.ppmmin){
	        ledR = 0;
	        ledG = 255-ledR;

	        ledRED = 0;

	    } else {
	    	if(co2value >= ledConf.ppmmax){
		        ledR = 255;
		        ledG = 0;

		    	os_timer_disarm(&ppmMaxLed);
		    	os_timer_setfn(&ppmMaxLed, &blink_led_red, 0);
		    	os_timer_arm(&ppmMaxLed, 200, 1);

		    	ledRED = 1;
		    	os_printf("SET ledRED to: %d\n", ledRED);

	    	} else {

	    		ledR = ((255)*(co2value-ledConf.ppmmin)*100/(ledConf.ppmmax - ledConf.ppmmin))/100;
	    		ledG = 255-ledR;

	    		ledRED = 0;
	    		os_printf("SET ledRED to: %d\n", ledRED);
	    	}
	    }

		//correct
		ledR = ledR/ledConf.brit;
		ledG = ledG/ledConf.brit;

		#ifdef DEBUG
			os_printf("R: %d, G: %d\n", ledR, ledG);
		#endif

		if(ledRED == 0){
			pwm_write(ledR,ledG);
			os_printf("SET ledRED to: %d\nDISABLE ppmMaxLed\n", ledRED);
			os_timer_disarm(&ppmMaxLed);
		}

	}

	if(onlyCO2 == 1){
		#ifdef DEBUG
			//os_printf("CO2 ONLY\n");
		#endif

		err = 0;

	} else {
		#ifdef DEBUG
			//os_printf("MQTT ONLY\n");
		#endif

		int len = 0;

		if(co2value < 1000){
			len = 3;
		} else if(co2value >= 1000) {
			len = 4;
		} else {
			len = 3;
		}

		char *buffer = (char*)os_malloc(sizeof(char)*(len+1));

		os_sprintf(buffer, "%d", co2value);

	    if(mIsConnected == 1){
	    	if(err == 1){
	    		MQTT_Publish(clientptr, topic, "err", len, mqConf.qos, 0);
	    	} else {
	    		MQTT_Publish(clientptr, topic, buffer, len, mqConf.qos, 0);
	    	}
	    }

	    err = 0;

	}
}

void ICACHE_FLASH_ATTR blink_led_red(void *arg){

	//sed led to red;
	if(setLEDRed == 0){
		pwm_write(255/ledConf.brit,0);
		setLEDRed = 1;
	} else {
		pwm_write(0,0);
		setLEDRed = 0;
	}

}

void ICACHE_FLASH_ATTR startCo2only(void){
	#ifdef DEBUG
		os_printf("Start co2 only\n");
	#endif
	struct co2sen co2Sett;
	wifi_set_opmode(0);
    wipe_flash(1);

    unsigned int getCO2Sett = spi_flash_read((CO2CONFADDR*SPI_FLASH_SEC_SIZE), (uint32*)&co2Sett, sizeof(struct co2sen));
	#ifdef DEBUG
		os_printf("(17) - Read co2 interval setting: %d\n", co2Sett.interval);
	#endif

	pwm_write(0,50);

    onlyCO2 = 1;

    sendinterval = co2Sett.interval;

    //Start os task
    system_os_task(loop, user_procTaskPrio,user_procTaskQueue, user_procTaskQueueLen);
    system_os_post(user_procTaskPrio, 0, 0 );

    initSendTimer();

}

void ICACHE_FLASH_ATTR startAPMode(void) {

	char const ssid_str[32] = "CO2.box";

	wifi_set_opmode(2);
	struct softap_config apconfig;
	os_memcpy(apconfig.ssid, &ssid_str, sizeof(ssid_str));

	apconfig.ssid_len = 0;
	apconfig.authmode = 0;
	apconfig.ssid_hidden = 0;
	apconfig.max_connection = 1;
	apconfig.beacon_interval = 100;

	if(wifi_softap_set_config(&apconfig))
	{
		#ifdef DEBUG
	   	   os_printf("\nRunning as AP. \nSSID: %s\n", apconfig.ssid);
		#endif

	} else {
		#ifdef DEBUG
			os_printf("\nERR: Cannot connect...\n");
		#endif
	}

	sta_init();

}

void ICACHE_FLASH_ATTR startClient(void){
	#ifdef DEBUG
		os_printf("Start client\n");
	#endif

	struct MQTTServer mqttInfo;

    wipe_flash(2);
    onlyCO2 = 0;

    //unsigned int getMqttSett = readMqttSetting(&mqttInfo);
    unsigned int getMqttSett = spi_flash_read((MQTTCONFADDR*SPI_FLASH_SEC_SIZE), (uint32*)&mqConf, sizeof(struct MQTTServer));
	#ifdef DEBUG
		os_printf("(14) - Read from flash mqtt client id: %s\n", mqConf.clientid);
		os_printf("(15) - Read from flash mqtt user: %s\n", mqConf.user);
		os_printf("(16) - Read from flash mqtt pass: %s\n", mqConf.password);
		os_printf("(15) - Read from flash mqtt topic: %s\n", mqConf.topic);
		os_printf("(16) - Read from flash mqtt port: %d\n", mqConf.port);
		os_printf("(17) - Read from flash co2 interval: %d\n", mqConf.sendinterval);
	#endif

	sendinterval = mqConf.sendinterval;
	os_memcpy(topic, &mqConf.topic, sizeof(mqConf.topic));

	MQTT_InitConnection(&mqttClient, mqConf.server, mqConf.port, 0);

	if(mqConf.auth == 1){
		#ifdef DEBUG
			os_printf("Authentification enabled\n");
		#endif
		MQTT_InitClient(&mqttClient, mqConf.clientid, mqConf.user, mqConf.password, 120, 1);
	} else {
		#ifdef DEBUG
			os_printf("Authentification disabled\n");
		#endif
		MQTT_InitClient(&mqttClient, mqConf.clientid, 0, 0, 120, 1);
	}

	MQTT_InitLWT(&mqttClient, "/lwt", "offline", 0, 0);
	MQTT_OnConnected(&mqttClient, mqttConnectedCb);
	MQTT_OnDisconnected(&mqttClient, mqttDisconnectedCb);
	MQTT_OnPublished(&mqttClient, mqttPublishedCb);

	WIFI_Connect(wifiConnectCb);

    //Start os task
    system_os_task(loop, user_procTaskPrio,user_procTaskQueue, user_procTaskQueueLen);
    system_os_post(user_procTaskPrio, 0, 0 );
	initSendTimer();
}

static void ICACHE_FLASH_ATTR wifiConnectCb(uint8_t status)
{
  if (status == STATION_GOT_IP) {
    MQTT_Connect(&mqttClient);
  } else {
    MQTT_Disconnect(&mqttClient);
  }
}

static char getValueOfKey(char *key, unsigned char maxlen, char *str, char *retval)
{
	unsigned char found = 0;
	char *keyptr = key;
	char prev_char = '\0';
	*retval = '\0';

	while( *str && *str!='\r' && *str!='\n' && !found )
	{
		if(*str == *keyptr)
		{
			if(keyptr == key && !( prev_char == '?' || prev_char == '&' ) )
			{
				str++;
				continue;
			}

			keyptr++;

			if (*keyptr == '\0')
			{
				str++;
				keyptr = key;
				if (*str == '=')
				{
					found = 1;
				}
			}
		}
		else
		{
			keyptr = key;
		}
		prev_char = *str;
		str++;
	}
	if(found == 1)
	{
		found = 0;
		while( *str && *str!='\r' && *str!='\n' && *str!=' ' && *str!='&' && maxlen>0 )
		{
			*retval = *str;
			maxlen--;
			str++;
			retval++;
			found++;
		}
		*retval = '\0';
	}
	return found;
}

void ICACHE_FLASH_ATTR initSendTimer(void){

	os_timer_disarm(&SendDataTimer);
	os_timer_setfn(&SendDataTimer, &send_data_cb, 0);
	os_timer_arm(&SendDataTimer, sendinterval*1000, 1);

	#ifdef DEBUG
		os_printf("interval was set to %ld s.\n", sendinterval);
	#endif
}

static void ICACHE_FLASH_ATTR sta_mode_recon_cb(void *arg, sint8 err)
{
    os_printf("STA Mode - Client reconnected\r\n");
}

static void ICACHE_FLASH_ATTR sta_mode_discon_cb(void *arg)
{
    os_printf("STA Mode - Client disconnected\r\n");
}

static void ICACHE_FLASH_ATTR mqttConnectedCb(uint32_t *args)
{
	mIsConnected = 1;

	MQTT_Client* client = (MQTT_Client*)args;

	clientptr = (MQTT_Client*)args;
	#ifdef DEBUG
		os_printf("MQTT: Connected\r\n");
	#endif

	pwm_write(0,50);

}

void ICACHE_FLASH_ATTR send_data_cb(void *arg){

	//send requiest to co2 sensor
	uart0_tx_buffer(cmd, sizeof(cmd));

}

static void ICACHE_FLASH_ATTR mqttDisconnectedCb(uint32_t *args){

	mIsConnected = 0;

	MQTT_Client* client = (MQTT_Client*)args;
	#ifdef DEBUG
	  os_printf("MQTT: Disconnected\r\n");
	#endif
}

static void ICACHE_FLASH_ATTR mqttPublishedCb(uint32_t *args)
{
	MQTT_Client* client = (MQTT_Client*)args;
	#ifdef DEBUG
	  os_printf("MQTT: Published\r\n");
	#endif
}

void ICACHE_FLASH_ATTR sta_send_cb(void *arg){
	os_printf("SEND DATA WAS OK\n");
	espconn_disconnect(arg);
}

void ICACHE_FLASH_ATTR errorConfigRed(void *arg){

	os_timer_disarm(&errorInputRed);
	pwm_write(50,50);

	//write init display
	writeNum(0b11101100, 1); //n
	writeNum(0,0); // 0
	writeNum(0b10011100, 1); //c
	writeNum(0b10001110, 1); //f
	latch();
}

static void ICACHE_FLASH_ATTR sta_mode_recv_cb(void *arg, char *data, unsigned short len){

	struct espconn *ptrespconn = (struct espconn *)arg;
	char tempInterval[32];
	int errCount = 0;

	#ifdef DEBUG
		os_printf("###### DStart ######\n");
		os_printf("Data: %s\n", data);
		os_printf("###### DStop ######\r\n");
	#endif


	if(os_strncmp(data, "GET / ", 6) == 0){
		espconn_send(ptrespconn, (uint8 *)formHtml, os_strlen(formHtml));
	}

	if(os_strncmp(data, "GET /?int", 9) == 0){

		char wifiON[2];
		char tempAdvcd[2];
		char tempppmmin[4];
		char temppmmax[4];
		char tempbrit[1];

		if(getValueOfKey("adv", sizeof(tempAdvcd), data, tempAdvcd) <= 0){
			//not set
			ledConf.ppmmin = PPMMIN;
			ledConf.ppmmax = PPMMAX;

			ledConf.brit = 2;

		} else {

			if(getValueOfKey("mmn", sizeof(tempppmmin), data, tempppmmin) <= 0){
				ledConf.ppmmin = PPMMIN;

			} else {
				ledConf.ppmmin = atoi(tempppmmin);

				if(ledConf.ppmmin <= 0){
					ledConf.ppmmin = PPMMIN;
				}
			}

			if(getValueOfKey("mmx", sizeof(temppmmax), data, temppmmax) <= 0){
				ledConf.ppmmax = PPMMAX;

			} else {
				ledConf.ppmmax = atoi(temppmmax);
				if(ledConf.ppmmax <= 0){
					ledConf.ppmmax = PPMMAX;

					//os_printf("ADVAN SET, MAX SET\n");
				}
				if(ledConf.ppmmax >= 5000){
					ledConf.ppmmax = 5000;
				}
			}

			if(getValueOfKey("brc", sizeof(tempbrit), data, tempbrit) <= 0){
				ledConf.brit = 2;
			} else {
				ledConf.brit = atoi(tempbrit);
			}

		}

		//save led config
		spi_flash_erase_sector(LEDCONFADDR);
		unsigned int l = spi_flash_write((LEDCONFADDR*SPI_FLASH_SEC_SIZE), (uint32*)&ledConf, sizeof(struct ledcnf));
		if(l != SPI_FLASH_RESULT_OK){
			#ifdef DEBUG
				os_printf("Error write Led config to flash\n");
				os_printf("TEST1: %d\n", l);
			#endif
			errCount++;
		}

		if(getValueOfKey("wS", 2, data, wifiON) <= 0) { //check if wifi & mqtt settings are activated
			//wifi is not set

			#ifdef DEBUG
				os_printf("CO2 only\n");
			#endif

			if(getValueOfKey("int", sizeof(tempInterval), data, tempInterval) > 0){ // refresh interval
				co2Conf.interval = atoi(tempInterval);
			} else {
				//set default
				co2Conf.interval = 120;
			}
			co2Conf.configsaved = 1;

			#ifdef DEBUG
				os_printf("CO2 Interval set to: %ld sec.\n", co2Conf.interval);
			#endif

			//unsigned int getMqttSett = writeCO2Setting(&co2Conf);
			spi_flash_erase_sector(CO2CONFADDR);
			unsigned int c = spi_flash_write(CO2CONFADDR*SPI_FLASH_SEC_SIZE, (uint32*)&co2Conf, sizeof(struct co2sen));

			if(c != SPI_FLASH_RESULT_OK){
				#ifdef DEBUG
					os_printf("Error write co2 sen config to flash\n");
					os_printf("TEST2: %d\n", c);
				#endif
				errCount++;
			} else {

				int wstat = wipe_flash(DELWIFIMQTTCONF);
				if(wstat == 0){
					os_printf("Wipe MQTT and Wifi config successfull\n");
					startMode();
				} else {
					os_printf("Wipe MQTT and Wifi config error\n");

					pwm_write(50,0);

					//write to display - E001;
					writeNum(0b10011110, 1); // - E
					writeNum(0,0);
					writeNum(0,0);
					writeNum(4,0);
					latch();

			    	os_timer_disarm(&errorInputRed);
			    	os_timer_setfn(&errorInputRed, &errorConfigRed, 0);
			    	os_timer_arm(&errorInputRed, 5000, 0);

				}
			}

		} else {
			//wifi is set
			#ifdef DEBUG
				os_printf("Wifi and MQTT was set\n");
			#endif

			char tempQOS[1]; // temp char for MQTT QoS
			char tempport[5]; // temp char for MQTT Port
			char tempDhcp[2]; // temp char for dhcp setting (on/off)
			char tempAuth[2]; // temp char for auth setting (on/off)


			if(getValueOfKey("wf", sizeof(clientConf.ssid), data, clientConf.ssid) <= 0){
				#ifdef DEBUG
					os_printf("Value of SSID not correct: %s\n", clientConf.ssid);
				#endif
				errCount++;
			}

			if(getValueOfKey("ps", sizeof(clientConf.password), data, clientConf.password) <= 0){
				#ifdef DEBUG
					os_printf("Value of WIFI Pass not correct: %s\n", clientConf.password);
				#endif
				errCount++;
			}

			if(getValueOfKey("ipc", sizeof(tempDhcp), data, tempDhcp) <= 0){
				clientConf.dhcp = 1;
				os_printf("DHCP is set\n");
			} else {
				#ifdef DEBUG
					os_printf("DHCP is NOT set\n");
				#endif

				clientConf.dhcp = 0;
				getValueOfKey("ip", sizeof(clientConf.ip), data, clientConf.ip);
				getValueOfKey("net", sizeof(clientConf.netmask), data, clientConf.netmask);
				getValueOfKey("gw", sizeof(clientConf.gw), data, clientConf.gw);
				getValueOfKey("dn1", sizeof(clientConf.dns1), data, clientConf.dns1);
				getValueOfKey("dn2", sizeof(clientConf.dns2), data, clientConf.dns2);
			}
			clientConf.configsaved = 1;
			os_printf("\n");

			if(getValueOfKey("mId", sizeof(mqConf.clientid), data, mqConf.clientid) <= 0){
				#ifdef DEBUG
					os_printf("Value of MQTT ID not correct: %s\n", mqConf.clientid);
				#endif
				errCount++;
			}

			if(getValueOfKey("mad", sizeof(mqConf.server), data, mqConf.server) <= 0){
				#ifdef DEBUG
					os_printf("Value of MQTT Server not correct: %s\n", mqConf.server);
				#endif
				errCount++;
			}

			if(getValueOfKey("mtpc", sizeof(mqConf.topic), data, mqConf.topic) <= 0){
				#ifdef DEBUG
					os_printf("Value of MQTT Topic not correct: %s\n", mqConf.topic);
				#endif
				errCount++;
			} else {

				//convert html string, "%2F" to "/"
				char *topicstr;
				int rpl = 0;
				topicstr = replace(mqConf.topic, "%2F", "/", &rpl);
				strcpy(mqConf.topic, topicstr);
			}
			if(getValueOfKey("mqs", sizeof(tempQOS), data, tempQOS) <= 0){
				mqConf.qos = 1;
			} else {
				mqConf.qos = atoi(tempQOS);
			}

			if(getValueOfKey("int", sizeof(tempInterval), data, tempInterval) <= 0){
				mqConf.sendinterval = 120;
			} else {
				mqConf.sendinterval = atoi(tempInterval);
			}

			if(getValueOfKey("mprt", sizeof(tempport), data, tempport) <= 0){
				mqConf.port = 1883;
			} else {
				mqConf.port = atoi(tempport);
			}

			if(getValueOfKey("ath", sizeof(tempAuth), data, tempAuth) <= 0){
				//without auth
				#ifdef DEBUG
					os_printf("MQTT Auth is disabled\n");
				#endif
				mqConf.auth = 0;
			} else {
				//Auth true
				#ifdef DEBUG
					os_printf("MQTT Auth is enabled\n");
				#endif
				mqConf.auth = 1;

				if(getValueOfKey("mus", sizeof(mqConf.user), data, mqConf.user) <= 0){
					#ifdef DEBUG
						os_printf("Value of MQTT User not correct: %s\n", mqConf.user);
					#endif
					errCount++;
				}

				if(getValueOfKey("mps", sizeof(mqConf.password), data, mqConf.password) <= 0){
					#ifdef DEBUG
						os_printf("Value of MQTT Pass not correct: %s\n", mqConf.password);
					#endif
					errCount++;
				}
			}

			//save wifi config
			spi_flash_erase_sector(WIFICONFADDR);
			unsigned int w = spi_flash_write((WIFICONFADDR*SPI_FLASH_SEC_SIZE),(uint32*)&clientConf, sizeof(struct wCLIENTCFG));
			if(w != SPI_FLASH_RESULT_OK){
				#ifdef DEBUG
					os_printf("Error write wifi config to flash\n");
					os_printf("TEST3: %d", w);
				#endif
				errCount++;
			}

			//save mqtt config
			spi_flash_erase_sector(MQTTCONFADDR);
			unsigned int m = spi_flash_write((MQTTCONFADDR*SPI_FLASH_SEC_SIZE),(uint32*)&mqConf, sizeof(struct MQTTServer));
			if(m != SPI_FLASH_RESULT_OK){
				#ifdef DEBUG
					os_printf("Error write mqtt config to flash\n");
					os_printf("TEST4: %d\n", m);
				#endif
				errCount++;
			}

			int wf = wipe_flash(DELCO2CONF);
			if(wf != 0){
				#ifdef DEBUG
					os_printf("Wipe co2 config error \n");
				#endif
				pwm_write(0,50);
			}

			if(errCount > 0){
				#ifdef DEBUG
					os_printf("Err Count: %d\n", errCount);

					pwm_write(50,0);

					//write to display - E001;
					writeNum(0b10011110, 1); // - E
					writeNum(0,0);
					writeNum(0,0);
					writeNum(3,0);
					latch();

			    	os_timer_disarm(&errorInputRed);
			    	os_timer_setfn(&errorInputRed, &errorConfigRed, 0);
			    	os_timer_arm(&errorInputRed, 5000, 0);

				#endif

			} else {

				errCount = 0;
				startMode();
			}

		}
	}
}

char* replace(const char *str, const char *oldstr, const char *newstr, int *count){
   const char *tmp = str;
   char *result;
   int   found = 0;
   int   length, reslen;
   int   oldlen = os_strlen(oldstr);
   int   newlen = os_strlen(newstr);
   int   limit = (count != NULL && *count > 0) ? *count : -1;

   tmp = str;
   while ((tmp = strstr(tmp, oldstr)) != NULL && found != limit)
      found++, tmp += oldlen;

   length = os_strlen(str) + found * (newlen - oldlen);
   if ( (result = (char *)os_malloc(length+1)) == NULL) {
      //fprintf(stderr, "Not enough memory\n");
      found = -1;
   } else {
      tmp = str;
      limit = found; /* Countdown */
      reslen = 0; /* length of current result */
      /* Replace each old string found with new string  */
      while ((limit-- > 0) && (tmp = strstr(tmp, oldstr)) != NULL) {
         length = (tmp - str); /* Number of chars to keep intouched */
         strncpy(result + reslen, str, length); /* Original part keeped */
         strcpy(result + (reslen += length), newstr); /* Insert new string */
         reslen += newlen;
         tmp += oldlen;
         str = tmp;
      }
      strcpy(result + reslen, str); /* Copies last part and ending nul char */
   }
   if (count != NULL) *count = found;
   return result;
}

static void ICACHE_FLASH_ATTR sta_connect_cb(void *arg){

	struct espconn *pesp_conn = arg;

	#ifdef DEBUG
		os_printf("Client connected\n");
	#endif

	espconn_regist_sentcb(pesp_conn, sta_send_cb);
	espconn_regist_recvcb(pesp_conn, sta_mode_recv_cb);


	#ifdef DEBUG
    	espconn_regist_reconcb(pesp_conn, sta_mode_recon_cb);
    	espconn_regist_disconcb(pesp_conn, sta_mode_discon_cb);
	#endif

}

void ICACHE_FLASH_ATTR sta_init(void){

	#ifdef DEBUG
		os_printf("TCP connection init...\n");
	#endif

    esp_conn.type = ESPCONN_TCP;
    esp_conn.state = ESPCONN_NONE;
    esp_conn.proto.tcp = &esptcp;
    esp_conn.proto.tcp->local_port = 80;
    espconn_regist_connectcb(&esp_conn, sta_connect_cb);
    espconn_accept(&esp_conn);
}

int ICACHE_FLASH_ATTR isClient(){
	//Read config from flash
	struct wCLIENTCFG wConfAct;
	struct co2sen co2Conf;

	//unsigned int readwifi = readWifiSetting(&wConfAct);
	unsigned int readwifi = spi_flash_read((WIFICONFADDR*SPI_FLASH_SEC_SIZE), (uint32*)&wConfAct, sizeof(struct wCLIENTCFG));
	#ifdef DEBUG
		os_printf("(1) - Read wifi conf from flash: %d\n", readwifi);
		os_printf("STAT WIFI: %d\n", wConfAct.configsaved);
	#endif

	//unsigned int readCo2 = readCO2Setting(&co2Conf);
	unsigned int readCo2 = spi_flash_read((CO2CONFADDR*SPI_FLASH_SEC_SIZE), (uint32*)&co2Conf, sizeof(struct co2sen));
	#ifdef DEBUG
		os_printf("STAT CO2 saved: %d\n", co2Conf.configsaved);
		os_printf("STAT CO2 interval: %d\n", co2Conf.interval);
	#endif

	if(co2Conf.configsaved == 1){
		return 0;
	} else {
		if(wConfAct.configsaved == 1){
			#ifdef DEBUG
				os_printf("Wifi config found, start as a client\n");
				os_printf("(2) - Wifi read conf: %d\n", wConfAct.configsaved);
			#endif
			return 1;
		} else {
			#ifdef DEBUG
				os_printf("Wifi config not found, start as AP\n");
				os_printf("(3) - Wifi read conf: %d\n", wConfAct.configsaved);
			#endif
			return 2;
		}
	}
}

// Инициализация кнопки CONFMODE (GPIO0)
void BtnInit() {
	// Уст. GPIO 0 на ввод-вывод
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO0_U, FUNC_GPIO0);
	// Вкл. подтяг. резистор
	PIN_PULLUP_EN(PERIPHS_IO_MUX_GPIO0_U);
	// Откл. глоб.прерываний
	ETS_GPIO_INTR_DISABLE();
	// Подкл. процедуры обраб. прерываний
	ETS_GPIO_INTR_ATTACH(input_intr_handler, NULL);
	GPIO_DIS_OUTPUT(BTN_CONFIG_GPIO);
	// Очистка статуса
	GPIO_REG_WRITE(GPIO_STATUS_W1TC_ADDRESS, BIT(2));
	// Вкл.прерываний
	gpio_pin_intr_state_set(GPIO_ID_PIN(BTN_CONFIG_GPIO), GPIO_PIN_INTR_NEGEDGE);
	// Вкл.глоб.прерываний
	ETS_GPIO_INTR_ENABLE();
	// Таймер
	os_timer_disarm(&DebounceTimer);
	os_timer_setfn(&DebounceTimer, &debounce_timer_cb, 0);
}

// Процедура обраб. прерываний
LOCAL void input_intr_handler(void *arg)
{
	// Состояние GPIO
	uint32 gpio_status = GPIO_REG_READ(GPIO_STATUS_ADDRESS);
	// Очистка
	GPIO_REG_WRITE(GPIO_STATUS_W1TC_ADDRESS, gpio_status);
	// Откл.прерываний
	ETS_GPIO_INTR_DISABLE();
	gpio_pin_intr_state_set(GPIO_ID_PIN(BTN_CONFIG_GPIO), GPIO_PIN_INTR_DISABLE);
	// Вкл.прерываний
	ETS_GPIO_INTR_ENABLE();
	// Установка таймера
	os_timer_arm(&DebounceTimer, 200, 0);
}

// Выполняется в случае нажатия кнопки CONFMODE
void ICACHE_FLASH_ATTR debounce_timer_cb(void *arg)
{
	ETS_GPIO_INTR_DISABLE();
	gpio_pin_intr_state_set(GPIO_ID_PIN(BTN_CONFIG_GPIO), GPIO_PIN_INTR_NEGEDGE);
	ETS_GPIO_INTR_ENABLE();
    if(wipe_flash(DELALLCONF) == 0){

    	os_timer_disarm(&ppmMaxLed);

    	//write init display
    	writeNum(0b11101100, 1); //n
    	writeNum(0,0); // 0
    	writeNum(0b10011100, 1); //c
    	writeNum(0b10001110, 1); //f
    	latch();

    	os_timer_disarm(&SendDataTimer);
    	startMode();
    }
}

int ICACHE_FLASH_ATTR wipe_flash(int delconf){
	if(delconf == 0){ //0 -delete all, 1 - delete wifi and mqtt, 2 - delete co2sens

		unsigned int c = spi_flash_erase_sector(CO2CONFADDR);
		unsigned int w = spi_flash_erase_sector(WIFICONFADDR);
		unsigned int m = spi_flash_erase_sector(MQTTCONFADDR);

		if(c == 0 && w == 0 && m == 0){
			#ifdef DEBUG
				os_printf("(1) - Sector wipe was successful\n");
			#endif
			return 0;
		} else {
			#ifdef DEBUG
				os_printf("(2) - Error: cannot erase a sector\n");
			#endif
			return 1;
		}
	}
	if(delconf == 1){

		unsigned int w = spi_flash_erase_sector(WIFICONFADDR);
		unsigned int m = spi_flash_erase_sector(MQTTCONFADDR);

		if(w == 0 && m == 0){
			#ifdef DEBUG
				os_printf("(3) - Sector wipe was successful\n");
			#endif
			return 0;
		} else {
			#ifdef DEBUG
				os_printf("(4) - Error: cannot erase a sector\n");
			#endif
			return 1;
		}
	}
	if(delconf == 2){ //0 -delete all, 1 - delete wifi and mqtt, 2 - delete co2sens
		unsigned int c = spi_flash_erase_sector(CO2CONFADDR);

		if(c == 0){
			#ifdef DEBUG
				os_printf("(5) - Sector wipe was successful\n");
			#endif
			return 0;
		} else {
			#ifdef DEBUG
				os_printf("(6) - Error: cannot erase a sector\n");
			#endif
			return 1;
		}
	}
}

void ICACHE_FLASH_ATTR startMode(void){
	#ifdef DEBUG
		os_printf("Start Mode Check \n");
	#endif

	pwm_write(50,50);

	int mode = isClient();
	os_printf("START MODE: %d\n", mode);

	switch(mode){
	case(0):
		startCo2only();
		break;
	case(1):
		startClient();
		break;
	case(2):
		startAPMode();
		break;
	}
}

uint8 ICACHE_FLASH_ATTR pwm_write(uint8 r, uint8 g){

	uint32 uson = 256*256;
	uint32 pwmlong[2] = {0};

	if(ledinvert == 0){
		pwmlong[0] = r*255;
		pwmlong[1] = g*255;
	} else {
		pwmlong[0] = uson - r*255;
		pwmlong[1] = uson - g*255;
	}

	//os_printf("RGB: %ld,%ld\n", pwmlong[0], pwmlong[1]);

  	pwm_set_duty(pwmlong[0], 0);
  	pwm_set_duty(pwmlong[1], 1);
	pwm_start();
	return 0;
}

void ICACHE_FLASH_ATTR ShtRegInit(void){
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTMS_U, FUNC_GPIO14); //14 - DS
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTDI_U, FUNC_GPIO12); //12 - ST_CP - latch
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTCK_U, FUNC_GPIO13); //13 - SH_CP - clock
}

void ICACHE_FLASH_ATTR strob(void){
	gpio_output_set(BIT13, 0, BIT13, 0); //high
	gpio_output_set(0, BIT13, BIT13, 0); //low
}

void ICACHE_FLASH_ATTR latch(void){
	gpio_output_set(BIT12, 0, BIT12, 0); //high
	gpio_output_set(0, BIT12, BIT12, 0); //low
}

void ICACHE_FLASH_ATTR setOn(void){
	gpio_output_set(0, BIT14, BIT14, 0); //low
	strob();
}

void ICACHE_FLASH_ATTR setOff(void){
	gpio_output_set(BIT14, 0, BIT14, 0); //high
	strob();
}

void ICACHE_FLASH_ATTR writeNum(int num, bool custom){

	int num0 = 0b11111100;
	int num1 = 0b01100000;
	int num2 = 0b11011010;
	int num3 = 0b11110010;
	int num4 = 0b01100110;
	int num5 = 0b10110110;
	int num6 = 0b10111110;
	int num7 = 0b11100000;
	int num8 = 0b11111110;
	int num9 = 0b11110110;

	if(custom){
		//write custom int to display
		// 0b11101100 - N;
		// 0b10011100 - C;
		// 0b10001110 - F;
		// 0b10011110 - E

	} else {
		switch(num){
			case 0: num = num0;break;
			case 1: num = num1;break;
			case 2: num = num2;break;
			case 3: num = num3;break;
			case 4: num = num4;break;
			case 5: num = num5;break;
			case 6: num = num6;break;
			case 7: num = num7;break;
			case 8: num = num8;break;
			case 9: num = num9;break;
		}
	}

	int i;
	for(i = 0;i<8;i++){
		if ((num&(1<<i)) == 0){
			setOff();
			latch();

		} else {
			setOn();
			latch();
		}
	}

};

void ICACHE_FLASH_ATTR sendNumToDisp(uint32 num){

	if(num < 1000){
		writeNum(0b00000000,1);
		writeNum(0b00000000,1);
		writeNum(0b00000000,1);
		writeNum(0b00000000,1);
	}
    int temp, factor = 1;

    temp = num;
    while (temp)
    {
      temp = temp / 10;
      factor = factor * 10;
    }

    while (factor>1)
    {
      factor = factor / 10;
      writeNum(num / factor, 0);
      num = num % factor;
    }
    latch();
}

void post_user_init_func(void){
	BtnInit(); //init button
	ShtRegInit(); //init pins for display

	//write init display
	writeNum(0b11101100, 1); //n
	writeNum(0,0); // 0
	writeNum(0b10011100, 1); //c
	writeNum(0b10001110, 1); //f
	latch();

	#ifdef DEBUG
	  os_printf("\n\nStart:\n");
	#endif

	pwm_init(freq, duty, 2, io_info);
	pwm_write(0,0);

	startMode();
}

//Init function
void ICACHE_FLASH_ATTR
user_init()
{

	//GPIO
	gpio_init();

	// Initialize UART0
	//115200
	//uart_div_modify(0, UART_CLK_FREQ / 9600);
	uart_init(BIT_RATE_9600,BIT_RATE_9600);
	//os_install_putc1((void *)uart1_write_char);
	//uart0_tx_buffer("1123\n",5);

	system_init_done_cb(&post_user_init_func);
}
