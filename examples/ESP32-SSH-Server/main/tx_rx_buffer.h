#pragma once
/* my_config.h
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

#include <string.h>
/* TODO do these really need to be so big? probably not */
#define ExternalReceiveBufferMaxLength 2047
#define ExternalTransmitBufferMaxLength 2047

typedef uint8_t byte;

int  init_tx_rx_buffer(byte TxPin, byte RxPin);
int Get_ExternalTransmitBuffer(byte **ToData);

int Set_ExternalTransmitBuffer(byte *FromData, int sz);

int Set_ExternalReceiveBuffer(byte *FromData, int sz);

bool ExternalReceiveBuffer_IsChar(char charValue);

