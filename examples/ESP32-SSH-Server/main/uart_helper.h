#pragma once
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "driver/uart.h"
#include "string.h"
#include "driver/gpio.h"

static const int RX_BUF_SIZE = 1024; /* TODO remove duplicate definition */

void uart_tx_task(void *arg);
void uart_rx_task(void *arg);

int sendData(const char* logName, const char* data);
