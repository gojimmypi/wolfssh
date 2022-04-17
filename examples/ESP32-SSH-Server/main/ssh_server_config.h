#pragma once
#include "driver/gpio.h"

/* */

static int wolfSshPort = 22222;

#define SINGLE_THREADED
#define DEBUG_WOLFSSL
#define DEBUG_WOLFSSH


static const char serverBanner[] = "wolfSSH Example Server\n";

#undef  SO_REUSEPORT

static char nonBlock = 1;
static int echo = 0;

#ifndef EXAMPLE_HIGHWATER_MARK
    #define EXAMPLE_HIGHWATER_MARK 0x3FFF8000 /* 1GB - 32kB */
#endif
#ifndef EXAMPLE_BUFFER_SZ
    #define EXAMPLE_BUFFER_SZ 4096
#endif
#define SCRATCH_BUFFER_SZ 1200


#ifdef WOLFSSL_NUCLEUS
    #define WFD_SET_TYPE FD_SET
    #define WFD_SET NU_FD_Set
    #define WFD_ZERO NU_FD_Init
    #define WFD_ISSET NU_FD_Check
#else
    #define WFD_SET_TYPE fd_set
    #define WFD_SET FD_SET
    #define WFD_ZERO FD_ZERO
    #define WFD_ISSET FD_ISSET
#endif

/**
 ******************************************************************************
 ******************************************************************************
 ** USER SETTINGS BEGIN
 ******************************************************************************
 ******************************************************************************
 **/
static const char *TAG = "eth_example";

/* UART pins and config */
#include "uart_helper.h"
// static const int RX_BUF_SIZE = 1024;

#undef ULX3S
#ifdef ULX3S
    /* reminder GPIO 34 to 39 are input only */
    #define TXD_PIN (GPIO_NUM_32) /* orange */
    #define RXD_PIN (GPIO_NUM_33) /* yellow */
#else
    #define TXD_PIN (GPIO_NUM_17) /* orange */
    #define RXD_PIN (GPIO_NUM_16) /* yellow */
#endif

/* Edgerouter is 57600, others are typically 115200 */
#define BAUD_RATE (57600)


/* ENC28J60 doesn't burn any factory MAC address, we need to set it manually.
   02:00:00 is a Locally Administered OUI range so should not be used except when testing on a LAN under your control.
*/
static uint8_t myMacAddress[] = {
    0x02,
    0x00,
    0x00,
    0x12,
    0x34,
    0x56
};

// see https://tf.nist.gov/tf-cgi/servers.cgi
static const int NTP_SERVER_COUNT = 3;
static const char* ntpServerList[] = {
    "pool.ntp.org",
    "time.nist.gov",
    "utcnist.colorado.edu"
};
static const char * TIME_ZONE = "PST-8";
static const long  gmtOffset_sec = 3600;
static const int   daylightOffset_sec = 3600;


static TickType_t DelayTicks = 10000 / portTICK_PERIOD_MS;
/**
 ******************************************************************************
 ******************************************************************************
 ** USER SETTINGS END
 ******************************************************************************
 ******************************************************************************
 **/
  
static volatile bool EthernetReady = 0;

void ssh_server_config_init();

