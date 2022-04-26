/* ssh_server.h
 *
 * Copyright (C) 2014-2022 wolfSSL Inc.
 *
 * This file is part of wolfSSH.
 *
 * wolfSSH is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * wolfSSH is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with wolfSSH.  If not, see <http://www.gnu.org/licenses/>.
 * 
 * Adapted from Public Domain Expressif ENC28J60 Example
 * 
 * https://github.com/espressif/esp-idf/blob/047903c612e2c7212693c0861966bf7c83430ebf/examples/ethernet/enc28j60/main/enc28j60_example_main.c#L1
 */

/* include ssh_server_config.h first  */
#include "ssh_server_config.h"

#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "driver/gpio.h"

/* see ssh_server_config.h for optional use of physical ethernet: USE_ENC28J60 */
#ifdef USE_ENC28J60
    #include <enc28j60_helper.h>
#endif

/*
 * wolfSSL
 *
 * IMPORTANT: Ensure wolfSSL settings.h appears before any other wolfSSL headers
 * 
 * Example locations:

 *   Standard ESP-IDF:
 *   C:\Users\[username]\Desktop\esp-idf\components\wolfssh\wolfssl\wolfcrypt\settings.h
 * 
 *   VisualGDB
 *   C:\SysGCC\esp32\esp-idf\[version]\components\wolfssl\wolfcrypt\settings.h
 *   
 **/
#define DEBUG_WOLFSSL
#define DEBUG_WOLFSSH

#define WOLFSSL_TLS13
#define HAVE_TLS_EXTENSIONS
#define HAVE_SUPPORTED_CURVES
#define HAVE_ECC
#define HAVE_HKDF
#define HAVE_FFDHE_8192 // or one of the other supported FFDHE sizes [2048, 3072, 4096, 6144, 8192]
#define WC_RSA_PSS
#define WOLFSSL_USER_SETTINGS

#define WOLFSSH_TEST_THREADING

/*  note "file system": "load keys and certificate from files" vs NO_FILESYSTEM 
 *  and "access an actual filesystem via SFTP/SCP" vs WOLFSSH_NO_FILESYSTEM 
 *  we'll typically have neither on an embedded device:
 */
#define NO_FILESYSTEM
#define WOLFSSH_NO_FILESYSTEM

#include <wolfssl/wolfcrypt/settings.h> /* make sure this appears before any other wolfSSL headers */
#include <wolfssl/wolfcrypt/logging.h>
#include <wolfssl/ssl.h>

#ifdef USE_ENC28J60
    /* no WiFi when using external ethernet */
#else
    #include "wifi.h"
#endif

#include "ssh_server.h"

/* logging 
 * see https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/log.html 
 */
#ifdef LOG_LOCAL_LEVEL
    #undef LOG_LOCAL_LEVEL
#endif    
#define LOG_LOCAL_LEVEL ESP_LOG_INFO
#include "esp_log.h"

/* time */
#include  <lwip/apps/sntp.h>

static const char *TAG = "SSH Server main";

static TickType_t DelayTicks = 10000 / portTICK_PERIOD_MS;


int set_time() {
    /* we'll also return a result code of zero */
    int res = 0;

    /* ideally, we'd like to set time from network, but let's set a default time, just in case */
    struct tm timeinfo;
    timeinfo.tm_year = 2022 - 1900;
    timeinfo.tm_mon = 4;
    timeinfo.tm_mday = 17;
    timeinfo.tm_hour = 10;
    timeinfo.tm_min = 46;
    timeinfo.tm_sec = 10;
    time_t t;
    t = mktime(&timeinfo);

    struct timeval now = { .tv_sec = t };
    settimeofday(&now, NULL);

    /* set timezone */
    setenv("TZ", TIME_ZONE, 1);
    tzset();

    /* next, let's setup NTP time servers
     *
     * see https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/system_time.html#sntp-time-synchronization
    */
    sntp_setoperatingmode(SNTP_OPMODE_POLL);

    int i = 0;
    WOLFSSL_MSG("sntp_setservername:");
    for (i = 0; i < NTP_SERVER_COUNT; i++) {
        const char* thisServer = ntpServerList[i];
        if (strncmp(thisServer, "\x00", 1) == 0) {
            /* just in case we run out of NTP servers */
            break;
        }
        WOLFSSL_MSG(thisServer);
        sntp_setservername(i, thisServer);
    }
    sntp_init();
    WOLFSSL_MSG("sntp_init done.");
    return res;
}



#define BUF_SIZE (1024)
#define RD_BUF_SIZE (BUF_SIZE)
static QueueHandle_t uart0_queue;

void init_UART(void) {
    

    // Configure parameters of an UART driver,
    // communication pins and install the driver
    /* swap GPIO pins 1,3 with 13,15 */
    uart_enable_swap();

    uart_config_t uart_config = {
        .baud_rate = BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
    };
    uart_param_config(EX_UART_NUM, &uart_config);

    // Install UART driver, and get the queue.
    uart_driver_install(EX_UART_NUM, BUF_SIZE * 2, BUF_SIZE * 2, 100, &uart0_queue, 0);

    /* UART 1 is Tx only! */
/*
//    uart_config_t uart_config1 = {
        .baud_rate = 74800,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
    };
    uart_param_config(UART_NUM_1, &uart_config1);
*/
    // Install UART driver, and get the queue.
//    uart_driver_install(UART_NUM_1, BUF_SIZE * 2, BUF_SIZE * 2, 100, &uart0_queue, 0);

    /* use RTOS menuconfig to set logout output to UART1 */

}

void server_session(void* args)
{
    while (1)
    {
        server_test(args);
        vTaskDelay(DelayTicks ? DelayTicks : 1); /* Minimum delay = 1 tick */
        /* esp_task_wdt_reset(); */
    }
}

/*
 * there may be any one of multiple ethernet interfaces
 * do we have one or not?
 **/
bool NoEthernet()
{
    bool ret = true;
#ifdef USE_ENC28J60
    /* the ENC28J60 is only available if one has been installed  */
    if (EthernetReady_ENC28J60()) { 
        ret = false;
    }
#endif
    
#ifndef USE_ENC28J60
    /* WiFi is pretty much always available on the ESP32 */
    if (wifi_ready()) {
        ret = false;
    }
#endif
    
    return ret;
}

/*
 * main initialization for UART, optional ethernet, time, etc.
 */
void init() {
    ESP_LOGI(TAG, "Begin main init.");
    
#ifdef DEBUG_WOLFSSH
    ESP_LOGI(TAG, "wolfSSH debugging on.");
    wolfSSH_Debugging_ON();
#endif

    
#ifdef DEBUG_WOLFSSL
    ESP_LOGI(TAG, "wolfSSL debugging on.");
    wolfSSL_Debugging_ON();
    WOLFSSL_MSG("Debug ON");
    /* TODO ShowCiphers(); */
#endif

    init_UART();
    
#ifdef USE_ENC28J60
    ESP_LOGI(TAG, "Found USE_ENC28J60 config.");
    init_ENC28J60();
#else
    ESP_LOGI(TAG, "Setting up nvs flash for WiFi.");
    esp_err_t ret = nvs_flash_init();
//    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
//        ESP_ERROR_CHECK(nvs_flash_erase());
//        ret = nvs_flash_init();
//    }
//    ESP_ERROR_CHECK(ret);
    
    ESP_LOGI(TAG, "Begin setup WiFi STA.");
    wifi_init_sta();
    ESP_LOGI(TAG, "End setup WiFi STA.");
#endif

    
    TickType_t EthernetWaitDelayTicks = 1000 / portTICK_PERIOD_MS;

       
    while (NoEthernet()) {
        WOLFSSL_MSG("Waiting for ethernet...");
        vTaskDelay(EthernetWaitDelayTicks ? EthernetWaitDelayTicks : 1);
    }
    
    /* one of the most important aspects of security is the time and date values */
    set_time();

    WOLFSSL_MSG("inet_pton"); /* TODO */
    
    wolfSSH_Init(); 
}

/**
 * @brief Checks the netif description if it contains specified prefix.
 * All netifs created withing common connect component are prefixed with the module TAG,
 * so it returns true if the specified netif is owned by this module
TODO

static bool is_our_netif(const char *prefix, esp_netif_t *netif) {
    return strncmp(prefix, esp_netif_get_desc(netif), strlen(prefix) - 1) == 0;
}

*/

void app_main(void) {
    init();
    /* note that by the time we get here, the scheduler is already running!
     * see https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/freertos.html#esp-idf-freertos-applications
     * Unlike Vanilla FreeRTOS, users must not call vTaskStartScheduler();
     *        
     * all of the tasks are at the same, highest idle priority, so they will all get equal attention
     * when priority was set to configMAX_PRIORITIES - [1,2,3] there was an odd WDT timeout warning.
     */
    xTaskCreate(uart_rx_task, "uart_rx_task", 1024 * 2, NULL, tskIDLE_PRIORITY, NULL);
    
    xTaskCreate(uart_tx_task, "uart_tx_task", 1024 * 2, NULL, tskIDLE_PRIORITY, NULL);

    xTaskCreate(server_session, "server_session", 6024 * 2, NULL, tskIDLE_PRIORITY, NULL);

    
    for (;;) {
        /* we're not actually doing anything here, other than a heartbeat message */
        WOLFSSL_MSG("main loop!");
        ESP_LOGI(TAG, "Loop!");

        /* esp_err_tesp_netif_get_ip_info(esp_netif_t *esp_netif, esp_netif_ip_info_t *ip_info) */ 
        /* TODO print IP address */
//        esp_netif_t *netif = NULL;
//        esp_netif_ip_info_t ip;
//        esp_err_t ret;
//        netif = esp_netif_next(netif);
//        ret = esp_netif_get_ip_info(netif, &ip);
//        if (ret == ESP_OK) {
//            ESP_LOGI(TAG, "- IPv4 address: " IPSTR, IP2STR(&ip.ip));
//            ESP_LOGI(TAG, "       netmask: " IPSTR, IP2STR(&ip.netmask));
//            ESP_LOGI(TAG, "       gateway: " IPSTR, IP2STR(&ip.gw));
//        }
        
        taskYIELD();
        vTaskDelay(DelayTicks ? DelayTicks : 1); /* Minimum delay = 1 tick */
    }

    /* todo this is unreachable with RTOS threads, do we ever want to shut down? */
    wolfSSH_Cleanup();
}
