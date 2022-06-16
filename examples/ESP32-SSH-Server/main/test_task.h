
#pragma once
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include <wolfssl/wolfcrypt/logging.h>


#include <freertos/FreeRTOS.h>
#include "soc/dport_reg.h"
// #include  <soc/esp32c3/dport_reg.h>
#include "soc/hwcrypto_reg.h"

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
