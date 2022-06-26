#
#  Copyright (C) 2014-2022 wolfSSL Inc.
#
# This file is part of wolfSSH.
#
# wolfSSH is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3 of the License, or
# (at your option) any later version.
#
# wolfSSH is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with wolfSSH.  If not, see <http://www.gnu.org/licenses/>.
#
set(srcs main.c ssh_server.c uart_helper.c uart_helper.c wifi.c ssh_server_config.c enc28j60_helper.c int_to_string.c tx_rx_buffer.c test_task.c ethernet_helper.c)

idf_component_register(
                       SRCS "${srcs}"
                       INCLUDE_DIRS "."
                       PRIV_INCLUDE_DIRS
                       # ../components/wolfssl/wolfcrypt/src
                       )
