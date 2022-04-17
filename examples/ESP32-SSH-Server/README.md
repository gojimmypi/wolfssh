# ESP32 SSH Server

Connect to Tx/Rx pins on ESP32 UART via remote SSH.

This particular example utilizes the sample application for the [Espressif Wired ENC28J60 Ethernet](https://github.com/espressif/esp-idf/tree/master/examples/ethernet/enc28j60) 
as well as the [Getting Started - Wi-Fi Station Example](https://github.com/espressif/esp-idf/tree/master/examples/wifi/getting_started/station)
and includes the [wolfSSH library](https://github.com/wolfssl/wolfssh) from [wolfSSL](https://www.wolfssl.com/).

See [tweet thread](https://twitter.com/gojimmypi/status/1510703484886085633?s=20&t=SuiFcn672jlhXtCVh0lRRw).

The code is operational, but yes: also pretty messy at the moment. Cleanup and PR coming soon!

There's an [ESP-IDF wolfSSH component install](../../ide/Espressif/ESP-IDF/setup_win.bat) for Windows. 

See also the related [ESP-IDF wolfSSL component install](https://github.com/wolfSSL/wolfssl/tree/master/IDE/Espressif/ESP-IDF) for both Windows and bash scripts.

## Configuration

See the [ssh_server_config.h](./main/ssh_server.h) files for various configuration settings.

## Defaults

The default users and passwords are the same as in the [linux server.c example](https://github.com/wolfSSL/wolfssh/blob/8a714b2864e6b5c623da2851af5b5c2d0f9b186b/examples/server/server.c#L412):

User: `jill` password: `upthehill`
User: `jack` password: `fetchapail`

The default port for this demo is `22222`.

Example to connect from linux:

```
ssh jill@192.168.75.39 -p 22222
```


# ENC28J60 Example
(See the README.md file in the upper level 'examples' [directory](https://github.com/espressif/esp-idf/tree/master/examples) for more information about examples.)

## Overview

ENC28J60 is a standalone Ethernet controller with a standard SPI interface. This example demonstrates how to drive this controller as an SPI device and then attach to TCP/IP stack.

This is also an example of how to integrate a new Ethernet MAC driver into the `esp_eth` component, without needing to modify the ESP-IDF component.

If you have a more complicated application to go (for example, connect to some IoT cloud via MQTT), you can always reuse the initialization codes in this example.

## How to use example

### Hardware Required

To run this example, you need to prepare following hardwares:
* [ESP32 board](https://docs.espressif.com/projects/esp-idf/en/latest/hw-reference/modules-and-boards.html) (e.g. ESP32-PICO, ESP32 DevKitC, etc)
* ENC28J60 module (the latest revision should be 6)
* **!! IMPORTANT !!** Proper input power source since ENC28J60 is quite power consuming device (it consumes more than 200 mA in peaks when transmitting). If improper power source is used, input voltage may drop and ENC28J60 may either provide nonsense response to host controller via SPI (fail to read registers properly) or it may enter to some strange state in the worst case. There are several options how to resolve it:
  * Power ESP32 board from `USB 3.0`, if board is used as source of power to ENC board.
  * Power ESP32 board from external 5V power supply with current limit at least 1 A, if board is used as source of power to ENC board.
  * Power ENC28J60 from external 3.3V power supply with common GND to ESP32 board. Note that there might be some ENC28J60 boards with integrated voltage regulator on market and so powered by 5 V. Please consult documentation of your board for details.

  If a ESP32 board is used as source of power to ENC board, ensure that that particular board is assembled with voltage regulator capable to deliver current up to 1 A. This is a case of ESP32 DevKitC or ESP-WROVER-KIT, for example. Such setup was tested and works as expected. Other boards may use different voltage regulators and may perform differently.
  **WARNING:** Always consult documentation/schematics associated with particular ENC28J60 and ESP32 boards used in your use-case first.

#### Pin Assignment

* ENC28J60 Ethernet module consumes one SPI interface plus an interrupt GPIO. By default they're connected as follows:

| GPIO   | ENC28J60    |
| ------ | ----------- |
| GPIO19 | SPI_CLK     |
| GPIO23 | SPI_MOSI    |
| GPIO25 | SPI_MISO    |
| GPIO22 | SPI_CS      |
| GPIO4  | Interrupt   |

### Configure the project

```
idf.py menuconfig
```

In the `Example Configuration` menu, set SPI specific configuration, such as SPI host number, GPIO used for MISO/MOSI/CS signal, GPIO for interrupt event and the SPI clock rate, duplex mode.

**Note:** According to ENC28J60 data sheet and our internal testing, SPI clock could reach up to 20MHz, but in practice, the clock speed may depend on your PCB layout/wiring/power source. In this example, the default clock rate is set to 8 MHz since some ENC28J60 silicon revisions may not properly work at frequencies less than 8 MHz.

### Build, Flash, and Run

Build the project and flash it to the board, then run monitor tool to view serial output:

```
idf.py -p PORT build flash monitor
```

(Replace PORT with the name of the serial port to use.)

(To exit the serial monitor, type ``Ctrl-]``.)

See the [Getting Started Guide](https://docs.espressif.com/projects/esp-idf/en/latest/get-started/index.html) for full steps to configure and use ESP-IDF to build projects.

## Example Output

```bash
I (0) cpu_start: Starting scheduler on APP CPU.
I (401) enc28j60: revision: 6
I (411) esp_eth.netif.glue: 00:04:a3:12:34:56
I (411) esp_eth.netif.glue: ethernet attached to netif
I (421) eth_example: Ethernet Started
I (2421) enc28j60: working in 10Mbps
I (2421) enc28j60: working in half duplex
I (2421) eth_example: Ethernet Link Up
I (2421) eth_example: Ethernet HW Addr 00:04:a3:12:34:56
I (4391) esp_netif_handlers: eth ip: 192.168.2.34, mask: 255.255.255.0, gw: 192.168.2.2
I (4391) eth_example: Ethernet Got IP Address
I (4391) eth_example: ~~~~~~~~~~~
I (4391) eth_example: ETHIP:192.168.2.34
I (4401) eth_example: ETHMASK:255.255.255.0
I (4401) eth_example: ETHGW:192.168.2.2
I (4411) eth_example: ~~~~~~~~~~~
```

Now you can ping your ESP32 in the terminal by entering `ping 192.168.2.34` (it depends on the actual IP address you get).

**Notes:**
1. ENC28J60 hasn't burned any valid MAC address in the chip, you need to write an unique MAC address into its internal MAC address register before any traffic happened on TX and RX line.
2. It is recommended to operate the ENC28J60 in full-duplex mode since various errata exist to the half-duplex mode (even though addressed in the example) and due to its poor performance in the half-duplex mode (especially in TCP connections). However, ENC28J60 does not support automatic duplex negotiation. If it is connected to an automatic duplex negotiation enabled network switch or Ethernet controller, then ENC28J60 will be detected as a half-duplex device. To communicate in Full-Duplex mode, ENC28J60 and the remote node (switch, router or Ethernet controller) **must be manually configured for full-duplex operation**:
   * The ENC28J60 can be set to full-duplex in the `Example Configuration` menu.
   * On Ubuntu/Debian Linux distribution use:
    ```
    sudo ethtool -s YOUR_INTERFACE_NAME speed 10 duplex full autoneg off
    ```
   * On Windows, go to `Network Connections` -> `Change adapter options` -> open `Properties` of selected network card -> `Configure` -> `Advanced` -> `Link Speed & Duplex` -> select `10 Mbps Full Duplex in dropdown menu`.
3. Ensure that your wiring between ESP32 board and the ENC28J60 board is realized by short wires with the same length and no wire crossings.
4. CS Hold Time needs to be configured to be at least 210 ns to properly read MAC and MII registers as defined by ENC28J60 Data Sheet. This is automatically configured in the example based on selected SPI clock frequency by computing amount of SPI bit-cycles the CS should stay active after the transmission. However, if your PCB design/wiring requires different value, please update `cs_ena_posttrans` member of `devcfg` structure per your actual needs.


## Troubleshooting


Although [Error -236](https://github.com/wolfSSL/wolfssl/blob/9b5ad6f218f657d8651a56b50b6db1b3946a811c/wolfssl/wolfcrypt/error-crypt.h#L189) 
typically means "_RNG required but not provided_", the reality is the time is probably wrong.

```
wolfssl: wolfSSL Leaving wc_ecc_shared_secret_gen_sync, return -236
wolfssl: wolfSSL Leaving wc_ecc_shared_secret_ex, return -236
```
If the time is set to a reasonable value, and the `-236` error is still occuring, check the [sdkconfig](https://github.com/gojimmypi/wolfssh/blob/ESP32_Development/examples/ESP32-SSH-Server/sdkconfig) 
file for unexpected changes, such as when using the EDP-IDF menuconfig. When in doubt, revert back to repo version.


A message such as `E (545) uart: uart_set_pin(605): tx_io_num error` typically means the pins assigned to be a UART
Tx/Rx are either input-only or output-only. see [gpio_types.h_](https://github.com/espressif/esp-idf/blob/master/components/hal/include/hal/gpio_types.h)
for example GPIO Pins [34](https://github.com/espressif/esp-idf/blob/3aeb80acb66038f14fc2a7606e7516a3e2bfa6c9/components/hal/include/hal/gpio_types.h#L108)
to 39 are input only.

```
E (545) uart: uart_set_pin(605): tx_io_num error
ESP_ERROR_CHECK failed: esp_err_t 0xffffffff (ESP_FAIL) at 0x400870c4
file: "../main/enc28j60_example_main.c" line 250
func: init_UART
expression: uart_set_pin(UART_NUM_1, TXD_PIN, RXD_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE)

```

If there are a lot of garbage characters on the UART Tx/Rx, ensure the proper baud rate, ground connection, and voltage level match. 
The ESP32 is 3.3V and typically not 5V tolerant.

For any technical queries specific to the ESP 32, please open an [issue](https://github.com/espressif/esp-idf/issues) on GitHub.

For any issues related to wolfSSL, please open an [issue](https://github.com/wolfssl/wolfssl/issues) on GitHub, 
visit the [wolfSSL support forum](https://www.wolfssl.com/forums/),
send an email to [support](mailto:support@wolfssl.com),   
or [contact us](https://www.wolfssl.com/contact/).
