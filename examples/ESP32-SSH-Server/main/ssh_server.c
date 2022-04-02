/* server.c
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

#include "ssh_server.h"

#ifndef NO_WOLFSSH_SERVER

static const char serverBanner[] = "wolfSSH Example Server\n";


typedef struct {
    WOLFSSH* ssh;
    int fd;
    word32 id;
    char nonBlock;
} thread_ctx_t;


#ifndef EXAMPLE_HIGHWATER_MARK
#define EXAMPLE_HIGHWATER_MARK 0x3FFF8000 /* 1GB - 32kB */
#endif
#ifndef EXAMPLE_BUFFER_SZ
#define EXAMPLE_BUFFER_SZ 4096
#endif
#define SCRATCH_BUFFER_SZ 1200


static byte find_char(const byte* str, const byte* buf, word32 bufSz) {
    const byte* cur;

    while (bufSz) {
        cur = str;
        while (*cur != '\0') {
            if (*cur == *buf)
                return *cur;
            cur++;
        }
        buf++;
        bufSz--;
    }

    return 0;
}


static int dump_stats(thread_ctx_t* ctx) {
    char stats[1024];
    word32 statsSz;
    word32 txCount, rxCount, seq, peerSeq;

    wolfSSH_GetStats(ctx->ssh, &txCount, &rxCount, &seq, &peerSeq);

    WSNPRINTF(stats,
        sizeof(stats),
        "Statistics for Thread #%u:\r\n"
        "  txCount = %u\r\n  rxCount = %u\r\n"
        "  seq = %u\r\n  peerSeq = %u\r\n",
        ctx->id,
        txCount,
        rxCount,
        seq,
        peerSeq);
    statsSz = (word32)strlen(stats);

    fprintf(stderr, "%s", stats);
    return wolfSSH_stream_send(ctx->ssh, (byte*)stats, statsSz);
}


static int NonBlockSSH_accept(WOLFSSH* ssh) {
    int ret;
    int error;
    int sockfd;
    int select_ret = 0;

    ret = wolfSSH_accept(ssh);
    error = wolfSSH_get_error(ssh);
    sockfd = (int)wolfSSH_get_fd(ssh);

    while (ret != WS_SUCCESS &&
            (error == WS_WANT_READ || error == WS_WANT_WRITE)) {
        if (error == WS_WANT_READ)
            printf("... client would read block\n");
        else if (error == WS_WANT_WRITE)
            printf("... client would write block\n");

//        select_ret = tcp_select(sockfd, 1);
//        if (select_ret == WS_SELECT_RECV_READY  ||
//            select_ret == WS_SELECT_ERROR_READY ||
//            error == WS_WANT_WRITE) {
//            ret = wolfSSH_accept(ssh);
//            error = wolfSSH_get_error(ssh);
//        }
//        else if (select_ret == WS_SELECT_TIMEOUT)
//            error = WS_WANT_READ;
//        else
//            error = WS_FATAL_ERROR;
    }

    return ret;
}


static void server_worker(void* vArgs) {
    int ret;
    thread_ctx_t* threadCtx = (thread_ctx_t*)vArgs;

#if defined(WOLFSSH_SCP) && defined(NO_FILESYSTEM)
    ScpBuffer scpBufferRecv, scpBufferSend;
    byte fileBuffer[49000];
    byte fileTmp[] = "wolfSSH SCP buffer file";

    WMEMSET(&scpBufferRecv, 0, sizeof(ScpBuffer));
    scpBufferRecv.buffer   = fileBuffer;
    scpBufferRecv.bufferSz = sizeof(fileBuffer);
    wolfSSH_SetScpRecvCtx(threadCtx->ssh, (void*)&scpBufferRecv);

    /* make buffer file to send if asked */
    WMEMSET(&scpBufferSend, 0, sizeof(ScpBuffer));
    WMEMCPY(scpBufferSend.name, "test.txt", sizeof("test.txt"));
    scpBufferSend.nameSz   = WSTRLEN("test.txt");
    scpBufferSend.buffer   = fileTmp;
    scpBufferSend.bufferSz = sizeof(fileBuffer);
    scpBufferSend.fileSz   = sizeof(fileTmp);
    scpBufferSend.mode     = 0x1A4;
    wolfSSH_SetScpSendCtx(threadCtx->ssh, (void*)&scpBufferSend);
#endif

    if (!threadCtx->nonBlock)
        ret = wolfSSH_accept(threadCtx->ssh);
    else
        ret = NonBlockSSH_accept(threadCtx->ssh);

    if (ret == WS_SUCCESS) {
        byte* buf = NULL;
        byte* tmpBuf;
        int bufSz, backlogSz = 0, rxSz, txSz, stop = 0, txSum;

        do {
            bufSz = EXAMPLE_BUFFER_SZ + backlogSz;

            tmpBuf = (byte*)realloc(buf, bufSz);
            if (tmpBuf == NULL)
                stop = 1;
            else
                buf = tmpBuf;

            if (!stop) {
                do {
                    rxSz = wolfSSH_stream_read(threadCtx->ssh,
                        buf + backlogSz,
                        EXAMPLE_BUFFER_SZ);
                    if (rxSz <= 0)
                        rxSz = wolfSSH_get_error(threadCtx->ssh);
                } while (rxSz == WS_WANT_READ || rxSz == WS_WANT_WRITE);

                if (rxSz > 0) {
                    backlogSz += rxSz;
                    txSum = 0;
                    txSz = 0;

                    while (backlogSz != txSum && txSz >= 0 && !stop) {
                        txSz = wolfSSH_stream_send(threadCtx->ssh,
                            buf + txSum,
                            backlogSz - txSum);

                        if (txSz > 0) {
                            byte c;
                            const byte matches[] = { 0x03, 0x05, 0x06, 0x00 };

                            c = find_char(matches, buf + txSum, txSz);
                            switch (c) {
                            case 0x03:
                                stop = 1;
                                break;
                            case 0x06:
                                if (wolfSSH_TriggerKeyExchange(threadCtx->ssh)
                                        != WS_SUCCESS)
                                    stop = 1;
                                break;
                            case 0x05:
                                if (dump_stats(threadCtx) <= 0)
                                    stop = 1;
                                break;
                            }
                            txSum += txSz;
                        }
                        else if (txSz != WS_REKEYING)
                            stop = 1;
                    }

                    if (txSum < backlogSz)
                        memmove(buf, buf + txSum, backlogSz - txSum);
                    backlogSz -= txSum;
                }
                else
                    stop = 1;
            }
        } while (!stop);

        free(buf);
    }
    else if (ret == WS_SCP_COMPLETE) {
        printf("scp file transfer completed\n");
#if defined(WOLFSSH_SCP) && defined(NO_FILESYSTEM)
        if (scpBufferRecv.fileSz > 0) {
            word32 z;

            printf("file name : %s\n", scpBufferRecv.name);
            printf("     size : %d\n", scpBufferRecv.fileSz);
            printf("     mode : %o\n", scpBufferRecv.mode);
            printf("    mTime : %lu\n", scpBufferRecv.mTime);
            printf("\n");

            for (z = 0; z < scpBufferRecv.fileSz; z++)
                printf("%c", scpBufferRecv.buffer[z]);
            printf("\n");
        }
#endif
    }
    else if (ret == WS_SFTP_COMPLETE) {
        printf("Use example/echoserver/echoserver for SFTP\n");
    }
    wolfSSH_stream_exit(threadCtx->ssh, 0);
    // WCLOSESOCKET(threadCtx->fd);
    wolfSSH_free(threadCtx->ssh);
    free(threadCtx);

    return;
}

#ifndef NO_FILESYSTEM
static int load_file(const char* fileName, byte* buf, word32 bufSz) {
    FILE* file;
    word32 fileSz;
    word32 readSz;

    if (fileName == NULL) return 0;

    if (WFOPEN(&file, fileName, "rb") != 0)
        return 0;
    fseek(file, 0, SEEK_END);
    fileSz = (word32)ftell(file);
    rewind(file);

    if (fileSz > bufSz) {
        fclose(file);
        return 0;
    }

    readSz = (word32)fread(buf, 1, fileSz, file);
    if (readSz < fileSz) {
        fclose(file);
        return 0;
    }

    fclose(file);

    return fileSz;
}
#endif /* !NO_FILESYSTEM */

static const unsigned char ecc_key_der_256[] =
{
        0x30, 0x77, 0x02, 0x01, 0x01, 0x04, 0x20, 0x45, 0xB6, 0x69,
        0x02, 0x73, 0x9C, 0x6C, 0x85, 0xA1, 0x38, 0x5B, 0x72, 0xE8,
        0xE8, 0xC7, 0xAC, 0xC4, 0x03, 0x8D, 0x53, 0x35, 0x04, 0xFA,
        0x6C, 0x28, 0xDC, 0x34, 0x8D, 0xE1, 0xA8, 0x09, 0x8C, 0xA0,
        0x0A, 0x06, 0x08, 0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x03, 0x01,
        0x07, 0xA1, 0x44, 0x03, 0x42, 0x00, 0x04, 0xBB, 0x33, 0xAC,
        0x4C, 0x27, 0x50, 0x4A, 0xC6, 0x4A, 0xA5, 0x04, 0xC3, 0x3C,
        0xDE, 0x9F, 0x36, 0xDB, 0x72, 0x2D, 0xCE, 0x94, 0xEA, 0x2B,
        0xFA, 0xCB, 0x20, 0x09, 0x39, 0x2C, 0x16, 0xE8, 0x61, 0x02,
        0xE9, 0xAF, 0x4D, 0xD3, 0x02, 0x93, 0x9A, 0x31, 0x5B, 0x97,
        0x92, 0x21, 0x7F, 0xF0, 0xCF, 0x18, 0xDA, 0x91, 0x11, 0x02,
        0x34, 0x86, 0xE8, 0x20, 0x58, 0x33, 0x0B, 0x80, 0x34, 0x89,
        0xD8
};
static const int sizeof_ecc_key_der_256 = sizeof(ecc_key_der_256);

static const unsigned char rsa_key_der_2048[] =
{
    0x30, 0x82, 0x04, 0xA3, 0x02, 0x01, 0x00, 0x02, 0x82, 0x01,
    0x01, 0x00, 0xDA, 0x5D, 0xAD, 0x25, 0x14, 0x76, 0x15, 0x59,
    0xF3, 0x40, 0xFD, 0x3C, 0xB8, 0x62, 0x30, 0xB3, 0x6D, 0xC0,
    0xF9, 0xEC, 0xEC, 0x8B, 0x83, 0x1E, 0x9E, 0x42, 0x9C, 0xCA,
    0x41, 0x6A, 0xD3, 0x8A, 0xE1, 0x52, 0x34, 0xE0, 0x0D, 0x13,
    0x62, 0x7E, 0xD4, 0x0F, 0xAE, 0x5C, 0x4D, 0x04, 0xF1, 0x8D,
    0xFA, 0xC5, 0xAD, 0x77, 0xAA, 0x5A, 0x05, 0xCA, 0xEF, 0xF8,
    0x8D, 0xAB, 0xFF, 0x8A, 0x29, 0x09, 0x4C, 0x04, 0xC2, 0xF5,
    0x19, 0xCB, 0xED, 0x1F, 0xB1, 0xB4, 0x29, 0xD3, 0xC3, 0x6C,
    0xA9, 0x23, 0xDF, 0xA3, 0xA0, 0xE5, 0x08, 0xDE, 0xAD, 0x8C,
    0x71, 0xF9, 0x34, 0x88, 0x6C, 0xED, 0x3B, 0xF0, 0x6F, 0xA5,
    0x0F, 0xAC, 0x59, 0xFF, 0x6B, 0x33, 0xF1, 0x70, 0xFB, 0x8C,
    0xA4, 0xB3, 0x45, 0x22, 0x8D, 0x9D, 0x77, 0x7A, 0xE5, 0x29,
    0x5F, 0x84, 0x14, 0xD9, 0x99, 0xEA, 0xEA, 0xCE, 0x2D, 0x51,
    0xF3, 0xE3, 0x58, 0xFA, 0x5B, 0x02, 0x0F, 0xC9, 0xB5, 0x2A,
    0xBC, 0xB2, 0x5E, 0xD3, 0xC2, 0x30, 0xBB, 0x3C, 0xB1, 0xC3,
    0xEF, 0x58, 0xF3, 0x50, 0x94, 0x28, 0x8B, 0xC4, 0x65, 0x4A,
    0xF7, 0x00, 0xD9, 0x97, 0xD9, 0x6B, 0x4D, 0x8D, 0x95, 0xA1,
    0x8A, 0x62, 0x06, 0xB4, 0x50, 0x11, 0x22, 0x83, 0xB4, 0xEA,
    0x2A, 0xE7, 0xD0, 0xA8, 0x20, 0x47, 0x4F, 0xFF, 0x46, 0xAE,
    0xC5, 0x13, 0xE1, 0x38, 0x8B, 0xF8, 0x54, 0xAF, 0x3A, 0x4D,
    0x2F, 0xF8, 0x1F, 0xD7, 0x84, 0x90, 0xD8, 0x93, 0x05, 0x06,
    0xC2, 0x7D, 0x90, 0xDB, 0xE3, 0x9C, 0xD0, 0xC4, 0x65, 0x5A,
    0x03, 0xAD, 0x00, 0xAC, 0x5A, 0xA2, 0xCD, 0xDA, 0x3F, 0x89,
    0x58, 0x37, 0x53, 0xBF, 0x2B, 0x46, 0x7A, 0xAC, 0x89, 0x41,
    0x2B, 0x5A, 0x2E, 0xE8, 0x76, 0xE7, 0x5E, 0xE3, 0x29, 0x85,
    0xA3, 0x63, 0xEA, 0xE6, 0x86, 0x60, 0x7C, 0x2D, 0x02, 0x03,
    0x01, 0x00, 0x01, 0x02, 0x81, 0xFF, 0x0F, 0x91, 0x1E, 0x06,
    0xC6, 0xAE, 0xA4, 0x57, 0x05, 0x40, 0x5C, 0xCD, 0x37, 0x57,
    0xC8, 0xA1, 0x01, 0xF1, 0xFF, 0xDF, 0x23, 0xFD, 0xCE, 0x1B,
    0x20, 0xAD, 0x1F, 0x00, 0x4C, 0x29, 0x91, 0x6B, 0x15, 0x25,
    0x07, 0x1F, 0xF1, 0xCE, 0xAF, 0xF6, 0xDA, 0xA7, 0x43, 0x86,
    0xD0, 0xF6, 0xC9, 0x41, 0x95, 0xDF, 0x01, 0xBE, 0xC6, 0x26,
    0x24, 0xC3, 0x92, 0xD7, 0xE5, 0x41, 0x9D, 0xB5, 0xFB, 0xB6,
    0xED, 0xF4, 0x68, 0xF1, 0x90, 0x25, 0x39, 0x82, 0x48, 0xE8,
    0xCF, 0x12, 0x89, 0x9B, 0xF5, 0x72, 0xD9, 0x3E, 0x90, 0xF9,
    0xC2, 0xE8, 0x1C, 0xF7, 0x26, 0x28, 0xDD, 0xD5, 0xDB, 0xEE,
    0x0D, 0x97, 0xD6, 0x5D, 0xAE, 0x00, 0x5B, 0x6A, 0x19, 0xFA,
    0x59, 0xFB, 0xF3, 0xF2, 0xD2, 0xCA, 0xF4, 0xE2, 0xC1, 0xB5,
    0xB8, 0x0E, 0xCA, 0xC7, 0x68, 0x47, 0xC2, 0x34, 0xC1, 0x04,
    0x3E, 0x38, 0xF4, 0x82, 0x01, 0x59, 0xF2, 0x8A, 0x6E, 0xF7,
    0x6B, 0x5B, 0x0A, 0xBC, 0x05, 0xA9, 0x27, 0x37, 0xB9, 0xF9,
    0x06, 0x80, 0x54, 0xE8, 0x70, 0x1A, 0xB4, 0x32, 0x93, 0x6B,
    0xF5, 0x26, 0xC7, 0x86, 0xF4, 0x58, 0x05, 0x43, 0xF9, 0x72,
    0x8F, 0xEC, 0x42, 0xA0, 0x3B, 0xBA, 0x35, 0x62, 0xCC, 0xEC,
    0xF4, 0xB3, 0x04, 0xA2, 0xEB, 0xAE, 0x3C, 0x87, 0x40, 0x8E,
    0xFE, 0x8F, 0xDD, 0x14, 0xBE, 0xBD, 0x83, 0xC9, 0xC9, 0x18,
    0xCA, 0x81, 0x7C, 0x06, 0xF9, 0xE3, 0x99, 0x2E, 0xEC, 0x29,
    0xC5, 0x27, 0x56, 0xEA, 0x1E, 0x93, 0xC6, 0xE8, 0x0C, 0x44,
    0xCA, 0x73, 0x68, 0x4A, 0x7F, 0xAE, 0x16, 0x25, 0x1D, 0x12,
    0x25, 0x14, 0x2A, 0xEC, 0x41, 0x69, 0x25, 0xC3, 0x5D, 0xE6,
    0xAE, 0xE4, 0x59, 0x80, 0x1D, 0xFA, 0xBD, 0x9F, 0x33, 0x36,
    0x93, 0x9D, 0x88, 0xD6, 0x88, 0xC9, 0x5B, 0x27, 0x7B, 0x0B,
    0x61, 0x02, 0x81, 0x81, 0x00, 0xDE, 0x01, 0xAB, 0xFA, 0x65,
    0xD2, 0xFA, 0xD2, 0x6F, 0xFE, 0x3F, 0x57, 0x6D, 0x75, 0x7F,
    0x8C, 0xE6, 0xBD, 0xFE, 0x08, 0xBD, 0xC7, 0x13, 0x34, 0x62,
    0x0E, 0x87, 0xB2, 0x7A, 0x2C, 0xA9, 0xCD, 0xCA, 0x93, 0xD8,
    0x31, 0x91, 0x81, 0x2D, 0xD6, 0x68, 0x96, 0xAA, 0x25, 0xE3,
    0xB8, 0x7E, 0xA5, 0x98, 0xA8, 0xE8, 0x15, 0x3C, 0xC0, 0xCE,
    0xDE, 0xF5, 0xAB, 0x80, 0xB1, 0xF5, 0xBA, 0xAF, 0xAC, 0x9C,
    0xC1, 0xB3, 0x43, 0x34, 0xAE, 0x22, 0xF7, 0x18, 0x41, 0x86,
    0x63, 0xA2, 0x44, 0x8E, 0x1B, 0x41, 0x9D, 0x2D, 0x75, 0x6F,
    0x0D, 0x5B, 0x10, 0x19, 0x5D, 0x14, 0xAA, 0x80, 0x1F, 0xEE,
    0x02, 0x3E, 0xF8, 0xB6, 0xF6, 0xEC, 0x65, 0x8E, 0x38, 0x89,
    0x0D, 0x0B, 0x50, 0xE4, 0x11, 0x49, 0x86, 0x39, 0x82, 0xDB,
    0x73, 0xE5, 0x3A, 0x0F, 0x13, 0x22, 0xAB, 0xAD, 0xA0, 0x78,
    0x9B, 0x94, 0x21, 0x02, 0x81, 0x81, 0x00, 0xFB, 0xCD, 0x4C,
    0x52, 0x49, 0x3F, 0x2C, 0x80, 0x94, 0x91, 0x4A, 0x38, 0xEC,
    0x0F, 0x4A, 0x7D, 0x3A, 0x8E, 0xBC, 0x04, 0x90, 0x15, 0x25,
    0x84, 0xFB, 0xD3, 0x68, 0xBD, 0xEF, 0xA0, 0x47, 0xFE, 0xCE,
    0x5B, 0xBF, 0x1D, 0x2A, 0x94, 0x27, 0xFC, 0x51, 0x70, 0xFF,
    0xC9, 0xE9, 0xBA, 0xBE, 0x2B, 0xA0, 0x50, 0x25, 0xD3, 0xE1,
    0xA1, 0x57, 0x33, 0xCC, 0x5C, 0xC7, 0x7D, 0x09, 0xF6, 0xDC,
    0xFB, 0x72, 0x94, 0x3D, 0xCA, 0x59, 0x52, 0x73, 0xE0, 0x6C,
    0x45, 0x0A, 0xD9, 0xDA, 0x30, 0xDF, 0x2B, 0x33, 0xD7, 0x52,
    0x18, 0x41, 0x01, 0xF0, 0xDF, 0x1B, 0x01, 0xC1, 0xD3, 0xB7,
    0x9B, 0x26, 0xF8, 0x1C, 0x8F, 0xFF, 0xC8, 0x19, 0xFD, 0x36,
    0xD0, 0x13, 0xA5, 0x72, 0x42, 0xA3, 0x30, 0x59, 0x57, 0xB4,
    0xDA, 0x2A, 0x09, 0xE5, 0x45, 0x5A, 0x39, 0x6D, 0x70, 0x22,
    0x0C, 0xBA, 0x53, 0x26, 0x8D, 0x02, 0x81, 0x81, 0x00, 0xB1,
    0x3C, 0xC2, 0x70, 0xF0, 0x93, 0xC4, 0x3C, 0xF6, 0xBE, 0x13,
    0x11, 0x98, 0x48, 0x82, 0xE1, 0x19, 0x61, 0xBB, 0x0A, 0x7D,
    0x80, 0x0E, 0x3B, 0xF6, 0xC0, 0xC4, 0xE2, 0xDF, 0x19, 0x03,
    0x23, 0x51, 0x44, 0x41, 0x08, 0x29, 0xB2, 0xE8, 0xC6, 0x50,
    0xCF, 0x5F, 0xDD, 0x49, 0xF5, 0x03, 0xDE, 0xEE, 0x86, 0x82,
    0x6A, 0x5A, 0x0B, 0x4F, 0xDC, 0xBE, 0x63, 0x02, 0x26, 0x91,
    0x18, 0x4E, 0xA1, 0xCE, 0xAF, 0xF1, 0x8E, 0x88, 0xE3, 0x30,
    0xF4, 0xF5, 0xFF, 0x71, 0xEB, 0xDF, 0x23, 0x3E, 0x14, 0x52,
    0x88, 0xCA, 0x3F, 0x03, 0xBE, 0xB4, 0xE1, 0xA0, 0x6E, 0x28,
    0x4E, 0x8A, 0x65, 0x73, 0x5D, 0x85, 0xAA, 0x88, 0x5F, 0x8F,
    0x90, 0xF0, 0x3F, 0x00, 0x63, 0x52, 0x92, 0x6C, 0xD1, 0xC4,
    0x52, 0x0D, 0x5E, 0x04, 0x17, 0x7D, 0x7C, 0xA1, 0x86, 0x54,
    0x5A, 0x9D, 0x0E, 0x0C, 0xDB, 0xA0, 0x21, 0x02, 0x81, 0x81,
    0x00, 0xEA, 0xFE, 0x1B, 0x9E, 0x27, 0xB1, 0x87, 0x6C, 0xB0,
    0x3A, 0x2F, 0x94, 0x93, 0xE9, 0x69, 0x51, 0x19, 0x97, 0x1F,
    0xAC, 0xFA, 0x72, 0x61, 0xC3, 0x8B, 0xE9, 0x2E, 0xB5, 0x23,
    0xAE, 0xE7, 0xC1, 0xCB, 0x00, 0x20, 0x89, 0xAD, 0xB4, 0xFA,
    0xE4, 0x25, 0x75, 0x59, 0xA2, 0x2C, 0x39, 0x15, 0x45, 0x4D,
    0xA5, 0xBE, 0xC7, 0xD0, 0xA8, 0x6B, 0xE3, 0x71, 0x73, 0x9C,
    0xD0, 0xFA, 0xBD, 0xA2, 0x5A, 0x20, 0x02, 0x6C, 0xF0, 0x2D,
    0x10, 0x20, 0x08, 0x6F, 0xC2, 0xB7, 0x6F, 0xBC, 0x8B, 0x23,
    0x9B, 0x04, 0x14, 0x8D, 0x0F, 0x09, 0x8C, 0x30, 0x29, 0x66,
    0xE0, 0xEA, 0xED, 0x15, 0x4A, 0xFC, 0xC1, 0x4C, 0x96, 0xAE,
    0xD5, 0x26, 0x3C, 0x04, 0x2D, 0x88, 0x48, 0x3D, 0x2C, 0x27,
    0x73, 0xF5, 0xCD, 0x3E, 0x80, 0xE3, 0xFE, 0xBC, 0x33, 0x4F,
    0x12, 0x8D, 0x29, 0xBA, 0xFD, 0x39, 0xDE, 0x63, 0xF9, 0x02,
    0x81, 0x81, 0x00, 0x8B, 0x1F, 0x47, 0xA2, 0x90, 0x4B, 0x82,
    0x3B, 0x89, 0x2D, 0xE9, 0x6B, 0xE1, 0x28, 0xE5, 0x22, 0x87,
    0x83, 0xD0, 0xDE, 0x1E, 0x0D, 0x8C, 0xCC, 0x84, 0x43, 0x3D,
    0x23, 0x8D, 0x9D, 0x6C, 0xBC, 0xC4, 0xC6, 0xDA, 0x44, 0x44,
    0x79, 0x20, 0xB6, 0x3E, 0xEF, 0xCF, 0x8A, 0xC4, 0x38, 0xB0,
    0xE5, 0xDA, 0x45, 0xAC, 0x5A, 0xCC, 0x7B, 0x62, 0xBA, 0xA9,
    0x73, 0x1F, 0xBA, 0x27, 0x5C, 0x82, 0xF8, 0xAD, 0x31, 0x1E,
    0xDE, 0xF3, 0x37, 0x72, 0xCB, 0x47, 0xD2, 0xCD, 0xF7, 0xF8,
    0x7F, 0x00, 0x39, 0xDB, 0x8D, 0x2A, 0xCA, 0x4E, 0xC1, 0xCE,
    0xE2, 0x15, 0x89, 0xD6, 0x3A, 0x61, 0xAE, 0x9D, 0xA2, 0x30,
    0xA5, 0x85, 0xAE, 0x38, 0xEA, 0x46, 0x74, 0xDC, 0x02, 0x3A,
    0xAC, 0xE9, 0x5F, 0xA3, 0xC6, 0x73, 0x4F, 0x73, 0x81, 0x90,
    0x56, 0xC3, 0xCE, 0x77, 0x5F, 0x5B, 0xBA, 0x6C, 0x42, 0xF1,
    0x21
};
static const int sizeof_rsa_key_der_2048 = sizeof(rsa_key_der_2048);


/* returns buffer size on success */
static int load_key(byte isEcc, byte* buf, word32 bufSz) {
    word32 sz = 0;

#ifndef NO_FILESYSTEM
    const char* bufName;
    bufName = isEcc ? "./keys/server-key-ecc.der" :
                       "./keys/server-key-rsa.der";
    sz = load_file(bufName, buf, bufSz);
#else
    /* using buffers instead */
    if (isEcc) {
        if ((word32)sizeof_ecc_key_der_256 > bufSz) {
            return 0;
        }
        WMEMCPY(buf, ecc_key_der_256, sizeof_ecc_key_der_256);
        sz = sizeof_ecc_key_der_256;
    }
    else {
        if ((word32)sizeof_rsa_key_der_2048 > bufSz) {
            return 0;
        }
        WMEMCPY(buf, rsa_key_der_2048, sizeof_rsa_key_der_2048);
        sz = sizeof_rsa_key_der_2048;
    }
#endif

    return sz;
}


static INLINE void c32toa(word32 u32, byte* c) {
    c[0] = (u32 >> 24) & 0xff;
    c[1] = (u32 >> 16) & 0xff;
    c[2] = (u32 >>  8) & 0xff;
    c[3] =  u32 & 0xff;
}


/* Map user names to passwords */
/* Use arrays for username and p. The password or public key can
 * be hashed and the hash stored here. Then I won't need the type. */
typedef struct PwMap {
    byte type;
    byte username[32];
    word32 usernameSz;
    byte p[WC_SHA256_DIGEST_SIZE];
    struct PwMap* next;
} PwMap;


typedef struct PwMapList {
    PwMap* head;
} PwMapList;


static PwMap* PwMapNew(PwMapList* list,
    byte type,
    const byte* username,
    word32 usernameSz,
    const byte* p,
    word32 pSz) {
    PwMap* map;

    map = (PwMap*)malloc(sizeof(PwMap));
    if (map != NULL) {
        wc_Sha256 sha;
        byte flatSz[4];

        map->type = type;
        if (usernameSz >= sizeof(map->username))
            usernameSz = sizeof(map->username) - 1;
        memcpy(map->username, username, usernameSz + 1);
        map->username[usernameSz] = 0;
        map->usernameSz = usernameSz;

        wc_InitSha256(&sha);
        c32toa(pSz, flatSz);
        wc_Sha256Update(&sha, flatSz, sizeof(flatSz));
        wc_Sha256Update(&sha, p, pSz);
        wc_Sha256Final(&sha, map->p);

        map->next = list->head;
        list->head = map;
    }

    return map;
}


static void PwMapListDelete(PwMapList* list) {
    if (list != NULL) {
        PwMap* head = list->head;

        while (head != NULL) {
            PwMap* cur = head;
            head = head->next;
            memset(cur, 0, sizeof(PwMap));
            free(cur);
        }
    }
}


static const char samplePasswordBuffer[] =
    "jill:upthehill\n"
    "jack:fetchapail\n";


static const char samplePublicKeyEccBuffer[] =
    "ecdsa-sha2-nistp256 AAAAE2VjZHNhLXNoYTItbmlzdHAyNTYAAAAIbmlzdHAyNTYAAA"
    "BBBNkI5JTP6D0lF42tbxX19cE87hztUS6FSDoGvPfiU0CgeNSbI+aFdKIzTP5CQEJSvm25"
    "qUzgDtH7oyaQROUnNvk= hansel\n"
    "ecdsa-sha2-nistp256 AAAAE2VjZHNhLXNoYTItbmlzdHAyNTYAAAAIbmlzdHAyNTYAAA"
    "BBBKAtH8cqaDbtJFjtviLobHBmjCtG56DMkP6A4M2H9zX2/YCg1h9bYS7WHd9UQDwXO1Hh"
    "IZzRYecXh7SG9P4GhRY= gretel\n";


static const char samplePublicKeyRsaBuffer[] =
    "ssh-rsa AAAAB3NzaC1yc2EAAAADAQABAAABAQC9P3ZFowOsONXHD5MwWiCciXytBRZGho"
    "MNiisWSgUs5HdHcACuHYPi2W6Z1PBFmBWT9odOrGRjoZXJfDDoPi+j8SSfDGsc/hsCmc3G"
    "p2yEhUZUEkDhtOXyqjns1ickC9Gh4u80aSVtwHRnJZh9xPhSq5tLOhId4eP61s+a5pwjTj"
    "nEhBaIPUJO2C/M0pFnnbZxKgJlX7t1Doy7h5eXxviymOIvaCZKU+x5OopfzM/wFkey0EPW"
    "NmzI5y/+pzU5afsdeEWdiQDIQc80H6Pz8fsoFPvYSG+s4/wz0duu7yeeV1Ypoho65Zr+pE"
    "nIf7dO0B8EblgWt+ud+JI8wrAhfE4x hansel\n"
    "ssh-rsa AAAAB3NzaC1yc2EAAAADAQABAAABAQCqDwRVTRVk/wjPhoo66+Mztrc31KsxDZ"
    "+kAV0139PHQ+wsueNpba6jNn5o6mUTEOrxrz0LMsDJOBM7CmG0983kF4gRIihECpQ0rcjO"
    "P6BSfbVTE9mfIK5IsUiZGd8SoE9kSV2pJ2FvZeBQENoAxEFk0zZL9tchPS+OCUGbK4SDjz"
    "uNZl/30Mczs73N3MBzi6J1oPo7sFlqzB6ecBjK2Kpjus4Y1rYFphJnUxtKvB0s+hoaadru"
    "biE57dK6BrH5iZwVLTQKux31uCJLPhiktI3iLbdlGZEctJkTasfVSsUizwVIyRjhVKmbdI"
    "RGwkU38D043AR1h0mUoGCPIKuqcFMf gretel\n";


static int LoadPasswordBuffer(byte* buf, word32 bufSz, PwMapList* list) {
    char* str = (char*)buf;
    char* delimiter;
    char* username;
    char* password;

    /* Each line of passwd.txt is in the format
     *     username:password\n
     * This function modifies the passed-in buffer. */

    if (list == NULL)
        return -1;

    if (buf == NULL || bufSz == 0)
        return 0;

    while (*str != 0) {
        delimiter = strchr(str, ':');
        if (delimiter == NULL) {
            return -1;
        }
        username = str;
        *delimiter = 0;
        password = delimiter + 1;
        str = strchr(password, '\n');
        if (str == NULL) {
            return -1;
        }
        *str = 0;
        str++;
        if (PwMapNew(list,
            WOLFSSH_USERAUTH_PASSWORD,
            (byte*)username,
            (word32)strlen(username),
            (byte*)password,
            (word32)strlen(password)) == NULL) {

            return -1;
        }
    }

    return 0;
}


static int LoadPublicKeyBuffer(byte* buf, word32 bufSz, PwMapList* list) {
    char* str = (char*)buf;
    char* delimiter;
    byte* publicKey64;
    word32 publicKey64Sz;
    byte* username;
    word32 usernameSz;
    byte  publicKey[300];
    word32 publicKeySz;

    /* Each line of passwd.txt is in the format
     *     ssh-rsa AAAB3BASE64ENCODEDPUBLICKEYBLOB username\n
     * This function modifies the passed-in buffer. */
    if (list == NULL)
        return -1;

    if (buf == NULL || bufSz == 0)
        return 0;

    while (*str != 0) {
        /* Skip the public key type. This example will always be ssh-rsa. */
        delimiter = strchr(str, ' ');
        if (delimiter == NULL) {
            return -1;
        }
        str = delimiter + 1;
        delimiter = strchr(str, ' ');
        if (delimiter == NULL) {
            return -1;
        }
        publicKey64 = (byte*)str;
        *delimiter = 0;
        publicKey64Sz = (word32)(delimiter - str);
        str = delimiter + 1;
        delimiter = strchr(str, '\n');
        if (delimiter == NULL) {
            return -1;
        }
        username = (byte*)str;
        *delimiter = 0;
        usernameSz = (word32)(delimiter - str);
        str = delimiter + 1;
        publicKeySz = sizeof(publicKey);

        if (Base64_Decode(publicKey64,
            publicKey64Sz,
            publicKey,
            &publicKeySz) != 0) {

            return -1;
        }

        if (PwMapNew(list,
            WOLFSSH_USERAUTH_PUBLICKEY,
            username,
            usernameSz,
            publicKey,
            publicKeySz) == NULL) {

            return -1;
        }
    }

    return 0;
}


static int wsUserAuth(byte authType,
    WS_UserAuthData* authData,
    void* ctx) {
    PwMapList* list;
    PwMap* map;
    byte authHash[WC_SHA256_DIGEST_SIZE];

    if (ctx == NULL) {
        fprintf(stderr, "wsUserAuth: ctx not set");
        return WOLFSSH_USERAUTH_FAILURE;
    }

    if (authType != WOLFSSH_USERAUTH_PASSWORD &&
        authType != WOLFSSH_USERAUTH_PUBLICKEY) {

        return WOLFSSH_USERAUTH_FAILURE;
    }

    /* Hash the password or public key with its length. */
    {
        wc_Sha256 sha;
        byte flatSz[4];
        wc_InitSha256(&sha);
        if (authType == WOLFSSH_USERAUTH_PASSWORD) {
            c32toa(authData->sf.password.passwordSz, flatSz);
            wc_Sha256Update(&sha, flatSz, sizeof(flatSz));
            wc_Sha256Update(&sha,
                authData->sf.password.password,
                authData->sf.password.passwordSz);
        }
        else if (authType == WOLFSSH_USERAUTH_PUBLICKEY) {
            c32toa(authData->sf.publicKey.publicKeySz, flatSz);
            wc_Sha256Update(&sha, flatSz, sizeof(flatSz));
            wc_Sha256Update(&sha,
                authData->sf.publicKey.publicKey,
                authData->sf.publicKey.publicKeySz);
        }
        wc_Sha256Final(&sha, authHash);
    }

    list = (PwMapList*)ctx;
    map = list->head;

    while (map != NULL) {
        if (authData->usernameSz == map->usernameSz &&
            memcmp(authData->username, map->username, map->usernameSz) == 0) {

            if (authData->type == map->type) {
                if (memcmp(map->p, authHash, WC_SHA256_DIGEST_SIZE) == 0) {
                    return WOLFSSH_USERAUTH_SUCCESS;
                }
                else {
                    return (authType == WOLFSSH_USERAUTH_PASSWORD ?
                            WOLFSSH_USERAUTH_INVALID_PASSWORD :
                            WOLFSSH_USERAUTH_INVALID_PUBLICKEY);
                }
            }
            else {
                return WOLFSSH_USERAUTH_INVALID_AUTHTYPE;
            }
        }
        map = map->next;
    }

    return WOLFSSH_USERAUTH_INVALID_USER;
}


int wolfSshPort = 22222;

void server_test() {
    int DEFAULT_PORT = wolfSshPort;
    int ret = WOLFSSL_SUCCESS; /* assume success until proven wrong */
    int sockfd = 0; /* the socket that will carry our secure connection */
    struct sockaddr_in servAddr;
    size_t len; /* we'll be looking at the length of messages sent and received */

    struct sockaddr_in clientAddr;
    socklen_t          size = sizeof(clientAddr);
    int                on;

    static int mConnd = SOCKET_INVALID;
    static int mShutdown = 0;

    /* declare wolfSSL objects */
    WOLFSSH_CTX *ctx = NULL; /* the wolfSSL context object*/

#ifdef HAVE_SIGNAL
    signal(SIGINT, sig_handler);
#endif

#ifdef DEBUG_WOLFSSL
    wolfSSL_Debugging_ON();
    WOLFSSL_MSG("Debug ON v0.2b");
    //ShowCiphers();
#endif /* DEBUG_WOLFSSL */

#ifndef WOLFSSL_TLS13
    ret = WOLFSSL_FAILURE;
    WOLFSSL_ERROR_MSG("ERROR: Example requires TLS v1.3.\n");
#endif /* WOLFSSL_TLS13 */
    
    /* Initialize the server address struct with zeros */
    memset(&servAddr, 0, sizeof(servAddr));

    /* Fill in the server address */
    servAddr.sin_family      = AF_INET; /* using IPv4      */
    servAddr.sin_port        = htons(DEFAULT_PORT); /* on DEFAULT_PORT */
    servAddr.sin_addr.s_addr = INADDR_ANY; /* from anywhere   */

    /* 
    ***************************************************************************
    * Create a socket that uses an internet IPv4 address,
    * Sets the socket to be stream based (TCP),
    * 0 means choose the default protocol.
    * 
    *  #include <sys/socket.h>
    *
    *  int socket(int domain, int type, int protocol);  
    *  
    *  The socket() function shall create an unbound socket in a communications 
    *  domain, and return a file descriptor that can be used in later function 
    *  calls that operate on sockets.
    *  
    *  The socket() function takes the following arguments:
    *    domain     Specifies the communications domain in which a 
    *                 socket is to be created.
    *    type       Specifies the type of socket to be created.
    *    protocol   Specifies a particular protocol to be used with the socket. 
    *               Specifying a protocol of 0 causes socket() to use an 
    *               unspecified default protocol appropriate for the 
    *               requested socket type.
    *               
    *    The domain argument specifies the address family used in the 
    *    communications domain. The address families supported by the system 
    *    are implementation-defined.
    *    
    *    Symbolic constants that can be used for the domain argument are
    *    defined in the <sys/socket.h> header.
    *
    *  The type argument specifies the socket type, which determines the semantics 
    *  of communication over the socket. The following socket types are defined; 
    *  implementations may specify additional socket types:
    *
    *    SOCK_STREAM    Provides sequenced, reliable, bidirectional, 
    *                   connection-mode byte streams, and may provide a 
    *                   transmission mechanism for out-of-band data.
    *    SOCK_DGRAM     Provides datagrams, which are connectionless-mode,
    *                   unreliable messages of fixed maximum length.
    *    SOCK_SEQPACKET Provides sequenced, reliable, bidirectional, 
    *                   connection-mode transmission paths for records. 
    *                   A record can be sent using one or more output 
    *                   operations and received using one or more input 
    *                   operations, but a single operation never transfers 
    *                   part of more than one record. Record boundaries 
    *                   are visible to the receiver via the MSG_EOR flag.
    *    
    *                   If the protocol argument is non-zero, it shall 
    *                   specify a protocol that is supported by the address 
    *                   family. If the protocol argument is zero, the default
    *                   protocol for this address family and type shall be
    *                   used. The protocols supported by the system are 
    *                   implementation-defined.
    *    
    *    The process may need to have appropriate privileges to use the socket() function or to create some sockets.
    *    
    *  Return Value
    *    Upon successful completion, socket() shall return a non-negative integer, 
    *    the socket file descriptor. Otherwise, a value of -1 shall be returned 
    *    and errno set to indicate the error.
    *    
    *  Errors; The socket() function shall fail if:
    *  
    *    EAFNOSUPPORT    The implementation does not support the specified address family.
    *    EMFILE          No more file descriptors are available for this process.
    *    ENFILE          No more file descriptors are available for the system.
    *    EPROTONOSUPPORT The protocol is not supported by the address family, or the protocol is not supported by the implementation.
    *    EPROTOTYPE      The socket type is not supported by the protocol.
    *    
    *  The socket() function may fail if:
    *  
    *    EACCES  The process does not have appropriate privileges.
    *    ENOBUFS Insufficient resources were available in the system to perform the operation.
    *    ENOMEM  Insufficient memory was available to fulfill the request.
    *    
    *  see: https://linux.die.net/man/3/socket
    ***************************************************************************
    */
    if (ret == WOLFSSL_SUCCESS) {
        /* Upon successful completion, socket() shall return 
         * a non-negative integer, the socket file descriptor.
        */
        sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd > 0) {
            WOLFSSL_MSG("socket creation successful\n");
        }
        else {
            // TODO show errno 
            ret = WOLFSSL_FAILURE;
            WOLFSSL_ERROR_MSG("ERROR: failed to create a socket.\n");
        }
    }
    else {
        WOLFSSL_ERROR_MSG("Skipping socket create.\n");
    }


    /*
    ***************************************************************************
    * set SO_REUSEADDR on socket
    * 
    *  #include <sys/types.h>
    *  # include <sys / socket.h>
    *  int getsockopt(int sockfd,
    *    int level,
    *    int optname,
    *    void *optval,
    *    socklen_t *optlen); int setsockopt(int sockfd,
    *    int level,
    *    int optname,
    *    const void *optval,
    *    socklen_t optlen);
    *    
    *  setsockopt() manipulates options for the socket referred to by the file 
    *  descriptor sockfd. Options may exist at multiple protocol levels; they 
    *  are always present at the uppermost socket level.
    *  
    *  When manipulating socket options, the level at which the option resides 
    *  and the name of the option must be specified. To manipulate options at 
    *  the sockets API level, level is specified as SOL_SOCKET. To manipulate 
    *  options at any other level the protocol number of the appropriate 
    *  protocol controlling the option is supplied. For example, to indicate 
    *  that an option is to be interpreted by the TCP protocol, level should 
    *  be set to the protocol number of TCP
    *  
    *  Return Value
    *    On success, zero is returned. On error, -1 is returned, and errno is set appropriately.
    *
    *  Errors
    *    EBADF       The argument sockfd is not a valid descriptor.
    *    EFAULT      The address pointed to by optval is not in a valid part of the process address space. For getsockopt(), this error may also be returned if optlen is not in a valid part of the process address space.
    *    EINVAL      optlen invalid in setsockopt(). In some cases this error can also occur for an invalid value in optval (e.g., for the IP_ADD_MEMBERSHIP option described in ip(7)).
    *    ENOPROTOOPT The option is unknown at the level indicated.
    *    ENOTSOCK    The argument sockfd is a file, not a socket.
    *
    *  see: https://linux.die.net/man/2/setsockopt
    ***************************************************************************
    */
    if (ret == WOLFSSL_SUCCESS) {
        /* make sure server is setup for reuse addr/port */
        on = 1;
        int soc_ret = setsockopt(sockfd,
            SOL_SOCKET,
            SO_REUSEADDR,
            (char*)&on,
            (socklen_t)sizeof(on));
        
        if (soc_ret == 0) {
            WOLFSSL_MSG("setsockopt re-use addr successful\n");
        }
        else {
            // TODO show errno 
            ret = WOLFSSL_FAILURE;
            WOLFSSL_ERROR_MSG("ERROR: failed to setsockopt addr on socket.\n");
        }
    }
    else {
        WOLFSSL_ERROR_MSG("Skipping setsockopt addr\n");
    }
        
#ifdef SO_REUSEPORT
    /* see above for details on getsockopt  */
    if (ret == WOLFSSL_SUCCESS) {
        int soc_ret = setsockopt(sockfd,
            SOL_SOCKET,
            SO_REUSEPORT,
            (char*)&on,
            (socklen_t)sizeof(on));
            
        if (soc_ret == 0) {
            WOLFSSL_MSG("setsockopt re-use port successful\n");
        }
        else {
            // TODO show errno 
            // ret = WOLFSSL_FAILURE;
            // TODO what's up with the error?
            WOLFSSL_ERROR_MSG("ERROR: failed to setsockopt port on socket.  >> IGNORED << \n");
        }
    } 
    else {
        WOLFSSL_ERROR_MSG("Skipping setsockopt port\n");
    }
#else
    WOLFSSL_MSG("SO_REUSEPORT not configured for setsockopt to re-use port\n");
#endif
    
    /*
    ***************************************************************************
    *  #include <sys/types.h>  
    *  #include <sys/socket.h>
    *  
    *  int bind(int sockfd,
    *      const struct sockaddr *addr,
    *      socklen_t addrlen);
    *      
    *  Description
    *  
    *  When a socket is created with socket(2), it exists in a name 
    *  space(address family) but has no address assigned to it.
    * 
    *  bind() assigns the address specified by addr to the socket referred to 
    *  by the file descriptor sockfd.addrlen specifies the size, in bytes, of 
    *  the address structure pointed to by addr.Traditionally, this operation 
    *  is called "assigning a name to a socket".
    *  
    *   It is normally necessary to assign a local address using bind() before
    *   a SOCK_STREAM socket may receive connections.
    *
    *  Return Value
    *    On success, zero is returned. On error, -1 is returned, and errno is set appropriately.
    *
    *  Errors
    *    EACCES     The address is protected, and the user is not the superuser.
    *    EADDRINUSE The given address is already in use.
    *    EBADF      sockfd is not a valid descriptor.
    *    EINVAL     The socket is already bound to an address.
    *    ENOTSOCK   sockfd is a descriptor for a file, not a socket.
    *
    *   see: https://linux.die.net/man/2/bind
    *   
    *       https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/lwip.html
    ***************************************************************************
    */
    if (ret == WOLFSSL_SUCCESS) {
        /* Bind the server socket to our port */
        int soc_ret = bind(sockfd, (struct sockaddr*)&servAddr, sizeof(servAddr));
        if (soc_ret > -1) {
            WOLFSSL_MSG("socket bind successful\n");
        }
        else {
            ret = WOLFSSL_FAILURE;
            WOLFSSL_ERROR_MSG("ERROR: failed to bind to socket.\n");
        }
    }

    /* 
    ***************************************************************************
    *  Listen for a new connection, allow 5 pending connections 
    *
    *  #include <sys/types.h>  
    *  #include <sys/socket.h>
    *  int listen(int sockfd, int backlog);
    *
    *  Description
    *  
    *  listen() marks the socket referred to by sockfd as a passive socket, 
    *  that is, as a socket that will be used to accept incoming connection 
    *  requests using accept.
    *
    *  The sockfd argument is a file descriptor that refers to a socket of 
    *  type SOCK_STREAM or SOCK_SEQPACKET.
    *  
    *  The backlog argument defines the maximum length to which the queue of 
    *  pending connections for sockfd may grow.If a connection request arrives 
    *  when the queue is full, the client may receive an error with an indication
    *  of ECONNREFUSED or, if the underlying protocol supports retransmission, 
    *  the request may be ignored so that a later reattempt at connection 
    *  succeeds.
    *
    *   Return Value
    *     On success, zero is returned.
    *     On Error, -1 is returned, and errno is set appropriately.
    *   Errors  
    *     EADDRINUSE   Another socket is already listening on the same port.
    *     EBADF        The argument sockfd is not a valid descriptor.
    *     ENOTSOCK     The argument sockfd is not a socket.
    *     EOPNOTSUPP   The socket is not of a type that supports the listen() operation.
    *
    *  ses: https://linux.die.net/man/2/listen
    */
    
    if (ret == WOLFSSL_SUCCESS) {
        int soc_ret = listen(sockfd, 5);
        if (soc_ret > -1) {
            WOLFSSL_MSG("socket listen successful\n");
        }
        else {
            ret = WOLFSSL_FAILURE;
            WOLFSSL_ERROR_MSG("ERROR: failed to listen to socket.\n");
        }
    }    
    
    

    
    PwMapList pwMapList;

    word32 defaultHighwater = EXAMPLE_HIGHWATER_MARK;
    word32 threadCount = 0;
    int port = wolfSshPort;
    char multipleConnections = 0;
    char useEcc = 0;
    int  ch;
    char nonBlock = 0;

#ifdef NO_RSA
    /* If wolfCrypt isn't built with RSA, force ECC on. */
    useEcc = 1;
#endif

    if (wolfSSH_Init() != WS_SUCCESS) {
        // fprintf(stderr, "Couldn't initialize wolfSSH.\n");
        exit(EXIT_FAILURE);
    }

    ctx = wolfSSH_CTX_new(WOLFSSH_ENDPOINT_SERVER, NULL);
    if (ctx == NULL) {
        // fprintf(stderr, "Couldn't allocate SSH CTX data.\n");
        exit(EXIT_FAILURE);
    }

    memset(&pwMapList, 0, sizeof(pwMapList));
    wolfSSH_SetUserAuth(ctx, wsUserAuth);
    wolfSSH_CTX_SetBanner(ctx, serverBanner);


    {
        const char* bufName;
        byte buf[SCRATCH_BUFFER_SZ];
        word32 bufSz;

        bufSz = load_key(useEcc, buf, SCRATCH_BUFFER_SZ);
        if (bufSz == 0) {
            fprintf(stderr, "Couldn't load key.\n");
            exit(EXIT_FAILURE);
        }
        if (wolfSSH_CTX_UsePrivateKey_buffer(ctx,
            buf,
            bufSz,
            WOLFSSH_FORMAT_ASN1) < 0) {
            fprintf(stderr, "Couldn't use key buffer.\n");
            exit(EXIT_FAILURE);
        }

        bufSz = (word32)strlen(samplePasswordBuffer);
        memcpy(buf, samplePasswordBuffer, bufSz);
        buf[bufSz] = 0;
        LoadPasswordBuffer(buf, bufSz, &pwMapList);

        bufName = useEcc ? samplePublicKeyEccBuffer :
                           samplePublicKeyRsaBuffer;
        bufSz = (word32)strlen(bufName);
        memcpy(buf, bufName, bufSz);
        buf[bufSz] = 0;
        LoadPublicKeyBuffer(buf, bufSz, &pwMapList);
    }

    listen(sockfd, 5);

    do {
        int      clientFd = 0;
        struct sockaddr_in clientAddr;
        socklen_t     clientAddrSz = sizeof(clientAddr);

        WOLFSSH*      ssh;
        thread_ctx_t* threadCtx;

        threadCtx = (thread_ctx_t*)malloc(sizeof(thread_ctx_t));
        if (threadCtx == NULL) {
            // fprintf(stderr, "Couldn't allocate thread context data.\n");
            exit(EXIT_FAILURE);
        }

        ssh = wolfSSH_new(ctx);
        if (ssh == NULL) {
            fprintf(stderr, "Couldn't allocate SSH data.\n");
            exit(EXIT_FAILURE);
        }
        wolfSSH_SetUserAuthCtx(ssh, &pwMapList);
        /* Use the session object for its own highwater callback ctx */
        if (defaultHighwater > 0) {
            wolfSSH_SetHighwaterCtx(ssh, (void*)ssh);
            wolfSSH_SetHighwater(ssh, defaultHighwater);
        }

        clientFd = accept(sockfd,
            (struct sockaddr*)&clientAddr,
            &clientAddrSz);
        if (clientFd == -1) {
            WOLFSSL_MSG("ERROR: failed accept");
            exit(EXIT_FAILURE);
        }

//        if (nonBlock)
//            tcp_set_nonblocking(&clientFd);

        wolfSSH_set_fd(ssh, (int)clientFd);

        threadCtx->ssh = ssh;
        threadCtx->fd = clientFd;
        threadCtx->id = threadCount++;
        threadCtx->nonBlock = nonBlock;

        server_worker(threadCtx);

    } while (multipleConnections);

    PwMapListDelete(&pwMapList);
    wolfSSH_CTX_free(ctx);
    if (wolfSSH_Cleanup() != WS_SUCCESS) {
        // fprintf(stderr, "Couldn't clean up wolfSSH.\n");
        exit(EXIT_FAILURE);
    }
#if defined(HAVE_ECC) && defined(FP_ECC) && defined(HAVE_THREAD_LS)
    wc_ecc_fp_free(); /* free per thread cache */
#endif

    return;
}

#endif /* NO_WOLFSSH_SERVER */


//#ifndef NO_MAIN_DRIVER
//
//int main(int argc, char** argv) {
//    func_args args;
//
//    args.argc = argc;
//    args.argv = argv;
//    args.return_code = 0;
//
//    WSTARTTCP();
//
//    ChangeToWolfSshRoot();
//#ifdef DEBUG_WOLFSSH
//    wolfSSH_Debugging_ON();
//#endif
//
//    wolfSSH_Init();
//
//#ifndef NO_WOLFSSH_SERVER
//    server_test(&args);
//#else
//    printf("wolfSSH compiled without server support\n");
//#endif
//
//    wolfSSH_Cleanup();
//
//    return args.return_code;
//}
//
//
//int myoptind = 0;
//char* myoptarg = NULL;
//
//#endif /* NO_MAIN_DRIVER */
