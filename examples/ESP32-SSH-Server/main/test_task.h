#pragma once

/* test_task.h
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
 */

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include <wolfssl/wolfcrypt/logging.h>


#include <freertos/FreeRTOS.h>
#ifdef TBD_TARGET_ESP32C3
    /* we don't yet know exactly how to detect ESP32 flavor */
    #include  <soc/esp32c3/dport_reg.h>
#else
    #if defined(CONFIG_IDF_TARGET_ESP32C3)
        /* no HW at this time */
    #else
        #include "soc/dport_reg.h"
        #include "soc/hwcrypto_reg.h"
    #endif
#endif


#if ESP_IDF_VERSION_MAJOR < 5
    #include "soc/cpu.h"
#endif

#if ESP_IDF_VERSION_MAJOR >= 5
    #include "esp_private/periph_ctrl.h"
#else
    #include "driver/periph_ctrl.h"
#endif

#if ESP_IDF_VERSION_MAJOR >= 4
    #include <esp32/rom/ets_sys.h>
#else
    #include <rom/ets_sys.h>
#endif

/******************************************/

#ifdef __cplusplus
    extern "C" {
#endif

void test_task(void *arg);


/* end c++ wrapper */
#ifdef __cplusplus
}
#endif
