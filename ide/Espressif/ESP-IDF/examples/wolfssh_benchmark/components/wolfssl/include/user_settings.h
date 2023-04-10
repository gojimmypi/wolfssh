/* user_settings.h
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

#include <sdkconfig.h> /* essential to chip set detection */
#undef WOLFSSL_ESPIDF
#undef WOLFSSL_ESPWROOM32
#undef WOLFSSL_ESPWROOM32SE
#undef WOLFSSL_ESPWROOM32
#undef WOLFSSL_ESP8266

/* The Espressif sdkconfig will have chipset info.
**
** Possible values:
**
**   CONFIG_IDF_TARGET_ESP32
**   CONFIG_IDF_TARGET_ESP32S3
**   CONFIG_IDF_TARGET_ESP32C3
**   CONFIG_IDF_TARGET_ESP32C6
*/
#include "sdkconfig.h"

#define WOLFSSL_ESPIDF

/*
 * choose ONE of these Espressif chips to define:
 *
 * WOLFSSL_ESPWROOM32
 * WOLFSSL_ESPWROOM32SE
 * WOLFSSL_ESP8266
 */

#define WOLFSSL_ESPWROOM32
/* #define WOLFSSL_NOSHA512_224 */
/* #define WOLFSSL_NOSHA512_256 */

/* #define DEBUG_WOLFSSL_VERBOSE */

#define BENCH_EMBEDDED
#define USE_CERT_BUFFERS_2048

/* TLS 1.3                                 */
#define WOLFSSL_TLS13
#define HAVE_TLS_EXTENSIONS
#define WC_RSA_PSS
#define HAVE_HKDF
#define HAVE_AEAD
#define HAVE_SUPPORTED_CURVES

#define WOLFSSL_BENCHMARK_FIXED_UNITS_KB

/* when you want to use SINGLE THREAD */
#define SINGLE_THREADED

#define NO_FILESYSTEM

#define HAVE_AESGCM

#define WOLFSSL_RIPEMD
/* when you want to use SHA224 */
#define WOLFSSL_SHA224

/* when you want to use SHA384 */
#define WOLFSSL_SHA3

#define WOLFSSL_SHA384
#define WOLFSSL_SHA512
#define HAVE_ECC
#define HAVE_CURVE25519
#define CURVE25519_SMALL
#define HAVE_ED25519

#define OPENSSL_EXTRA
/* when you want to use pkcs7 */

#define HAVE_PKCS7

#if defined(HAVE_PKCS7)
    #define HAVE_AES_KEYWRAP
    #define HAVE_X963_KDF
    #define WOLFSSL_AES_DIRECT
#endif

/* when you want to use aes counter mode */
/* #define WOLFSSL_AES_DIRECT */
/* #define WOLFSSL_AES_COUNTER */

/* esp32-wroom-32se specific definition */
#if defined(WOLFSSL_ESPWROOM32SE)
    #define WOLFSSL_ATECC508A
    #define HAVE_PK_CALLBACKS
    /* when you want to use a custom slot allocation for ATECC608A */
    /* unless your configuration is unusual, you can use default   */
    /* implementation.                                             */
    /* #define CUSTOM_SLOT_ALLOCATION                              */
#endif

/* rsa primitive specific definition */
#if defined(WOLFSSL_ESPWROOM32) || defined(WOLFSSL_ESPWROOM32SE)
    /* Define USE_FAST_MATH and SMALL_STACK                        */
    #define ESP32_USE_RSA_PRIMITIVE
    /* threshold for performance adjustment for HW primitive use   */
    /* X bits of G^X mod P greater than                            */
    #define EPS_RSA_EXPT_XBTIS           36
    /* X and Y of X * Y mod P greater than                         */
    #define ESP_RSA_MULM_BITS            2000
#endif

/* debug options */
// #define DEBUG_WOLFSSL
#define WOLFSSL_ESP32WROOM32_CRYPT_DEBUG
/* #define WOLFSSL_ATECC508A_DEBUG          */

/* date/time                               */
/* if it cannot adjust time in the device, */
/* enable macro below                      */
/* #define NO_ASN_TIME */
/* #define XTIME time */

#if defined(CONFIG_IDF_TARGET_ESP32C3)
    /* TODO not yet implemented to C3 RISC-V */
    #define NO_ESP32WROOM32_CRYPT
    #define NO_WOLFSSL_ESP32WROOM32_CRYPT_HASH
    #define NO_WOLFSSL_ESP32WROOM32_CRYPT_AES
    #define NO_WOLFSSL_ESP32WROOM32_CRYPT_RSA_PRI
#else
    /* when you want not to use HW acceleration
    ** otherise when commented out, enable accerlation: */

    /* #define NO_ESP32WROOM32_CRYPT                    */
    /* #define NO_WOLFSSL_ESP32WROOM32_CRYPT_HASH       */
    /* #define NO_WOLFSSL_ESP32WROOM32_CRYPT_AES        */
    /* #define NO_WOLFSSL_ESP32WROOM32_CRYPT_RSA_PRI    */
#endif
/* adjust wait-timeout count if you see timeout in RSA HW acceleration */
#define ESP_RSA_TIMEOUT_CNT    0x249F00

#define HASH_SIZE_LIMIT /* for test.c */

#define USE_FAST_MATH
#define WOLFSSL_SMALL_STACK

#if defined(CONFIG_IDF_TARGET_ESP32S3) && !defined(NO_WOLFSSL_ESP32WROOM32_CRYPT_AES)
    /* AES192 is not supported on the ESP32-S3 HW at this time */
    #define NO_AES_192
#endif

#define HAVE_VERSION_EXTENDED_INFO
#define HAVE_WC_INTROSPECTION

/* allows for all version info, even that supporessed with intospection */
#define ALLOW_BINARY_MISMATCH_INTROSPECTION

#define  HAVE_SESSION_TICKET
#define WOLFSSL_RSA_KEY_SIZE 2048

// #define WOLFSSL_HAVE_SP_RSA
// #define WOLFSSL_SP_MATH_ALL


#if defined(CONFIG_IDF_TARGET_ESP32C3)
    /* #define WOLFSSL_SP_RISCV32 */
#else
    #if defined(WOLFSSL_SP_RISCV32)
        #error "WOLFSSL_SP_RISCV32 not supported for this target"
    #endif
#endif
