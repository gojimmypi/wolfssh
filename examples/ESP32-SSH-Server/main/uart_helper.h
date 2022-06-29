#pragma once

/* uart_hlper.h
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

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "driver/uart.h"
#include "string.h"
#include "driver/gpio.h"

/* see also tx_rx_buffer */

/* typically used for SSH, this sends the login welcome */
void uart_send_welcome(void);


/* FreeRTOS task for sending data */
void uart_tx_task(void *arg);

/* FreeRTOS task for receiving data */
void uart_rx_task(void *arg);

/* when we have specific data we want to send */
int sendData(const char* logName, const char* data);

/* initialization */
int init_UART(int tx_pin, int rx_pin, int baud_rate);