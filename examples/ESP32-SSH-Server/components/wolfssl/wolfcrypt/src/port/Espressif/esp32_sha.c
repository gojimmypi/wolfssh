/* esp32_sha.c
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

/* this entire file content is excluded when NO_SHA, NO_SHA256
 * or when using WC_SHA384 or WC_SHA512
 */
#if !defined(NO_SHA) || !defined(NO_SHA256) || defined(WC_SHA384) || \
     defined(WC_SHA512)

#include "wolfssl/wolfcrypt/logging.h"


/* this entire file content is excluded if not using HW hash accleration */
#if defined(WOLFSSL_ESP32WROOM32_CRYPT) && \
   !defined(NO_WOLFSSL_ESP32WROOM32_CRYPT_HASH)

#include <wolfssl/wolfcrypt/sha.h>
#include <wolfssl/wolfcrypt/sha256.h>
#include <wolfssl/wolfcrypt/sha512.h>

#include "wolfssl/wolfcrypt/port/Espressif/esp32-crypt.h"
#include "wolfssl/wolfcrypt/error-crypt.h"

#ifdef NO_INLINE
    #include <wolfssl/wolfcrypt/misc.h>
#else
    #define WOLFSSL_MISC_INCLUDED
    #include <wolfcrypt/src/misc.c>
#endif

static const char* TAG = "wolf_hw_sha";
/* continue register offset */
#define CONTINUE_REG_OFFSET    (0x04)     /* start_reg + 0x04 */

#ifdef NO_SHA
    #define WC_SHA_DIGEST_SIZE 20
#endif

/* mutex */
#if defined(SINGLE_THREADED)
    static int InUse = 0;
#else
    static wolfSSL_Mutex sha_mutex;
    static int espsha_CryptHwMutexInit = 0;
#endif
/*
    enum SHA_TYPE {
        SHA1 = 0,
        SHA2_256,
        SHA2_384,
        SHA2_512,
        SHA_INVALID = -1,
    };
*/
static word32 esp_sha_digest_size(enum SHA_TYPE type)
{
    ESP_LOGV(TAG, "enter esp_sha_digest_size");

    switch(type){
        #ifndef NO_SHA
            case SHA1:
                return WC_SHA_DIGEST_SIZE;
        #endif

        #ifndef NO_SHA256
            case SHA2_256:
                return WC_SHA256_DIGEST_SIZE;
        #endif

        #ifdef WOLFSSL_SHA384
            case SHA2_384:
                return WC_SHA384_DIGEST_SIZE;
        #endif

        #ifdef WOLFSSL_SHA512
            case SHA2_512:
                return WC_SHA512_DIGEST_SIZE;
        #endif

        default:
            ESP_LOGE(TAG, "Bad sha type");
            return WC_SHA_DIGEST_SIZE;
    }
    /* we never get here, as all the above switches should have a return */
}

/*
* wait until all engines becomes idle
*/
static void esp_wait_until_idle()
{
    while((DPORT_REG_READ(SHA_1_BUSY_REG)  !=0) ||
          (DPORT_REG_READ(SHA_256_BUSY_REG)!=0) ||
          (DPORT_REG_READ(SHA_384_BUSY_REG)!=0) ||
          (DPORT_REG_READ(SHA_512_BUSY_REG)!=0)) { }
}

/*
* lock hw engine.
* this should be called before using engine.
*/
int esp_sha_try_hw_lock(WC_ESP32SHA* ctx)
{
    int ret = 0;

    ESP_LOGV(TAG, "enter esp_sha_hw_lock");

    /* Init mutex
     *
     * Note that even single thread mode may calculate hashes
     * concurrently, so we still need to keep track of the
     * engine being busy or not.
     **/
#if defined(SINGLE_THREADED)
    if(ctx->mode == ESP32_SHA_INIT) {
        if(!InUse) {
            ctx->mode = ESP32_SHA_HW;
            InUse = 1;
        } else {
            ctx->mode = ESP32_SHA_SW;
        }
    } else {
         /* this should not happens */
        ESP_LOGE(TAG, "unexpected error in esp_sha_try_hw_lock.");
        return -1;
    }
#else
    /*
     * there's only one SHA engine for all the hash types
     * so when any hash is in use, no others can use it.
     * fall back to SW.
     **/
    if (espsha_CryptHwMutexInit == 0) {
        ret = esp_CryptHwMutexInit(&sha_mutex);
        if (ret == 0) {
            espsha_CryptHwMutexInit = 1;
        }
        else {
            ESP_LOGE(TAG, " mutex initialization failed.");
            ctx->mode = ESP32_SHA_SW;
            return 0;
        }
    }

    /* check if this sha has been operated as sw or hw, or not yet init */
    if (ctx->mode == ESP32_SHA_INIT) {
        /* try to lock the hw engine */
        ESP_LOGV(TAG, "ESP32_SHA_INIT");

        /* we don't wait: either the engine is free,
         * or we fall back to SW
         */
        if (esp_CryptHwMutexLock(&sha_mutex, (TickType_t)0) == 0) {
            ctx->mode = ESP32_SHA_HW;
            ESP_LOGV(TAG, "Hardware Mode");
        }
        else {
            ESP_LOGI(TAG, ">>>> Hardware Mode REVERT to software");
            ESP_LOGI(TAG, "someone used. hw is locked.....");
            ESP_LOGI(TAG, "the rest of operation will use sw implementation for this sha");
            ctx->mode = ESP32_SHA_SW;
            return 0;
        }
    }
    else {
        /* this should not happens */
        ESP_LOGE(TAG, "unexpected error in esp_sha_try_hw_lock.");
        return -1;
    }
#endif
   /* Enable SHA hardware */
    periph_module_enable(PERIPH_SHA_MODULE);

    ESP_LOGV(TAG, "leave esp_sha_hw_lock");
    return ret;
}
/*
* release hw engine
*/
void esp_sha_hw_unlock( void )
{
    ESP_LOGV(TAG, "enter esp_sha_hw_unlock");

    /* Disable AES hardware */
    periph_module_disable(PERIPH_SHA_MODULE);

    #if defined(SINGLE_THREADED)
        InUse = 0;
    #else
        /* unlock hw engine for next use */
        esp_CryptHwMutexUnLock(&sha_mutex);
    #endif

    ESP_LOGV(TAG, "leave esp_sha_hw_unlock");
}
/*
* start sha process by using hw engine
*/
static void esp_sha_start_process(WC_ESP32SHA* sha, uint32_t address)
{
    ESP_LOGV(TAG, "    enter esp_sha_start_process");

    if(sha->isfirstblock){
        /* start first message block */
        DPORT_REG_WRITE(address, 1);
        sha->isfirstblock = 0;
        ESP_LOGV(TAG, "      sha->isfirstblock = 0");
    }
    else {
        /* continue  */
        DPORT_REG_WRITE(address + CONTINUE_REG_OFFSET , 1);
        ESP_LOGV(TAG, "      continue");
   }

   ESP_LOGV(TAG, "    leave esp_sha_start_process");
}

int sha256_init()
{

    while (DPORT_REG_READ(SHA_256_BUSY_REG) == 1) {}

//    DPORT_REG_WRITE(SHA_TEXT_BASE + (0 * sizeof(word32)), 0x6a09e667);
//    DPORT_REG_WRITE(SHA_TEXT_BASE + (1 * sizeof(word32)), 0xbb67ae85);
//    DPORT_REG_WRITE(SHA_TEXT_BASE + (2 * sizeof(word32)), 0x3c6ef372);
//    DPORT_REG_WRITE(SHA_TEXT_BASE + (3 * sizeof(word32)), 0xa54ff53a);
//    DPORT_REG_WRITE(SHA_TEXT_BASE + (4 * sizeof(word32)), 0x510e527f);
//    DPORT_REG_WRITE(SHA_TEXT_BASE + (5 * sizeof(word32)), 0x9b05688c);
//    DPORT_REG_WRITE(SHA_TEXT_BASE + (6 * sizeof(word32)), 0x1f83d9ab);
//    DPORT_REG_WRITE(SHA_TEXT_BASE + (7 * sizeof(word32)), 0x5be0cd19);

    DPORT_REG_WRITE(SHA_TEXT_BASE + (0 * sizeof(word32)), __builtin_bswap32(0x6a09e667));
    DPORT_REG_WRITE(SHA_TEXT_BASE + (1 * sizeof(word32)), __builtin_bswap32(0xbb67ae85));
    DPORT_REG_WRITE(SHA_TEXT_BASE + (2 * sizeof(word32)), __builtin_bswap32(0x3c6ef372));
    DPORT_REG_WRITE(SHA_TEXT_BASE + (3 * sizeof(word32)), __builtin_bswap32(0xa54ff53a));
    DPORT_REG_WRITE(SHA_TEXT_BASE + (4 * sizeof(word32)), __builtin_bswap32(0x510e527f));
    DPORT_REG_WRITE(SHA_TEXT_BASE + (5 * sizeof(word32)), __builtin_bswap32(0x9b05688c));
    DPORT_REG_WRITE(SHA_TEXT_BASE + (6 * sizeof(word32)), __builtin_bswap32(0x1f83d9ab));
    DPORT_REG_WRITE(SHA_TEXT_BASE + (7 * sizeof(word32)), __builtin_bswap32(0x5be0cd19));

    DPORT_REG_WRITE(SHA_256_START_REG, 1);
    while (DPORT_REG_READ(SHA_256_BUSY_REG) == 1) {}
    return 0;
}

/* sample / test code */
int esp32_Transform_Sha256(wc_Sha256* target_sha256, const byte* data)
{
    /* check if there are any busy engine */
    int i;

    ESP_LOGV(TAG, ">> enter esp32_Transform_Sha256"); /* 3090 */

    int len = sizeof(target_sha256->buffer);
    if (len == 0) {
        return -1;
    }

    /* turn it on first */
    periph_module_enable(PERIPH_SHA_MODULE);

    while (DPORT_REG_READ(SHA_256_BUSY_REG) != 0) {}

    DPORT_REG_WRITE(SHA_256_LOAD_REG, 1);

    // sha256_init();

    while (DPORT_REG_READ(SHA_256_BUSY_REG) != 0) {}


    int word_len = ((len) / (sizeof(word32)));

    word_len = 1;
    int block_count = 0;
    // uint32_t[8] *w = (uint32_t[8] *)target_sha256->digest;

    uint32_t *sha_text_reg = (uint32_t *)(SHA_TEXT_BASE);
    word32 theValue = 0;
    /* load message data into hw, of 0..31 of 4-byte words  */
    for(i = 0 ; i < word_len ; i++) {
        // sha_text_reg[block_count + i] = __builtin_bswap32(w[i]);
        theValue = __builtin_bswap32(*(word32*)(data + i*sizeof(word32)));
        // put the data in  (*(volatile uint32_t *)(SHA_TEXT_BASE + (i*4)))
        DPORT_REG_WRITE(SHA_TEXT_BASE + (i*sizeof(word32)), theValue);
    }

    /* TODO manually add 0x80 and zero padding, currently manual*/

    /* instructions before volatile memory references to guarantee sequential consistency */
    /*  At least one MEMW should be executed in between every load or store to a volatile variable*/
    asm volatile("memw");

    /* start, the computation is hidden, the SHA_TEXT_BASE registers don't change
     * if  START_REG is not used, the result registers words [0..7] contain zero.
     **/
    DPORT_REG_WRITE(SHA_256_START_REG, 1);
    // DPORT_REG_WRITE(SHA_256_CONTINUE_REG, 1);
    while (DPORT_REG_READ(SHA_256_BUSY_REG) != 0) {}

    /* done */

    while (DPORT_REG_READ(SHA_256_BUSY_REG) != 0) {}

    /* this is where the hidden SHA calculation is actually presented in the SHA_TEXT_BASE registers */
    DPORT_REG_WRITE(SHA_256_LOAD_REG, 1);

    // asm volatile("memw");

    /* wait for completion */
    while (DPORT_REG_READ(SHA_256_BUSY_REG) != 0) {}

    /* instructions before volatile memory references to guarantee sequential consistency */
    /*  At least one MEMW should be executed in between every load or store to a volatile variable*/
    asm volatile("memw");

    /* read results into digest */
    int thisSize = esp_sha_digest_size(SHA2_256) / sizeof(word32);
    if (thisSize > 31)    {
        ESP_LOGI(TAG, "size warning!");
    }

    // asm volatile("memw");

    // the answer is in the first [n] bytes: ((uint32_t[32])  (*(volatile uint32_t *)(SHA_TEXT_BASE)))

    esp_dport_access_read_buffer((uint32_t *)target_sha256->digest,
        SHA_TEXT_BASE,
        esp_sha_digest_size(SHA2_256)/sizeof(word32));

    // Software divides the message into blocks according to "5.2 Parsing the Message"
    // 512 bits of the input block may be expressed as sixteen 32-bit words
    // in FIPS PUB 180 - 4 and writes one block to the SHA_TEXT_n_REG registers each time
    for (size_t i = 0; i < word_len; i++) {
        target_sha256->digest[i] = __builtin_bswap32(sha_text_reg[i]);
    }

    asm volatile("memw");

    ESP_LOGV(TAG, ">> leave esp32_Transform_Sha256"); /* 3090 */
    periph_module_disable(PERIPH_SHA_MODULE);
    return 0;
}

/*
* process message block
*/
static void esp_process_block(WC_ESP32SHA* ctx,  word32 address,
                                         const word32* data, word32 len)
{
    int i;

    ESP_LOGV(TAG, "  enter esp_process_block");

    /* check if there are any busy engine */
    esp_wait_until_idle();

    /* load [len] bytes of message data into hw */
    for (i = 0; i < ((len) / (sizeof(word32))); ++i) {
        /* by stuffing the values in array order, we avoid the need
         * to call __builtin_bswap32 to address endiness
         *
         * a useful watch array case to watch at runtime:
         *   ((uint32_t[32])  (*(volatile uint32_t *)(SHA_TEXT_BASE)))
         */
        DPORT_REG_WRITE(SHA_TEXT_BASE + (i*sizeof(word32)), *(data + i));
    }

    /* notify hw to start process */
    esp_sha_start_process(ctx, address);

    ESP_LOGV(TAG, "  leave esp_process_block");
}
/*
* retrieve sha digest from memory
*/
int esp_digest_state(WC_ESP32SHA* ctx, byte* hash, enum SHA_TYPE sha_type)
{
    uint32_t SHA_LOAD_REG = SHA_1_LOAD_REG;
    uint32_t SHA_BUSY_REG = SHA_1_BUSY_REG;

    ESP_LOGV(TAG, "enter esp_digest_state");

    /* sanity check */
    if(sha_type == SHA_INVALID) {
        ESP_LOGE(TAG, "unexpected error. sha_type is invalid.");
        return -1;
    }

    if (ctx == NULL) {
        return -1;
    }

    /* registers */
    switch (sha_type) {
        case SHA1:
            SHA_LOAD_REG = SHA_1_LOAD_REG;
            SHA_BUSY_REG = SHA_1_BUSY_REG;
            break;

        case SHA2_256:
            SHA_LOAD_REG = SHA_256_LOAD_REG;
            SHA_BUSY_REG = SHA_256_BUSY_REG;
            break;

    #if defined(WOLFSSL_SHA384)
        case SHA2_384:
            SHA_LOAD_REG = SHA_384_LOAD_REG;
            SHA_BUSY_REG = SHA_384_BUSY_REG;
            break;
    #endif

    #if defined(WOLFSSL_SHA512)
        case SHA2_512:
            SHA_LOAD_REG = SHA_512_LOAD_REG;
            SHA_BUSY_REG = SHA_512_BUSY_REG;
            break;
    #endif

    default:
        return -1;
        break;
    }


    if(ctx->isfirstblock == 1){
        /* no hardware use yet. Nothing to do yet */
        return 0;
    }

    /* wait until idle */
    esp_wait_until_idle();

    /* LOAD final digest */
    /* TODO what if we repeatedly ask to read? surely this would not reset  */
    DPORT_REG_WRITE(SHA_LOAD_REG, 1);

    /* wait until done */
    while (DPORT_REG_READ(SHA_BUSY_REG) != 0) {}


    esp_wait_until_idle();

    /* MEMW instructions before volatile memory references to guarantee
     * sequential consistency. At least one MEMW should be executed in
     * between every load or store to a volatile variable
     */
    asm volatile("memw");

    /* put result in hash variable */
    esp_dport_access_read_buffer((word32*)(hash), SHA_TEXT_BASE,
                                 esp_sha_digest_size(sha_type)/sizeof(word32));

#if defined(WOLFSSL_SHA512) || defined(WOLFSSL_SHA384)
    if (sha_type == SHA2_384 || sha_type == SHA2_512) {
        word32  i;
        word32* pwrd1 = (word32*)(hash);
        /* swap value */
        for (i = 0; i < WC_SHA512_DIGEST_SIZE / 4; i += 2) {
            pwrd1[i]     ^= pwrd1[i + 1];
            pwrd1[i + 1] ^= pwrd1[i];
            pwrd1[i]     ^= pwrd1[i + 1];
        }
    }
#endif

    ESP_LOGV(TAG, "leave esp_digest_state");
    return 0;
}

#ifndef NO_SHA
/*
* sha1 process
*/
int esp_sha_process(struct wc_Sha* sha, const byte* data)
{
    int ret = 0;

    ESP_LOGV(TAG, "enter esp_sha_process");

    word32 SHA_START_REG = SHA_1_START_REG;

    esp_process_block(&sha->ctx, SHA_START_REG, (const word32*)data,
                                        WC_SHA_BLOCK_SIZE);

    ESP_LOGV(TAG, "leave esp_sha_process");
    return ret;
}
/*
* retrieve sha1 digest
*/
int esp_sha_digest_process(struct wc_Sha* sha, byte blockproc)
{
    int ret = 0;

    ESP_LOGV(TAG, "enter esp_sha_digest_process");

    if(blockproc) {
        word32 SHA_START_REG = SHA_1_START_REG;

        esp_process_block(&sha->ctx, SHA_START_REG, sha->buffer,
                                            WC_SHA_BLOCK_SIZE);
    }

    esp_digest_state(&sha->ctx, (byte*)sha->digest, SHA1);  /* TODO  SHA1 ?? */

    ESP_LOGV(TAG, "leave esp_sha_digest_process");

    return ret;
}
#endif /* NO_SHA */


#ifndef NO_SHA256
/*
* sha256 process
*/
int esp_sha256_process(struct wc_Sha256* sha, const byte* data)
{
    int ret = 0;
    // word32 SHA_START_REG = SHA_1_START_REG;

    ESP_LOGV(TAG, "  enter esp_sha256_process");

    /* start register offset */
    // SHA_START_REG += (SHA2_256 << 4);

    esp_process_block(&sha->ctx, SHA_256_START_REG, (const word32*)data,
        WC_SHA256_BLOCK_SIZE);

    ESP_LOGV(TAG, "  leave esp_sha256_process");

    return ret;
}
/*
* retrieve sha256 digest
*/
int esp_sha256_digest_process(struct wc_Sha256* sha, byte blockproc)
{
    int ret = 0;

    ESP_LOGV(TAG, "enter esp_sha256_digest_process");

    if(blockproc) {

        esp_process_block(&sha->ctx, SHA_256_START_REG, sha->buffer,
                                           WC_SHA256_BLOCK_SIZE);
    }

    esp_digest_state(&sha->ctx, (byte*)sha->digest, SHA2_256);

    ESP_LOGV(TAG, "leave esp_sha256_digest_process");
    return ret;
}
#endif /* NO_SHA256 */

#if defined(WOLFSSL_SHA512) || defined(WOLFSSL_SHA384)
/*
* sha512 proess. this is used for sha384 too.
*/
void esp_sha512_block(struct wc_Sha512* sha, const word32* data, byte isfinal)
{
    enum SHA_TYPE sha_type = sha->ctx.sha_type;
    word32 SHA_START_REG = SHA_1_START_REG;

    ESP_LOGV(TAG, "enter esp_sha512_block");
    /* start register offset */
    SHA_START_REG += (sha_type << 4);

    if(sha->ctx.mode == ESP32_SHA_SW){
        ByteReverseWords64(sha->buffer, sha->buffer,
                               WC_SHA512_BLOCK_SIZE);
        if(isfinal){
            sha->buffer[WC_SHA512_BLOCK_SIZE / sizeof(word64) - 2] = sha->hiLen;
            sha->buffer[WC_SHA512_BLOCK_SIZE / sizeof(word64) - 1] = sha->loLen;
        }

    } else {
        ByteReverseWords((word32*)sha->buffer, (word32*)sha->buffer,
                                                        WC_SHA512_BLOCK_SIZE);
        if(isfinal){
            sha->buffer[WC_SHA512_BLOCK_SIZE / sizeof(word64) - 2] =
                                        rotlFixed64(sha->hiLen, 32U);
            sha->buffer[WC_SHA512_BLOCK_SIZE / sizeof(word64) - 1] =
                                        rotlFixed64(sha->loLen, 32U);
        }

        esp_process_block(&sha->ctx, SHA_START_REG, data, WC_SHA512_BLOCK_SIZE);
    }
    ESP_LOGV(TAG, "leave esp_sha512_block");
}
/*
* sha512 process. this is used for sha384 too.
*/
int esp_sha512_process(struct wc_Sha512* sha)
{
    word32 *data = (word32*)sha->buffer;

    ESP_LOGV(TAG, "enter esp_sha512_process");

    esp_sha512_block(sha, data, 0);

    ESP_LOGV(TAG, "leave esp_sha512_process");
    return 0;
}
/*
* retrieve sha512 digest. this is used for sha384 too.
*/
int esp_sha512_digest_process(struct wc_Sha512* sha, byte blockproc)
{
    ESP_LOGV(TAG, "enter esp_sha512_digest_process");

    if(blockproc) {
        word32* data = (word32*)sha->buffer;

        esp_sha512_block(sha, data, 1);
    }
    if(sha->ctx.mode != ESP32_SHA_SW)
        esp_digest_state(&sha->ctx, (byte*)sha->digest, sha->ctx.sha_type);

    ESP_LOGV(TAG, "leave esp_sha512_digest_process");
    return 0;
}
#endif /* WOLFSSL_SHA512 || WOLFSSL_SHA384 */
#endif /* WOLFSSL_ESP32WROOM32_CRYPT */
#endif /* !defined(NO_SHA) ||... */
