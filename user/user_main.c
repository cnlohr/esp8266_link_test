//Copyright 2015 <>< Charles Lohr, see LICENSE file.

#include "mem.h"
#include "c_types.h"
#include "user_interface.h"
#include "ets_sys.h"
#include "driver/uart.h"
#include "osapi.h"
#include "espconn.h"
#include "mystuff.h"
#include "ws2812_i2s.h"


#define PORT 7777
#define REF_PORT 9999

#define procTaskPrio        0
#define procTaskQueueLen    1

static volatile os_timer_t some_timer;
static struct espconn *pUdpServer;
static struct espconn *pUdpServer9999;
uint8_t last_leds[512*3];
int last_led_count;


void user_rf_pre_init(void)
{
	//nothing.
}


char * strcat( char * dest, char * src )
{
	return strcat(dest, src );
}



//Tasks that happen all the time.

os_event_t    procTaskQueue[procTaskQueueLen];

static void ICACHE_FLASH_ATTR procTask(os_event_t *events)
{
	system_os_post(procTaskPrio, 0, 0 );
}

//Timer event.
static void ICACHE_FLASH_ATTR myTimer(void *arg)
{
	int i;
	ws2812_push( last_leds, last_led_count );
	for( i = 0; i < last_led_count*3; i++ )
	{
		int k = last_leds[i];
		if( k )
			k -= 10;
		if( k < 10 ) last_leds[i] = 0;
		else last_leds[i] = k;
	}

#ifndef SERVER
	printf( " RSSI: %d\n", wifi_station_get_rssi() );

	uint32_to_IP4( ((uint32_t)0xffffffff), pUdpServer->proto.udp->remote_ip );
	pUdpServer->proto.udp->remote_port = 9999;
	espconn_sent( (struct espconn *)pUdpServer, "!!!", 3 );
#endif

}

#ifdef SERVER
static void ICACHE_FLASH_ATTR udpserver_recv_9999(void *arg, char *pusrdata, unsigned short len)
{
	remot_info * ri = 0;
	struct espconn *rc = (struct espconn *)arg;
	espconn_get_connection_info( rc, &ri, 0);
	printf( "Got 9999 %d %d\n", pUdpServer->proto.udp->remote_port, pUdpServer->proto.udp->local_port );

	//Unicast packet from whence it came.
	ets_memcpy( rc->proto.udp->remote_ip, ri->remote_ip, 4 );
	rc->proto.udp->remote_port = ri->remote_port;
	espconn_sendto( rc, "\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff", 15 );

}

#endif
//Called when new packet comes in.
static void ICACHE_FLASH_ATTR udpserver_recv(void *arg, char *pusrdata, unsigned short len)
{
	struct espconn *pespconn = (struct espconn *)arg;

	uart0_sendStr("X");

	len -= 3;
	if( len > sizeof(last_leds) + 3 )
	{
		len = sizeof(last_leds) + 3;
	}
	ets_memcpy( last_leds, pusrdata+3, len );
	last_led_count = len / 3;
}

void ICACHE_FLASH_ATTR charrx( uint8_t c )
{
	//Called from UART.
}

void user_init(void)
{
	uart_init(BIT_RATE_115200, BIT_RATE_115200);

	uart0_sendStr("\r\n\033cesp8266 ws2812 driver\r\n");

//Uncomment this to force a system restore.
//	system_restore();

    pUdpServer = (struct espconn *)os_zalloc(sizeof(struct espconn));
	ets_memset( pUdpServer, 0, sizeof( struct espconn ) );
	espconn_create( pUdpServer );
	pUdpServer->type = ESPCONN_UDP;
	pUdpServer->proto.udp = (esp_udp *)os_zalloc(sizeof(esp_udp));
	pUdpServer->proto.udp->local_port = 7777;
	espconn_regist_recvcb(pUdpServer, udpserver_recv);

	if( espconn_create( pUdpServer ) )
	{
		while(1) { uart0_sendStr( "\r\nFAULT\r\n" ); }
	}


#ifdef SERVER
    pUdpServer9999 = (struct espconn *)os_zalloc(sizeof(struct espconn));
	ets_memset( pUdpServer9999, 0, sizeof( struct espconn ) );
	espconn_create( pUdpServer9999 );
	pUdpServer9999->type = ESPCONN_UDP;
	pUdpServer9999->proto.udp = (esp_udp *)os_zalloc(sizeof(esp_udp));
	pUdpServer9999->proto.udp->local_port = REF_PORT;
	espconn_regist_recvcb(pUdpServer9999, udpserver_recv_9999);

	if( espconn_create( pUdpServer9999 ) )
	{
		while(1) { uart0_sendStr( "\r\nFAULT\r\n" ); }
	}
#endif

	//Set GPIO16 for Input
    WRITE_PERI_REG(PAD_XPD_DCDC_CONF, (READ_PERI_REG(PAD_XPD_DCDC_CONF) & 0xffffffbc) | (uint32)0x1); 	// mux configuration for XPD_DCDC and rtc_gpio0 connection
    WRITE_PERI_REG(RTC_GPIO_CONF, (READ_PERI_REG(RTC_GPIO_CONF) & (uint32)0xfffffffe) | (uint32)0x0);	//mux configuration for out enable
    WRITE_PERI_REG(RTC_GPIO_ENABLE, READ_PERI_REG(RTC_GPIO_ENABLE) & (uint32)0xfffffffe);	//out disable

	//Add a process
	system_os_task(procTask, procTaskPrio, procTaskQueue, procTaskQueueLen);

	//Timer example
	os_timer_disarm(&some_timer);
	os_timer_setfn(&some_timer, (os_timer_func_t *)myTimer, NULL);
	os_timer_arm(&some_timer, 30, 1);

	ws2812_init();


#ifdef SERVER

	//You are the host AP

	uint8_t ledout[] = { 0x00, 0xFF, 0xFF };
	ws2812_push( ledout, sizeof( ledout ) );

	struct softap_config sap;
	wifi_softap_get_config_default( &sap );
	os_strcpy(&sap.ssid, "rangetestX" ); //Not sure why the 'X' is needed.
	sap.ssid_len = 10;
	os_strcpy(&sap.password, "soldering" );
    sap.authmode = AUTH_WPA2_PSK;
	wifi_softap_set_config_current( &sap );
	wifi_softap_set_config( &sap );
	wifi_set_opmode_current( 2 );
	wifi_set_opmode( 2 );

#else
	uint8_t ledout[] = { 0x00, 0x00, 0xFF };
	ws2812_push( ledout, sizeof( ledout ) );

	struct station_config stationConf;
	wifi_station_get_config(&stationConf);
	os_strcpy(&stationConf.ssid, "rangetest" );
	os_strcpy(&stationConf.password, "soldering" );
	stationConf.bssid_set = 0;
	printf( "-->'%s'\n",stationConf.ssid);
	printf( "-->'%s'\n",stationConf.password);
	wifi_station_set_config(&stationConf);
	wifi_station_set_config_current(&stationConf);
	wifi_set_opmode_current( 1 );
	wifi_set_opmode( 1 );
	wifi_station_connect();

#endif


	printf( "Boot Ok.\n" );
	printf( "Range Tester Running.\n" );

    wifi_set_sleep_type(LIGHT_SLEEP_T);
    wifi_fpm_set_sleep_type(LIGHT_SLEEP_T);


	system_os_post(procTaskPrio, 0, 0 );
}


//There is no code in this project that will cause reboots if interrupts are disabled.
void EnterCritical()
{
}

void ExitCritical()
{
}


