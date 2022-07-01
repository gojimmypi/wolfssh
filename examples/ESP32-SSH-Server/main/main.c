/* main.c
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
 * Adapted from Public Domain Espressif ENC28J60 Example
 *
 * https://github.com/espressif/esp-idf/blob/047903c612e2c7212693c0861966bf7c83430ebf/examples/ethernet/enc28j60/main/enc28j60_example_main.c#L1
 *
 *
 * WARNING: although this code makes use of the UART #2 (as UART_NUM_0)
 *
 * DO NOT LEAVE ANYTHING CONNECTED TO TXD2 (GPIO 15) and RXD2 (GPIO 13)
 *
 * In particular, GPIO 15 must not be high during programming.
 *
 */

/* WOLFSSL_USER_SETTINGS is defined here only for the syntax highlighter
 * see CMakeLists.txt
#define WOLFSSL_USER_SETTINGS
 */

#include "sdkconfig.h"

/* include ssh_server_config.h first  */
#include "my_config.h"
#include "ssh_server_config.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_netif.h>
#include <esp_event.h>
#include <driver/gpio.h>

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
#ifdef WOLFSSL_STALE_EXAMPLE
    #warning "This project is configured using local, stale wolfSSL code. See Makefile."
#endif

#define DEBUG_WOLFSSL
#define DEBUG_WOLFSSH

#define WOLFSSL_ESPIDF
#define WOLFSSL_ESPWROOM32

#define WOLFSSL_TLS13
#define HAVE_TLS_EXTENSIONS
#define HAVE_SUPPORTED_CURVES
#define HAVE_ECC
#define HAVE_HKDF
#define HAVE_FFDHE_8192 /* or one of the other supported FFDHE sizes [2048, 3072, 4096, 6144, 8192] */
#define WC_RSA_PSS
#define WOLFSSH_TEST_THREADING

/*  note "file system": "load keys and certificate from files" vs NO_FILESYSTEM
 *  and "access an actual file system via SFTP/SCP" vs WOLFSSH_NO_FILESYSTEM
 *  we'll typically have neither on an embedded device:
 */
#define NO_FILESYSTEM
#define WOLFSSH_NO_FILESYSTEM

/* TODO check wolfSSL config
 * #include <wolfssl/include/user_settings.h>
 * make sure this appears before any other wolfSSL headers
 */

#include <wolfssl/wolfcrypt/logging.h>
#include <wolfssl/ssl.h>

/* optional memory tracking */
/* #define WOLFSSL_TRACK_MEMORY */
#define WOLFSSL_TRACK_MEMORY
#ifdef WOLFSSL_TRACK_MEMORY
    #include <wolfssl/wolfcrypt/mem_track.h>
#endif

#ifdef USE_ENC28J60
    /* no WiFi when using external ethernet */
#else
    #include "wifi.h"
#endif

#include "ssh_server.h"

/* logging
 *
 * see
 *   https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/log.html
 *   https://github.com/wolfSSL/wolfssl/blob/master/wolfssl/wolfcrypt/logging.h
 */
#ifdef LOG_LOCAL_LEVEL
    #undef LOG_LOCAL_LEVEL
#endif
#define LOG_LOCAL_LEVEL ESP_LOG_INFO
#include "esp_log.h"

/* time */
#include  <lwip/apps/sntp.h>

#include "test_task.h"

static const char *TAG = "SSH Server main";

/* 10 seconds, used for heartbeat message in thread */
static TickType_t DelayTicks = (10000 / portTICK_PERIOD_MS);


int set_time(void)
{
    /* we'll also return a result code of zero */
    int res = 0;
    int i = 0; /* counter for time servers */
    time_t interim_time;

    /* ideally, we'd like to set time from network,
     * but let's set a default time, just in case */
    struct tm timeinfo = {
        .tm_year = 2022 - 1900,
        .tm_mon = 6,
        .tm_mday = 29,
        .tm_hour = 10,
        .tm_min = 46,
        .tm_sec = 10
    };
    struct timeval now;

    /* set interim static time */
    interim_time = mktime(&timeinfo);
    now = (struct timeval){ .tv_sec = interim_time };
    settimeofday(&now, NULL);

    /* set timezone */
    setenv("TZ", TIME_ZONE, 1);
    tzset();

    /* next, let's setup NTP time servers
     *
     * see https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/system_time.html#sntp-time-synchronization
     */
    sntp_setoperatingmode(SNTP_OPMODE_POLL);

    ESP_LOGI(TAG, "sntp_setservername:");
    for (i = 0; i < NTP_SERVER_COUNT; i++) {
        const char* thisServer = ntpServerList[i];
        if (strncmp(thisServer, "\x00", 1) == 0) {
            /* just in case we run out of NTP servers */
            break;
        }
        ESP_LOGI(TAG, "%s", thisServer);
        sntp_setservername(i, thisServer);
    }
    sntp_init();
    ESP_LOGI(TAG, "sntp_init done.");
    return res;
}


#include "driver/uart.h"


void server_session(void* args)
{
    while (1) {
        server_test(args);
        vTaskDelay(DelayTicks ? DelayTicks : 1); /* Minimum delay = 1 tick */

#ifdef DEBUG_WDT
        /* if we get panic faults, perhaps the watchdog needs attention? */
        esp_task_wdt_reset();
#endif
    }
}

/*
 * there may be any one of multiple ethernet interfaces
 * do we have one or not?
 **/
bool NoEthernet(void)
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

#if defined(WOLFSSH_SERVER_IS_AP) || defined(WOLFSSH_SERVER_IS_STA)
void init_nvsflash(void)
{
    ESP_LOGI(TAG, "Setting up nvs flash for WiFi.");
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES
          ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {

        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
}
#endif

/*
 * main initialization for UART, optional ethernet, time, etc.
 */
void init(void)
{
    TickType_t EthernetWaitDelayTicks = (1000 / portTICK_PERIOD_MS);

    ESP_LOGI(TAG, "Begin main init.");

#ifdef DEBUG_WOLFSSH
    ESP_LOGI(TAG, "wolfSSH debugging on.");
    wolfSSH_Debugging_ON();
#endif


#ifdef DEBUG_WOLFSSL
    ESP_LOGI(TAG, "wolfSSL debugging on.");
    wolfSSL_Debugging_ON();
    ESP_LOGI(TAG, "Debug ON");
    /* TODO ShowCiphers(); */
#endif

    init_UART(TXD_PIN, RXD_PIN, BAUD_RATE);

    /*
     * here we have one of three options:
     *
     * Wired Ethernet: USE_ENC28J60
     *
     * WiFi Access Point: WOLFSSH_SERVER_IS_AP
     *
     * WiFi Station: WOLFSSH_SERVER_IS_STA
     **/
#if defined(USE_ENC28J60)
    /* wired ethernet */
    ESP_LOGI(TAG, "Found USE_ENC28J60 config.");
    init_ENC28J60(MY_MAC_ADDRESS);

#elif defined( WOLFSSH_SERVER_IS_AP)
    /* acting as an access point */
    init_nvsflash();

    ESP_LOGI(TAG, "Begin setup WiFi Soft AP.");
    wifi_init_softap();
    ESP_LOGI(TAG, "End setup WiFi Soft AP.");

#elif defined(WOLFSSH_SERVER_IS_STA)
    /* acting as a WiFi Station (client) */
    init_nvsflash();

    ESP_LOGI(TAG, "Begin setup WiFi STA.");
    wifi_init_sta();
    ESP_LOGI(TAG, "End setup WiFi STA.");
#else
    /* we should never get here */
    while (1)
    {
        ESP_LOGE(TAG, "ERROR: No network is defined... choose USE_ENC28J60, \
                          WOLFSSH_SERVER_IS_AP, or WOLFSSH_SERVER_IS_STA ");
        vTaskDelay(EthernetWaitDelayTicks ? EthernetWaitDelayTicks : 1);
    }
#endif

    while (NoEthernet()) {
        ESP_LOGI(TAG, "Waiting for ethernet...");
        vTaskDelay(EthernetWaitDelayTicks ? EthernetWaitDelayTicks : 1);
    }

    /* one of the most important aspects of security is the time and date values */
    set_time();

    ESP_LOGI(TAG, "inet_pton"); /* TODO */

#ifdef WOLFSSL_TRACK_MEMORY
    InitMemoryTracker();
#endif


    wolfSSH_Init();

#ifdef WOLFSSL_TRACK_MEMORY
    ShowMemoryTracker();
#endif

}


void app_main(void)
{
    init();

    /* note that by the time we get here, the scheduler is already running!
     * see https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/freertos.html#esp-idf-freertos-applications
     * Unlike Vanilla FreeRTOS, users must not call vTaskStartScheduler();
     *
     * all of the tasks are at the same, highest idle priority, so they will all get equal attention
     * when priority was set to configMAX_PRIORITIES - [1,2,3] there was an odd WDT timeout warning.
     */
    xTaskCreate(uart_rx_task, "uart_rx_task", 1024 * 2, NULL,
                tskIDLE_PRIORITY, NULL);

    xTaskCreate(uart_tx_task, "uart_tx_task", 1024 * 2, NULL,
                tskIDLE_PRIORITY, NULL);

    xTaskCreate(server_session, "server_session", 6024 * 2, NULL,
                tskIDLE_PRIORITY, NULL);

#ifdef ENABLE_TEST_TASK
    xTaskCreate(test_task,
        "test_task",
        6024 * 2,
        NULL,
        tskIDLE_PRIORITY,
        NULL);
#endif

    for (;;) {
        /* we're not actually doing anything here, other than a heartbeat message */
        ESP_LOGI(TAG, "wolfSSH Server main loop heartbeat!");

        taskYIELD();
        vTaskDelay(DelayTicks ? DelayTicks : 1); /* Minimum delay = 1 tick */
    }

    /* TODO this is unreachable with RTOS threads, do we ever want to shut down? */
    wolfSSH_Cleanup();
}
