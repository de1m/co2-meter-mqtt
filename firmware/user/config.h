#define MQTTCONFADDR 0xC8
#define WIFICONFADDR 0xC9
#define CO2CONFADDR 0xCA
#define LEDCONFADDR 0xCB
#define DELALLCONF 0
#define DELWIFIMQTTCONF 1
#define DELCO2CONF 2

struct wCLIENTCFG {
	uint32 configsaved;
	uint32 dhcp;
	char ssid[32];
	char password[64];
	char ip[15];
	char netmask[15];
	char gw[15];
	char dns1[15];
	char dns2[15];
};

struct co2sen {
	uint32 interval;
	uint32 configsaved;
};

struct ledcnf {
	uint32 ppmmin;
	uint32 ppmmax;
	uint8 brit;
};

struct MQTTServer {
	unsigned char clientid[32];
	unsigned char server[64];
	uint32 port;
	uint32 auth;
	uint32 Advcd;
	unsigned char user[32];
	unsigned char password[64];
	char topic[128];
	int qos;
	uint32 sendinterval; //in sek
};

//mqtt
static void ICACHE_FLASH_ATTR mqttConnectedCb(uint32_t *args);
static void ICACHE_FLASH_ATTR mqttDisconnectedCb(uint32_t *args);
static void ICACHE_FLASH_ATTR mqttPublishedCb(uint32_t *args);
static void ICACHE_FLASH_ATTR wifiConnectCb(uint8_t status);

void ICACHE_FLASH_ATTR sta_init(void);
static char getValueOfKey(char *key, unsigned char maxlen, char *str, char *retval);
int ICACHE_FLASH_ATTR isClient();
LOCAL void input_intr_handler(void *arg);
void ICACHE_FLASH_ATTR debounce_timer_cb(void *arg);
LOCAL void timeCounter(void *arg);
int ICACHE_FLASH_ATTR wipe_flash(int delconf);
void ICACHE_FLASH_ATTR startMode(void);
uint8 ICACHE_FLASH_ATTR pwm_write(uint8 r, uint8 g);

void ICACHE_FLASH_ATTR send_data_cb(void *arg);
char* replace(const char *str, const char *oldstr, const char *newstr, int *count);
void ICACHE_FLASH_ATTR initSendTimer(void);
void ICACHE_FLASH_ATTR sendNumToDisp(uint32 num);
void ICACHE_FLASH_ATTR resolvCO2Value(void);
void ICACHE_FLASH_ATTR writeNum(int num, bool custom);
void ICACHE_FLASH_ATTR latch(void);
void ICACHE_FLASH_ATTR blink_led_red(void *arg);
LOCAL void ICACHE_FLASH_ATTR mdns_init();
void ICACHE_FLASH_ATTR blink_conf_err_led(void *arg);
void ICACHE_FLASH_ATTR check_wifi_stat(void *arg);
