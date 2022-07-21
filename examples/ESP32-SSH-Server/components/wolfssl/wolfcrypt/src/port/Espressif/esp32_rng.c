#include <esp_wifi.h>
/* esp32_rng.c
 *
 * Copyright (C) 2006-2021 wolfSSL Inc.
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
#ifdef HAVE_CONFIG_H
    #include <config.h>
#endif

#include <wolfssl/wolfcrypt/settings.h>

#include "esp_types.h"
#include <esp_random.h>
#ifdef BT_ENABLED
    #include "esp_bt_main.h"
#endif
#if CONFIG_ESP32_WIFI_ENABLED
    #include <esp_wifi_types.h>
#endif

#include "sdkconfig.h"

#ifdef USE_BOOTLOADER_RANDOM
    #include "bootloader_random.h"
#endif


int wc_esp_fill_random()
{
    wifi_mode_t *mode[1] = { };

    int ret = 0;
    bool needSpeciailInit = 1;
    int isUsingWiFi = 0;

    /* BT_ENABLED may be disabled in sdkconfig/make menuconfig*/
#ifdef BT_ENABLED
    esp_bluedroid_status_t isUsingBlueTooth = 0;
#endif

    /* this is one of many functions to detect if WiFi is initialized
     *
     * see https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/network/esp_wifi.html
     */
    isUsingWiFi = esp_wifi_get_mode(mode[0]);

#ifdef BT_ENABLED
    isUsingBlueTooth = esp_bluedroid_get_status();
    /* result is only success, fail, or invalid argument */
    needSpeciailInit = (isUsingWiFi == ESP_ERR_WIFI_NOT_INIT)
                            &&
                       (isUsingBlueTooth == ESP_BLUEDROID_STATUS_UNINITIALIZED);
#else
    /* result is only success, fail, or invalid argument */
    needSpeciailInit = (isUsingWiFi == ESP_ERR_WIFI_NOT_INIT);
#endif

    if (needSpeciailInit) {
        /* This function is not safe to use if any other subsystem is
         * accessing the RF subsystem or the ADC at the same time! */
#ifdef USE_BOOTLOADER_RANDOM
        bootloader_random_enable();
#endif
    }
    else {
        /* If WiFi or BlueTooth are enabled,
         * the function returns true random numbers */
    }

    /* This function automatically busy-waits to ensure enough external entropy
     * has been introduced into the hardware RNG state, before returning a new
     * random number.
     *
     * This delay is very short (always less than 100 CPU cycles).
     */

    /* in case the user want ADC, I2S, WiFi or BlueTooth, we need to disable
     * the bootloader random number generator.
     *
     * See https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/random.html
     * */
    if (isUsingWiFi == ESP_ERR_WIFI_NOT_INIT) {
        /* must be called to disable the entropy source again before using ADC, I2S, WiFi or BlueTooth.*/
#ifdef USE_BOOTLOADER_RANDOM
        bootloader_random_disable();
#endif
    }
    else {
    }

    return ret;
}
