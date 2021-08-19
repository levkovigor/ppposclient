
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "driver/uart.h"
#include "driver/gpio.h"
#include "tcpip_adapter.h"
#include "netif/ppp/pppos.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"
#include "netif/ppp/pppapi.h"
#include "PPPOS.h"


 #ifdef __cplusplus
extern "C" {
#endif


bool PPPOS_firststart = false;
bool PPPOS_connected = false;
bool PPPOS_started = false;
char *PPP_User = "";
char *PPP_Pass = "";
char PPPOS_out[BUF_SIZE];

/* UART */
int PPPOS_uart_num;

static const char *TAG = "status";

/* The PPP control block */
ppp_pcb *ppp;

/* The PPP IP interface */
struct netif ppp_netif;

/* PPP status callback example */
static void ppp_status_cb(ppp_pcb *pcb, int err_code, void *ctx)
{
    struct netif *pppif = ppp_netif(pcb);
    LWIP_UNUSED_ARG(ctx);

    switch (err_code) {
    case PPPERR_NONE: {
        ESP_LOGE(TAG, "status_cb: Connected\n");
    #if PPP_IPV4_SUPPORT
            ESP_LOGE(TAG, "   ipaddr_v4  = %s\n", ipaddr_ntoa(&pppif->ip_addr));
            ESP_LOGE(TAG, "   gateway  = %s\n", ipaddr_ntoa(&pppif->gw));
            ESP_LOGE(TAG, "   netmask     = %s\n", ipaddr_ntoa(&pppif->netmask));
    #endif /* PPP_IPV4_SUPPORT */
    #if PPP_IPV6_SUPPORT
            ESP_LOGE(TAG, "   ipaddr_v6 = %s\n", ip6addr_ntoa(netif_ip6_addr(pppif, 0)));
    #endif /* PPP_IPV6_SUPPORT */
          PPPOS_connected = true;
      
        break;
    }
    case PPPERR_PARAM: {
        ESP_LOGE(TAG, "status_cb: Invalid parameter\n");
        break;
    }
    case PPPERR_OPEN: {
        ESP_LOGE(TAG, "status_cb: Unable to open PPP session\n");
        break;
    }
    case PPPERR_DEVICE: {
        ESP_LOGE(TAG, "status_cb: Invalid I/O device for PPP\n");
        break;
    }
    case PPPERR_ALLOC: {
        ESP_LOGE(TAG, "status_cb: Unable to allocate resources\n");
        break;
    }
    case PPPERR_USER: {
       ESP_LOGE(TAG, "status_cb: User interrupt\n");
       PPPOS_started = false;
       PPPOS_connected = false;
       break;
    }
    case PPPERR_CONNECT: {
       ESP_LOGE(TAG, "status_cb: Connection lost\n");
       PPPOS_started = false;
       PPPOS_connected = false;
       break;
    }
    case PPPERR_AUTHFAIL: {
        ESP_LOGE(TAG, "status_cb: Failed authentication challenge\n");
        PPPOS_started = false;
        PPPOS_connected = false;
        break;
    }
    case PPPERR_PROTOCOL: {
        ESP_LOGE(TAG, "status_cb: Failed to meet protocol\n");
        PPPOS_started = false;
        PPPOS_connected = false;
        break;
    }
    case PPPERR_PEERDEAD: {
        ESP_LOGE(TAG, "status_cb: Connection timeout\n");
        PPPOS_started = false;
        PPPOS_connected = false;
        break;
    }
    case PPPERR_IDLETIMEOUT: {
        ESP_LOGE(TAG, "status_cb: Idle Timeout\n");
        PPPOS_started = false;
        PPPOS_connected = false;
        break;
    }
    case PPPERR_CONNECTTIME: {
        ESP_LOGE(TAG, "status_cb: Max connect time reached\n");
        PPPOS_started = false;
        PPPOS_connected = false;
        break;
    }
    case PPPERR_LOOPBACK: {
        ESP_LOGE(TAG, "status_cb: Loopback detected\n");
        PPPOS_started = false;
        PPPOS_connected = false;
        break;
    }
    default: {
        ESP_LOGE(TAG, "status_cb: Unknown error code %d\n", err_code);
        PPPOS_started = false;
        PPPOS_connected = false;
        break;
    }
    }

    if (err_code == PPPERR_NONE) {
        return;
    }
    if (err_code == PPPERR_USER) {
        return;
    }
}

static u32_t ppp_output_callback(ppp_pcb *pcb, u8_t *data, u32_t len, void *ctx)
{
    return uart_write_bytes(PPPOS_uart_num, (const char *)data, len);
}



static void pppos_client_task(void *pvParameters)
{
 
char* data =  (char*)malloc(BUF_SIZE);
    while (1) {  
        while (PPPOS_started) {
            memset(data, 0, BUF_SIZE);
            int len = uart_read_bytes(PPPOS_uart_num, (uint8_t *)data, BUF_SIZE, 10 / portTICK_RATE_MS);
            if (len > 0) {
                pppos_input_tcpip(ppp, (u8_t *)data, len);
            }
             vTaskDelay(100 / portTICK_PERIOD_MS);
        }
        vTaskDelay(100 / portTICK_PERIOD_MS);
       }
      
    
}
void PPPOS_init(int txPin, int rxPin, int baudrate, int uart_number, char* user, char* pass){
  PPPOS_uart_num = uart_number;
  gpio_set_direction(txPin, GPIO_MODE_OUTPUT);
  gpio_set_direction(rxPin, GPIO_MODE_INPUT);
  gpio_set_pull_mode(rxPin, GPIO_PULLUP_ONLY);
   
  uart_config_t uart_config = {
      .baud_rate = baudrate,
      .data_bits = UART_DATA_8_BITS,
      .parity = UART_PARITY_DISABLE,
      .stop_bits = UART_STOP_BITS_1,
      .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
  };

  uart_param_config(PPPOS_uart_num, &uart_config) ;
  uart_set_pin(PPPOS_uart_num, txPin, rxPin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
  uart_driver_install(PPPOS_uart_num, BUF_SIZE * 2, BUF_SIZE * 2, 0, NULL, 0);   
  tcpip_adapter_init();
  PPP_User = user;
  PPP_Pass = pass;
  xTaskCreate(&pppos_client_task, "pppos_client_task", 10048, NULL, 5, NULL);
}

bool PPPOS_isConnected(){
  return PPPOS_connected;
}

void PPPOS_start(){
  if (!PPPOS_firststart){
        ppp = pppapi_pppos_create(&ppp_netif, ppp_output_callback, ppp_status_cb, NULL);
     
        if (ppp == NULL) {
            return;
        }
  }
        pppapi_set_default(ppp);
        pppapi_set_auth(ppp, PPPAUTHTYPE_PAP, PPP_User, PPP_Pass);
        ppp_set_usepeerdns(ppp, 1);
        pppapi_connect(ppp, 0);
        
        PPPOS_started = true;
        PPPOS_firststart = true;
}

bool PPPOS_status(){
  return PPPOS_started;
}

void PPPOS_stop(){
  pppapi_close(ppp, 0); 
}

/*void gsmInit(int txPin, int rxPin, int baudrate, int uart_number){
  PPPOS_uart_num = uart_number;
  gpio_set_direction(txPin, GPIO_MODE_OUTPUT);
  gpio_set_direction(rxPin, GPIO_MODE_INPUT);
  gpio_set_pull_mode(rxPin, GPIO_PULLUP_ONLY);
   
    uart_config_t uart_config = {
      .baud_rate = baudrate,
      .data_bits = UART_DATA_8_BITS,
      .parity = UART_PARITY_DISABLE,
      .stop_bits = UART_STOP_BITS_1,
      .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
  };
    uart_param_config(PPPOS_uart_num, &uart_config) ;
    uart_set_pin(PPPOS_uart_num, txPin, rxPin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    uart_driver_install(PPPOS_uart_num, BUF_SIZE * 2, BUF_SIZE * 2, 0, NULL, 0);   
}*/

char* PPPOS_read(){
  memset(PPPOS_out, 0, BUF_SIZE);
  int len = uart_read_bytes(PPPOS_uart_num, (uint8_t *)PPPOS_out, BUF_SIZE, 10 / portTICK_RATE_MS);
  if (len > 0) {
    return PPPOS_out;
  } else {
    return NULL;
  }
}

void PPPOS_write(char* cmd){
  uart_flush(PPPOS_uart_num);
  if (cmd != NULL) {
     int cmdSize = strlen(cmd);
     uart_write_bytes(PPPOS_uart_num, (const char*)cmd, cmdSize);
     uart_wait_tx_done(PPPOS_uart_num, 100 / portTICK_RATE_MS);
  }
}

#ifdef __cplusplus
}
#endif
