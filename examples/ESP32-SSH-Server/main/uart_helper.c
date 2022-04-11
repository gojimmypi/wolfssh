#include "uart_helper.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "string.h"
#include "ssh_server.h"

#define DEBUG_WOLFSSL
#define DEBUG_WOLFSSH
#include <wolfssl/wolfcrypt/logging.h>


int sendData(const char* logName, const char* data) {
    const int len = strlen(data);
    const int txBytes = uart_write_bytes(UART_NUM_1, data, len);
    ESP_LOGI(logName, "Wrote %d bytes", txBytes);
    return txBytes;
}



void uart_tx_task(void *arg) {
    /* 
     * when we receive chars from ssh, we'll send them out the UART
    */
    static const char *TX_TASK_TAG = "TX_TASK";
    esp_log_level_set(TX_TASK_TAG, ESP_LOG_INFO);
    while (1) {
        if (ExternalReceiveBufferSz() > 0)
        {
            WOLFSSL_MSG("UART Send Data");
            sendData(TX_TASK_TAG, ExternalReceiveBuffer());
            Set_ExternalReceiveBufferSz(0);
        }
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}

void uart_rx_task(void *arg) {
    /* 
     * when we receive chars from UART, we'll send them out SSH
    */
    static const char *RX_TASK_TAG = "RX_TASK";
    esp_log_level_set(RX_TASK_TAG, ESP_LOG_INFO);
    uint8_t* data = (uint8_t*) malloc(RX_BUF_SIZE + 1);

    volatile char __attribute__((optimize("O0"))) *thisBuf;
    while (1) {
        const int rxBytes = uart_read_bytes(UART_NUM_1, data, RX_BUF_SIZE, 1000 / portTICK_RATE_MS);
        if (rxBytes > 0) {
            WOLFSSL_MSG("UART Rx Data!");
            data[rxBytes] = 0;
            ESP_LOGI(RX_TASK_TAG, "Read %d bytes: '%s'", rxBytes, data);
            ESP_LOG_BUFFER_HEXDUMP(RX_TASK_TAG, data, rxBytes, ESP_LOG_INFO);
            thisBuf = ExternalTransmitBuffer();
            memcpy(thisBuf, data, rxBytes);

            Set_ExternalTransmitBufferSz(rxBytes);
        }
    }
    free(data);
}