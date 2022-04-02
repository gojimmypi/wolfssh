#pragma once

/* server.h
 *
 * Copyright (C) 2014-2021 wolfSSL Inc.
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


#ifndef _WOLFSSH_EXAMPLES_SERVER_H_
#define _WOLFSSH_EXAMPLES_SERVER_H_

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#define NO_FILESYSTEM
#define WOLFSSH_TEST_SERVER
#define WOLFSSH_TEST_THREADING

#undef  WOLFSSL_USER_SETTINGS

#ifdef WOLFSSL_USER_SETTINGS
#include <wolfssl/wolfcrypt/settings.h>
#else
#include <wolfssl/options.h>
#endif

/* wolfSSL */
#include <wolfssl/wolfcrypt/settings.h> // make sure this appears before any other wolfSSL headers
#include <wolfssl/ssl.h>

#include <wolfssl/wolfcrypt/sha256.h>
#include <wolfssl/wolfcrypt/coding.h>
#include <wolfssl/wolfcrypt/logging.h>

#include <wolfssh/ssh.h>
#include <wolfssl/wolfcrypt/ecc.h>
/* socket includes */
#include "lwip/netdb.h"
#include "lwip/sockets.h"



#ifdef NO_FILESYSTEM
// #include <wolfssh/certs_test.h>
#ifdef WOLFSSH_SCP
#include <wolfssh/wolfscp.h>
#endif
#endif



void server_test();


#endif /* _WOLFSSH_EXAMPLES_SERVER_H_ */
