
#include "test_task.h"
#include "esp_log.h"
#include <wolfssl/wolfcrypt/sha256.h>

#include "C:/workspace/wolfssh-gojimmypi-ESP32/examples/ESP32-SSH-Server/components/wolfssl/wolfcrypt/src/sha256.c"

static void esp_wait_until_idle() {
    while ((DPORT_REG_READ(SHA_1_BUSY_REG)  != 0) ||
          (DPORT_REG_READ(SHA_256_BUSY_REG) != 0) ||
          (DPORT_REG_READ(SHA_384_BUSY_REG) != 0) ||
          (DPORT_REG_READ(SHA_512_BUSY_REG) != 0)) {}
}


word32* myInputA[1000] = { 0,1,2,3 };
word32* myInputB[1000] = { 0,1,2,3 };
wc_Sha256* myOutput = { 0 };
// wc_Sha256* sha256;
word32* data = 0;
wc_Sha256 sha256[1];
char* TAG = "test_task";
void test_task(void *arg) {
    int i = 0;
    int len = 0;
    int ret = 0;
    word32 blocksLen;

    static TickType_t DelayTicks = 1000 / portTICK_PERIOD_MS;
    // sha256->buffer = &myInputA;
    data = &myInputA;

    ret = wc_InitSha256(sha256);
    byte* local = (byte*)sha256->buffer;
    blocksLen = min(len, WC_SHA256_BLOCK_SIZE - sha256->buffLen);
    XMEMCPY(&local[sha256->buffLen], data, blocksLen);

    while (1) {
        taskYIELD();
        vTaskDelay(DelayTicks ? DelayTicks : 1); /* Minimum delay = 1 tick */

//#define SHA_256_START_REG       ((DR_REG_SHA_BASE) + 0x90)
//#define SHA_256_CONTINUE_REG    ((DR_REG_SHA_BASE) + 0x94)
//#define SHA_256_LOAD_REG        ((DR_REG_SHA_BASE) + 0x98)
//#define SHA_256_BUSY_REG        ((DR_REG_SHA_BASE) + 0x9c)

            /* wait until idle */
        esp_wait_until_idle();

        /* LOAD final digest */
        /* wait until done */
        while (DPORT_REG_READ(SHA_256_BUSY_REG) != 0) {

#if defined(SINGLE_THREADED)
            esp_task_wdt_reset();
#else
            taskYIELD();
#endif
        }


        data = &myInputB;
//        sha256 = &myOutput;

        len = WC_SHA_BLOCK_SIZE; // sizeof(myInput);


        if (0)
        {
            if (ret == 0) {
                ret = XTRANSFORM(sha256, (const byte*)local);
            }

            if (ret == 0)
            {
                ESP_LOGI(TAG, "XTRANSFORM success");
            }
            else  {
                ESP_LOGI(TAG, "XTRANSFORM fail");
            }

            /* load message data into hw */
            for (i = 0; i < ((len) / (sizeof(word32))); ++i) {
                DPORT_REG_WRITE(SHA_TEXT_BASE + (i*sizeof(word32)), *(data + i));
            }
            DPORT_REG_WRITE(SHA_256_LOAD_REG, 1);


            int isfirstblock = 1;

            if (isfirstblock) {
                /* start first message block */
               DPORT_REG_WRITE(SHA_256_START_REG, 1);
                isfirstblock = 0;
            }
            else {
                /* CONTINU_REG */
                 DPORT_REG_WRITE(SHA_256_CONTINUE_REG, 1);
            }
            // sha256->ctx.isfirstblock = 0;

        }




    }

}