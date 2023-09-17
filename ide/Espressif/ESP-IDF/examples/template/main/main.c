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
#include <wolfssl/wolfcrypt/port/Espressif/esp32-crypt.h>
#include <wolfssh/ssh.h>
#include <wolfssh/log.h>

/* project */
#include "main.h"

static const char* const TAG = "My Project";

void app_main(void)
{
    ESP_LOGI(TAG, "Hello wolfSSL!");

#ifdef HAVE_VERSION_EXTENDED_INFO
    esp_ShowExtendedSystemInfo();
#endif

#ifdef DEBUG_WOLFSSH
    wolfSSH_Debugging_ON();
#else
    ESP_LOGI(TAG, "DEBUG_WOLFSSH is not defined, "
                  "so nothing will happen for teh next statement");
#endif
    wolfSSH_Log(WS_LOG_INFO, "[wolfssh] Hello World!");

    ESP_LOGI(TAG, "\n\nDone! \n\n"
                  "WOLFSSL_COMPLETE\n" /* exit keyword for wolfssl_monitor.py */
                  "If running from idf.py monitor, press twice: Ctrl+]\n\n"
            );
}
