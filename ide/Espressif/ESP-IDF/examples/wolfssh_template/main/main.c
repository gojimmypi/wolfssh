/* main.c
 *
 * Copyright (C) 2006-2023 wolfSSL Inc.
 *
 * This file is part of wolfSSL.
 *
 * wolfSSL is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * wolfSSL is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1335, USA
 */

/* Espressif */
#include <esp_log.h>

/* wolfSSL  */
#include "user_settings.h" /* always include wolfSSL user_settings.h first */
#include <wolfssl/wolfcrypt/port/Espressif/esp32-crypt.h>
#include <wolfssl/version.h>

/* wolfSSH  */
#include <wolfssh/ssh.h>
#include <wolfssh/log.h>

/* project */
#include "main.h"

static const char* const TAG = "My Project";

void app_main(void)
{
    ESP_LOGI(TAG, "------------ wolfSSL wolfSSH template Example ----------");
    ESP_LOGI(TAG, "--------------------------------------------------------");
    ESP_LOGI(TAG, "--------------------------------------------------------");
    ESP_LOGI(TAG, "---------------------- BEGIN MAIN ----------------------");
    ESP_LOGI(TAG, "--------------------------------------------------------");
    ESP_LOGI(TAG, "--------------------------------------------------------");

    ESP_LOGI(TAG, "Hello wolfSSL!");

#ifdef DEBUG_WOLFSSH
    wolfSSH_Debugging_ON();
#else
    ESP_LOGI(TAG, "DEBUG_WOLFSSH is not defined, "
                  "so nothing will happen for teh next statement");
#endif

#ifdef HAVE_VERSION_EXTENDED_INFO
    esp_ShowExtendedSystemInfo();
#endif

#ifdef INCLUDE_uxTaskGetStackHighWaterMark
        ESP_LOGI(TAG, "Stack HWM: %d", uxTaskGetStackHighWaterMark(NULL));

        ESP_LOGI(TAG, "Stack used: %d", CONFIG_ESP_MAIN_TASK_STACK_SIZE
                                        - (uxTaskGetStackHighWaterMark(NULL)));
#endif

/* the simplest check of the wolfSSL library presence: */
#ifdef LIBWOLFSSL_VERSION_STRING
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "Found wolfSSL Version %s\n", LIBWOLFSSL_VERSION_STRING);
#else
    ESP_LOGW(TAG, "Warning: Could not find wolfSSL Version");
#endif

/* the simplest check of the wolfMQTT library presence: */
#ifdef LIBWOLFSSH_VERSION_STRING
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "Found wolfSSH Version %s\n", LIBWOLFSSH_VERSION_STRING);
    wolfSSH_Log(WS_LOG_INFO, "[wolfssh] Hello World!");
#else
    ESP_LOGW(TAG, "Warning: Could not find wolfMQTT Version");
#endif

    ESP_LOGI(TAG, "\n\nDone!\n\n"
                  "If running from idf.py monitor, press twice: Ctrl+]\n\n"
             "WOLFSSL_COMPLETE\n" /* exit keyword for wolfssl_monitor.py */
            );
} /* app_main */