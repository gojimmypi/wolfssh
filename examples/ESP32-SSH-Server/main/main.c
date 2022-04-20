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
#include "esp_eth.h"
#include "esp_event.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"

#ifdef USE_ENC28J60
    #include <enc28j69_helper.h>
#endif

/* wolfSSL */
#define DEBUG_WOLFSSL
#define DEBUG_WOLFSSH

#define WOLFSSL_ESPIDF
#define WOLFSSL_ESPWROOM32
#define WOLFSSL_USER_SETTINGS

#define WOLFSSH_TEST_THREADING
#define NO_FILESYSTEM
#include <wolfssl/wolfcrypt/settings.h> /* make sure this appears before any other wolfSSL headers */
#include <wolfssl/wolfcrypt/logging.h>
#include <wolfssl/ssl.h>

#include "wifi.h"
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


int set_time() {
    /* we'll also return a result code of zero */
    int res = 0;

    //*ideally, we'd like to set time from network, but let's set a default time, just in case */
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


#include "driver/uart.h"

void init_UART(void) {
    ESP_LOGI(TAG, "Begin init_UART.");    
    const uart_config_t uart_config = {
        .baud_rate = BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_APB,
    };
    int intr_alloc_flags = 0;
    
#if CONFIG_UART_ISR_IN_IRAM
    intr_alloc_flags = ESP_INTR_FLAG_IRAM;
#endif    
    // We won't use a buffer for sending data.
    ESP_ERROR_CHECK(uart_driver_install(UART_NUM_1, RX_BUF_SIZE * 2, 0, 0, NULL, intr_alloc_flags));
    ESP_ERROR_CHECK(uart_param_config(UART_NUM_1, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(UART_NUM_1, TXD_PIN, RXD_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

    ESP_LOGI(TAG, "End init_UART.");    
}

void server_session(void* args)
{
    while (1)
    {
        server_test(args);
        vTaskDelay(DelayTicks ? DelayTicks : 1); /* Minimum delay = 1 tick */
        // esp_task_wdt_reset();
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
    
    /* WiFi is pretty much always available on the ESP32 */
    if (wifi_ready()) {
        ret = false;
    }

    return ret;
}

void init() {
    ESP_LOGI(TAG, "Begin main init.");
#ifdef DEBUG_WOLFSSH
    wolfSSH_Debugging_ON();
#endif

    
#ifdef DEBUG_WOLFSSL
    WOLFSSL_MSG("Debug ON");
    wolfSSL_Debugging_ON();
    //ShowCiphers();
#endif

    init_UART();
    
#ifdef USE_ENC28J60
    ESP_LOGI(TAG, "Found USE_ENC28J60.");
    init_ENC28J60();
#else
    ESP_LOGI(TAG, "Setting up nvs flash for WiFi.");
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    ESP_LOGI(TAG, "Begin setup WiFi STA.");
    wifi_init_sta();
    ESP_LOGI(TAG, "End setup WiFi STA.");
#endif

    
    TickType_t EthernetWaitDelayTicks = 1000 / portTICK_PERIOD_MS;

       
    while (NoEthernet()) {
        WOLFSSL_MSG("Waiting for ethernet...");
        vTaskDelay(EthernetWaitDelayTicks ? EthernetWaitDelayTicks : 1);
    }
    
    // one of the most important aspects of security is the time and date values
    set_time();

    WOLFSSL_MSG("inet_pton"); /* TODO */
    
    wolfSSH_Init(); 
}

/**
 * @brief Checks the netif description if it contains specified prefix.
 * All netifs created withing common connect component are prefixed with the module TAG,
 * so it returns true if the specified netif is owned by this module
 */
static bool is_our_netif(const char *prefix, esp_netif_t *netif) {
    return strncmp(prefix, esp_netif_get_desc(netif), strlen(prefix) - 1) == 0;
}
#define CONFIG_BLINK_PERIOD 1000
static uint8_t s_led_state = 0;

void app_main(void) {
    init();
    // note that by the time we get here, the scheduler is already running!
    // see https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/freertos.html#esp-idf-freertos-applications
    // Unlike Vanilla FreeRTOS, users must not call vTaskStartScheduler();

        
    // all of the tasks are at the same, highest idle priority, so they will all get equal attention
    // when priority was set to configMAX_PRIORITIES - [1,2,3] there was an odd WDT timeout warning.
    xTaskCreate(uart_rx_task, "uart_rx_task", 1024 * 2, NULL, tskIDLE_PRIORITY, NULL);
    
    xTaskCreate(uart_tx_task, "uart_tx_task", 1024 * 2, NULL, tskIDLE_PRIORITY, NULL);

    xTaskCreate(server_session, "server_session", 6024 * 2, NULL, tskIDLE_PRIORITY, NULL);

    
    for (;;) {
        /* we're not actually doing anything here, other than a heartbeat message */
        WOLFSSL_MSG("main loop!");
        ESP_LOGI(TAG, "Loop!");

        /* esp_err_tesp_netif_get_ip_info(esp_netif_t *esp_netif, esp_netif_ip_info_t *ip_info) */ 
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
        // esp_task_wdt_reset();
    }

    // todo this is unreachable with RTOS threads, do we ever want to shut down?
    wolfSSH_Cleanup();
}
