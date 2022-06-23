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

/* TODO this may be chip type dependant: */
#include <hal/clk_gate_ll.h>

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

#ifdef NO_SHA
    #define WC_SHA_DIGEST_SIZE 20
#endif

/* mutex */
#if defined(SINGLE_THREADED)
    static int InUse = 0;
#else
    static wolfSSL_Mutex sha_mutex;
    static int espsha_CryptHwMutexInit = 0;

    #if defined(DEBUG_WOLFSSL)
        static int this_block_num = 0;
    #endif
#endif

/*
 * determine the digest size, depending on SHA type.
 *
 * See FIPS PUB 180-4, Intruction Section 1.
 *
 *
    enum SHA_TYPE {
        SHA1 = 0,
        SHA2_256,
        SHA2_384,
        SHA2_512,
        SHA_INVALID = -1,
    };
*/
static word32 wc_esp_sha_digest_size(enum SHA_TYPE type)
{
    ESP_LOGV(TAG, "  esp_sha_digest_size");

    switch(type){
        #ifndef NO_SHA
            case SHA1: /* typically 20 bytes */
                return WC_SHA_DIGEST_SIZE;
        #endif

        #ifndef NO_SHA256
            case SHA2_256: /* typically 32 bytes */
                return WC_SHA256_DIGEST_SIZE;
        #endif

        #ifdef WOLFSSL_SHA384
            case SHA2_384:
                return WC_SHA384_DIGEST_SIZE;
        #endif

        #ifdef WOLFSSL_SHA512
            case SHA2_512: /* typically 64 bytes */
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
static void wc_esp_wait_until_idle()
{
    while((DPORT_REG_READ(SHA_1_BUSY_REG)   != 0) ||
          (DPORT_REG_READ(SHA_256_BUSY_REG) != 0) ||
          (DPORT_REG_READ(SHA_384_BUSY_REG) != 0) ||
          (DPORT_REG_READ(SHA_512_BUSY_REG) != 0)) {
        /* do nothing while waiting. TODO add timeout? */
    }
}

/*
 * hack alert. there really should have been something implemented
 * in periph_ctrl.c to detect ref_counts[periph] depth.
 *
 * TODO: check if this works with other ESP32 platforms ESP32-C3, ESP32-S3, etc
 *
 * since there is not at this time, we have this brute-force method:
 *
 */
int esp_unroll_sha_module_enable(WC_ESP32SHA* ctx)
{
    /* there was a prior unexpected fail and we need to unroll enables */
    int ret = 0; /* assume succcess unless proven otherwose */
    int i;
    uint32_t this_sha_mask;
    int actual_unroll_count = 0;

    /* TODO update to while() */
    for (i = 0; i < ctx->lockDepth; i++) {
        /* unwind prior calls to THIS ctx. decrement ref_counts[periph] */
        periph_module_disable(PERIPH_SHA_MODULE);
        actual_unroll_count++;
        /* only when ref_counts[periph] == 0 does something actuall happen */
        this_sha_mask = periph_ll_get_clk_en_mask(PERIPH_SHA_MODULE);

        /* once the value we read is a 0 in the DPORT_PERI_CLK_EN_REG bit
         * then we have fully unrolled the enables via ref_counts[periph]==0 */
        if ((this_sha_mask & *(uint32_t*)DPORT_PERI_CLK_EN_REG) == 0) {
            ESP_LOGI(TAG, "unroll complete!\n");
            break;
        }
        else {
            ESP_LOGI(TAG, "unroll not yet successful. try #%d", actual_unroll_count);
        }
    }
    if (ctx->lockDepth != actual_unroll_count) {
        /* this could be a warning of wokiness in RTOS envirvonment */
        ESP_LOGE(TAG, "lockDepth mismatch\n");
    }
    ctx->lockDepth = 0;
    ctx->mode = ESP32_SHA_INIT;
    return ret;
}

/*
* lock hw engine.
* this should be called before using engine.
*/
int esp_sha_try_hw_lock(WC_ESP32SHA* ctx)
{
    int ret = 0;

    ESP_LOGV(TAG, "enter esp_sha_hw_lock");

    if (ctx == NULL) {
        ESP_LOGE(TAG, " esp_sha_try_hw_lock called with NULL ctx");
        return -1;
    }

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
        }
        else {
            ctx->mode = ESP32_SHA_SW;
        }
    }
    else {
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

    /*
     * here is some sample code to test the unrolling of sha enables:
     *
    periph_module_enable(PERIPH_SHA_MODULE);
    ctx->lockDepth++;
    periph_module_enable(PERIPH_SHA_MODULE);
    ctx->lockDepth++;
    ctx->mode = ESP32_FAIL_NEED_INIT;

    */

    /* check to see if we had a prior fail and need to unroll enables */
    if (ctx->mode == ESP32_SHA_FAIL_NEED_UNROLL) {
        ret = esp_unroll_sha_module_enable(ctx);
    }

    if (espsha_CryptHwMutexInit == 0) {
        ESP_LOGV(TAG, "set esp_CryptHwMutexInit");
        ret = esp_CryptHwMutexInit(&sha_mutex);
        if (ret == 0) {
            espsha_CryptHwMutexInit = 1;
        }
        else {
            ESP_LOGE(TAG, " mutex initialization failed. revert to sofware");
            ctx->mode = ESP32_SHA_SW;
            return 0; /* success, just not using HW */
        }
    }

    /* check if this sha has been operated as sw or hw, or not yet init */
    if (ctx->mode == ESP32_SHA_INIT) {
        /* try to lock the hw engine */
        ESP_LOGV(TAG, "ESP32_SHA_INIT\n");

        /* we don't wait: either the engine is free,
         * or we fall back to SW
         */
        if (esp_CryptHwMutexLock(&sha_mutex, (TickType_t)0) == 0) {
            ctx->mode = ESP32_SHA_HW;
            ESP_LOGV(TAG, "Hardware Mode, lock depth = %d", ctx->lockDepth);
        }
        else {
            ESP_LOGI(TAG, ">>>> Hardware Mode REVERT to ESP32_SHA_SW");
            ctx->mode = ESP32_SHA_SW;
            return 0; /* success, but revert to SW */
        }
    }
    else {
        /* this should not happen: called during mode != ESP32_SHA_INIT  */
        ESP_LOGE(TAG, "unexpected error in esp_sha_try_hw_lock.");
        return -1;
    }
#endif
   /* Enable SHA hardware only if no premature exit */
    ctx->lockDepth++; /* depth for THIS ctx (there could be others!) */
    periph_module_enable(PERIPH_SHA_MODULE);

    ESP_LOGV(TAG, "leave esp_sha_hw_lock");
    return ret;
}
/*
* release hw engine. when we don't have it locked, SHA module is DISABLED
*/
int esp_sha_hw_unlock(WC_ESP32SHA* ctx)
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

    /* we'll keep track of our lock depth.
     * in case of unexpected results, all the periph_module_disable()
     * and periph_module_disable() need to be unwound. see ref_counts[periph]
     * in file: periph_ctrl.c */
    if (ctx->lockDepth > 0) {
        ctx->lockDepth--;
    }
    else {
        ctx->lockDepth = 0;
    }


    ESP_LOGV(TAG, "leave esp_sha_hw_unlock");
    return 0;
}

/*
* start sha process by using hw engine.
* assumes register already loaded.
*/
static int esp_sha_start_process(WC_ESP32SHA* sha)
{
    int ret = 0;
    if (sha == NULL) {
        return -1;
    }

    ESP_LOGV(TAG, "    enter esp_sha_start_process");

    if(sha->isfirstblock){
        /* start registers for first message block
         * we don't make any relational memory position assumptions.
         */
        switch (sha->sha_type) {
            case SHA1:
                DPORT_REG_WRITE(SHA_1_START_REG, 1);
                break;

            case SHA2_256:
                DPORT_REG_WRITE(SHA_256_START_REG, 1);
            break;

        #if defined(WOLFSSL_SHA384)
            case SHA2_384:
                DPORT_REG_WRITE(SHA_384_START_REG, 1);
                break;
        #endif

        #if defined(WOLFSSL_SHA512)
            case SHA2_512:
                DPORT_REG_WRITE(SHA_512_START_REG, 1);
            break;
        #endif

            default:
                sha->mode = ESP32_SHA_FAIL_NEED_UNROLL;
                ret = -1;
                break;
       }

        sha->isfirstblock = 0;
        ESP_LOGV(TAG, "      set sha->isfirstblock = 0");

        #if defined(DEBUG_WOLFSSL)
            this_block_num = 1; /* one-based counter, just for debug info */
        #endif

    }
    else {
        /* continue  */
        /* continue registers for next message block.
         * we don't make any relational memory position assumptions
         * for future chip architecture changes.
         */
        switch (sha->sha_type) {
            case SHA1:
                DPORT_REG_WRITE(SHA_1_CONTINUE_REG, 1);
                break;

            case SHA2_256:
                DPORT_REG_WRITE(SHA_256_CONTINUE_REG, 1);
            break;

        #if defined(WOLFSSL_SHA384)
            case SHA2_384:
                DPORT_REG_WRITE(SHA_384_CONTINUE_REG, 1);
                break;
        #endif

        #if defined(WOLFSSL_SHA512)
            case SHA2_512:
                DPORT_REG_WRITE(SHA_512_CONTINUE_REG, 1);
            break;
        #endif

            default:
                sha->mode = ESP32_FAIL_NEED_INIT;
                ret = -1;
                break;
       }
        #if defined(DEBUG_WOLFSSL)
            this_block_num++; /* one-based counter */
        #endif

        ESP_LOGV(TAG, "      continue block #%d", this_block_num);
   }

   ESP_LOGV(TAG, "    leave esp_sha_start_process");

    return ret;
}

/* TODO sample / test code */
int esp32_Transform_Sha256_demo(wc_Sha256* target_sha256, const byte* data)
{
    /* check if there are any busy engine */
    int i;

    ESP_LOGV(TAG, ">> enter esp32_Transform_Sha256_demo"); /* 3090 */

    int len = sizeof(target_sha256->buffer);
    if (len == 0) {
        return -1;
    }

    /* turn it on first */
    periph_module_enable(PERIPH_SHA_MODULE);

    while (DPORT_REG_READ(SHA_256_BUSY_REG) != 0) {}

    DPORT_REG_WRITE(SHA_256_LOAD_REG, 1);

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
    int thisSize = wc_esp_sha_digest_size(SHA2_256) / sizeof(word32);
    if (thisSize > 31)    {
        ESP_LOGI(TAG, "size warning!");
    }

    // asm volatile("memw");

    // the answer is in the first [n] bytes: ((uint32_t[32])  (*(volatile uint32_t *)(SHA_TEXT_BASE)))

    esp_dport_access_read_buffer(
        (uint32_t *)target_sha256->digest,
        SHA_TEXT_BASE,
        wc_esp_sha_digest_size(SHA2_256) / sizeof(word32)
    );

    // Software divides the message into blocks according to "5.2 Parsing the Message"
    // 512 bits of the input block may be expressed as sixteen 32-bit words
    // in FIPS PUB 180 - 4 and writes one block to the SHA_TEXT_n_REG registers each time
    for (size_t i = 0; i < word_len; i++) {
        target_sha256->digest[i] = __builtin_bswap32(sha_text_reg[i]);
    }

    asm volatile("memw");

    ESP_LOGV(TAG, ">> leave esp32_Transform_Sha256_demo"); /* 3090 */
    periph_module_disable(PERIPH_SHA_MODULE);
    return 0;
}

/*
* process message block
*/
static void wc_esp_process_block(WC_ESP32SHA* ctx, /* see ctx->sha_type */
                              const word32* data,
                              word32 len)
{
    int i;
    int word32_to_save = (len) / (sizeof(word32));
    ESP_LOGV(TAG, "  enter esp_process_block");
    if (word32_to_save > 0x31) {
        word32_to_save = 0x31;
        ESP_LOGE(TAG, "  ERROR esp_process_block len exceeds 0x31 words");
    }

    /* check if there are any busy engine */
    wc_esp_wait_until_idle();

    /* load [len] words of message data into hw */
    for (i = 0; i < word32_to_save; i++) {
        /* by using DPORT_REG_WRITE, we avoid the need
         * to call __builtin_bswap32 to address endiness
         *
         * a useful watch array cast to watch at runtime:
         *   ((uint32_t[32])  (*(volatile uint32_t *)(SHA_TEXT_BASE)))
         *
         * Write value to DPORT register (does not require protecting)
         */
        DPORT_REG_WRITE(SHA_TEXT_BASE + (i*sizeof(word32)), *(data + i));
        /* memw confirmed auto inserted by compiler here */
    }

    /* notify hw to start process
     * see ctx->sha_type
     * reg data does not change until we are ready to read */
    esp_sha_start_process(ctx);

    ESP_LOGV(TAG, "  leave esp_process_block");
}

/*
 * retrieve sha digest from memory
 */
int wc_esp_digest_state(WC_ESP32SHA* ctx, byte* hash)
{
    ESP_LOGV(TAG, "enter esp_digest_state");

    if (ctx == NULL)    {
        return -1;
    }

    /* sanity check */
    if (ctx->sha_type == SHA_INVALID) {
        ESP_LOGE(TAG, "unexpected error. sha_type is invalid.");
        return -1;
    }

    if (ctx == NULL) {
        return -1;
    }

    /* wait until idle */
    wc_esp_wait_until_idle();

    /* each sha_type register is at a different location  */
    switch (ctx->sha_type) {
        case SHA1:
            DPORT_REG_WRITE(SHA_1_LOAD_REG, 1);
            break;

        case SHA2_256:
            DPORT_REG_WRITE(SHA_256_LOAD_REG, 1);
            break;

    #if defined(WOLFSSL_SHA384)
        case SHA2_384:
            SHA_LOAD_REG = SHA_384_LOAD_REG;
            SHA_BUSY_REG = SHA_384_BUSY_REG;
            break;
    #endif

    #if defined(WOLFSSL_SHA512)
        case SHA2_512:
            DPORT_REG_WRITE(SHA_512_LOAD_REG, 1);
            break;
    #endif

        default:
            return -1;
            break;
    }


    if(ctx->isfirstblock == 1){
        /* no hardware use yet. Nothing to do yet */
        /* TODO but what if it is a tiny block? */
        return 0;
    }


    /* LOAD final digest */
    /* TODO what if we repeatedly ask to read? surely this would not reset  */

    wc_esp_wait_until_idle();

    /* MEMW instructions before volatile memory references to guarantee
     * sequential consistency. At least one MEMW should be executed in
     * between every load or store to a volatile variable
     */
    asm volatile("memw");

    /* put result in hash variable.
     *
     * ALERT - hardware specific. See esp_hw_support\port\esp32\dport_access.c
     *
     * note we read 4-byte word32's here via DPORT_SEQUENCE_REG_READ
     *
     *  example:
     *    DPORT_SEQUENCE_REG_READ(address + i * 4);
     */
    esp_dport_access_read_buffer(
        (word32*)(hash), /* the result will be found in hash upon exit     */
        SHA_TEXT_BASE,   /* there's a fixed reg addy for all SHA           */
        wc_esp_sha_digest_size(ctx->sha_type) / sizeof(word32) /* # 4-byte */
    );

#if defined(WOLFSSL_SHA512) || defined(WOLFSSL_SHA384)
    if (ctx->sha_type == SHA2_384 || ctx->sha_type == SHA2_512) {
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

    wc_esp_process_block(&sha->ctx, (const word32*)data, WC_SHA_BLOCK_SIZE);

    ESP_LOGV(TAG, "leave esp_sha_process");
    return ret;
}
/*
* retrieve sha1 digest
*/
int esp_sha_digest_process(struct wc_Sha* sha, byte blockprocess)
{
    int ret = 0;

    ESP_LOGV(TAG, "enter esp_sha_digest_process");

    if (blockprocess) {
        wc_esp_process_block(&sha->ctx, sha->buffer, WC_SHA_BLOCK_SIZE);
    }

    wc_esp_digest_state(&sha->ctx, (byte*)sha->digest);

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

    ESP_LOGV(TAG, "  enter esp_sha256_process");

    if ((&sha->ctx)->sha_type == SHA2_256) {
#if defined(DEBUG_WOLFSSL_VERBOSE)
        ESP_LOGV(TAG, "    confirmed sha type call match");
#endif
    }
    else {
        ret = -1;
        ESP_LOGE(TAG, "    ERROR sha type call mismatch");
    }

    wc_esp_process_block(&sha->ctx, (const word32*)data, WC_SHA256_BLOCK_SIZE);

    ESP_LOGV(TAG, "  leave esp_sha256_process");

    return ret;
}
/*
* retrieve sha256 digest
*
* note that wc_Sha256Final() in sha256.c expects to need to reverse byte
* order, even though we could have returned them in the right order.
*/
int esp_sha256_digest_process(struct wc_Sha256* sha, byte blockprocess)
{
    int ret = 0;

    ESP_LOGV(TAG, "enter esp_sha256_digest_process");

    if(blockprocess) {

        wc_esp_process_block(&sha->ctx, sha->buffer, WC_SHA256_BLOCK_SIZE);
    }

    wc_esp_digest_state(&sha->ctx, (byte*)sha->digest);

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
    ESP_LOGV(TAG, "enter esp_sha512_block");
    /* start register offset */

    if(sha->ctx.mode == ESP32_SHA_SW){
        ByteReverseWords64(sha->buffer, sha->buffer,
                               WC_SHA512_BLOCK_SIZE);
        if(isfinal) {
            sha->buffer[WC_SHA512_BLOCK_SIZE / sizeof(word64) - 2] = sha->hiLen;
            sha->buffer[WC_SHA512_BLOCK_SIZE / sizeof(word64) - 1] = sha->loLen;
        }

    }
    else {
        ByteReverseWords((word32*)sha->buffer, (word32*)sha->buffer,
                                                        WC_SHA512_BLOCK_SIZE);
        if(isfinal){
            sha->buffer[WC_SHA512_BLOCK_SIZE / sizeof(word64) - 2] =
                                        rotlFixed64(sha->hiLen, 32U);
            sha->buffer[WC_SHA512_BLOCK_SIZE / sizeof(word64) - 1] =
                                        rotlFixed64(sha->loLen, 32U);
        }

        wc_esp_process_block(&sha->ctx, data, WC_SHA512_BLOCK_SIZE);
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
        wc_esp_digest_state(&sha->ctx, (byte*)sha->digest);

    ESP_LOGV(TAG, "leave esp_sha512_digest_process");
    return 0;
}
#endif /* WOLFSSL_SHA512 || WOLFSSL_SHA384 */
#endif /* WOLFSSL_ESP32WROOM32_CRYPT */
#endif /* !defined(NO_SHA) ||... */
