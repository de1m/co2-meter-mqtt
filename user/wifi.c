/*
 * wifi.c
 *
 *  Created on: Dec 30, 2014
 *      Author: Minh
 */
#include "wifi.h"
#include "user_interface.h"
#include "osapi.h"
#include "espconn.h"
#include "os_type.h"
#include "mem.h"
#include "mqtt_msg.h"
#include "debug.h"
#include "user_config.h"
#include "config.h"

#define DEBUG

static ETSTimer WiFiLinker;
WifiCallback wifiCb = NULL;
static uint8_t wifiStatus = STATION_IDLE, lastWifiStatus = STATION_IDLE;
static void ICACHE_FLASH_ATTR wifi_check_ip(void *arg)
{
  struct ip_info ipConfig;
  os_timer_disarm(&WiFiLinker);
  wifi_get_ip_info(STATION_IF, &ipConfig);
  wifiStatus = wifi_station_get_connect_status();
  if (wifiStatus == STATION_GOT_IP && ipConfig.ip.addr != 0)
  {
    os_timer_setfn(&WiFiLinker, (os_timer_func_t *)wifi_check_ip, NULL);
    os_timer_arm(&WiFiLinker, 2000, 0);
  }
  else
  {
    if (wifi_station_get_connect_status() == STATION_WRONG_PASSWORD)
    {
      INFO("STATION_WRONG_PASSWORD\r\n");
      wifi_station_connect();
    }
    else if (wifi_station_get_connect_status() == STATION_NO_AP_FOUND)
    {
      INFO("STATION_NO_AP_FOUND\r\n");
      wifi_station_connect();
    }
    else if (wifi_station_get_connect_status() == STATION_CONNECT_FAIL)
    {
      INFO("STATION_CONNECT_FAIL\r\n");
      wifi_station_connect();
    }
    else
    {
      INFO("STATION_IDLE\r\n");
    }

    os_timer_setfn(&WiFiLinker, (os_timer_func_t *)wifi_check_ip, NULL);
    os_timer_arm(&WiFiLinker, 500, 0);
  }
  if (wifiStatus != lastWifiStatus) {
    lastWifiStatus = wifiStatus;
    if (wifiCb)
      wifiCb(wifiStatus);
  }
}

void ICACHE_FLASH_ATTR WIFI_Connect(WifiCallback cb)
{
  //struct station_config stationConf;

  //INFO("WIFI_INIT\r\n");
  //wifi_set_opmode_current(STATION_MODE);
  wifiCb = cb;
  //os_memset(&stationConf, 0, sizeof(struct station_config));
  //os_sprintf(stationConf.ssid, "%s", ssid);
  //os_sprintf(stationConf.password, "%s", pass);
  //wifi_station_set_config_current(&stationConf);
  os_timer_disarm(&WiFiLinker);
  os_timer_setfn(&WiFiLinker, (os_timer_func_t *)wifi_check_ip, NULL);
  os_timer_arm(&WiFiLinker, 1000, 0);
  //wifi_station_connect();

  struct wCLIENTCFG wifiClientConf;
  int ipint[4];
  int subnetint[4];
  int gw[4];
  int dns1[4];
  int dns2[4];
  int iptrue, subnettrue, gwtrue, dns1true, dns2true;

  //set mode to STA
  wifi_station_disconnect();
  wifi_set_opmode(1);

  //int readConf = readWifiSetting(wifiClientConf);
  unsigned int readwifi = spi_flash_read((WIFICONFADDR*SPI_FLASH_SEC_SIZE), (uint32*)&wifiClientConf, sizeof(struct wCLIENTCFG));

  #ifdef DEBUG
	os_printf("(5) - Read Ip from flash: %s\n", wifiClientConf.ip);
	os_printf("(6) - Read subnet from flash: %s\n", wifiClientConf.netmask);
	os_printf("(7) - Read gateway from flash: %s\n###\n", wifiClientConf.gw);
  #endif

  iptrue = strintIPtoInt(wifiClientConf.ip, ipint);
  subnettrue = strintIPtoInt(wifiClientConf.netmask, subnetint);
  gwtrue = strintIPtoInt(wifiClientConf.gw, gw);
  dns1true = strintIPtoInt(wifiClientConf.dns1, dns1);
  dns2true = strintIPtoInt(wifiClientConf.dns2, dns2);

	#ifdef DEBUG
		os_printf("###\nStatic IP Config: \nIP:%d.%d.%d.%d\n", ipint[0], ipint[1], ipint[2], ipint[3]);
		os_printf("Subnet: %d.%d.%d.%d\n", subnetint[0], subnetint[1], subnetint[2], subnetint[3]);
		os_printf("GW: %d.%d.%d.%d\n", gw[0], gw[1], gw[2], gw[3]);
		os_printf("DHCP is: %d\n###\n", wifiClientConf.dhcp);
	  #endif

  //prepare SSID and Passwd of the router
  struct station_config stationConf;
  os_bzero(&stationConf, sizeof(struct station_config));
  os_memcpy(stationConf.ssid, wifiClientConf.ssid, sizeof(wifiClientConf.ssid));
  os_memcpy(stationConf.password, wifiClientConf.password, sizeof(wifiClientConf.password));

  //Setting up wifi
  wifi_station_set_config(&stationConf);

  if(wifiClientConf.dhcp == 0){
	if(iptrue == 1 && subnettrue == 1 && gwtrue == 1){
			//char ip is not ip
			os_printf("IP or netmask or GW is not ip\n");
			//write to display - E001;
			writeNum(0b10011110, 1); // - E
			writeNum(0,0);
			writeNum(0,0);
			writeNum(2,0);
			latch();
		} else {
	    	//set static ip
	    	struct ip_info ipact;

	    	wifi_station_dhcpc_stop();
	    	wifi_softap_dhcps_stop();

	    	//set dns
	    	ip_addr_t dns1ip;
	    	ip_addr_t dns2ip;
	    	IP4_ADDR(&dns1ip, dns1[0],dns1[1],dns1[2], dns1[3]);
	    	IP4_ADDR(&dns2ip, dns2[0],dns2[1],dns2[2], dns2[3]);

	    	espconn_dns_setserver(0,&dns1ip);
	    	espconn_dns_setserver(1,&dns2ip);

	    	IP4_ADDR(&ipact.ip, ipint[0], ipint[1], ipint[2], ipint[3]);
	    	IP4_ADDR(&ipact.netmask, subnetint[0], subnetint[1], subnetint[2], subnetint[3]);
	    	IP4_ADDR(&ipact.gw, gw[0], gw[1], gw[2], gw[3]);


	    	wifi_set_ip_info(STATION_IF, &ipact);
	    	wifi_station_connect();
	    	wifi_station_set_auto_connect(1);
		}
	} else {
  	//Start connection
		wifi_station_connect();
		wifi_station_dhcpc_start();
		wifi_station_set_auto_connect(1);
	}

}

unsigned int ICACHE_FLASH_ATTR strintIPtoInt (char *ip, int *ipr)
{
    int i;
    const char * start;
    start = ip;
    int intarr[4] = {0};
    int couter = 0;

    for (i = 0; i < 4; i++) {
        char c;
        int n = 0;
        while (1) {
            c = * start;
            start++;
            if (c >= '0' && c <= '9') {
                n *= 10;
                n += c - '0';

            }
            else if ((i < 3 && c == '.') || i == 3) {
                ipr[i] = n;
                break;
            }
            else {
                return 1;
            }
        }
        if (n >= 256) {
            return 1;
        }
    }
    return 0;
}
