# ESP8266 SSH Server

Connect to Tx/Rx pins on ESP8266 UART via remote SSH.

There's an [ESP-IDF wolfSSH component install](../../ide/Espressif/ESP-IDF/setup_win.bat) for Windows, 
but to get started quickly there's a stale copy of the the components included.

See also the related [ESP-IDF wolfSSL component install](https://github.com/wolfSSL/wolfssl/tree/master/IDE/Espressif/ESP-IDF) for both Windows and bash scripts.

There's also a [blog about ESP8266 UARTs](https://gojimmypi.github.io/SSH-to-ESP8266/) used in this project.

## Configuration

See the [ssh_server_config.h](./main/ssh_server.h) files for various configuration settings.

For private settings (those files with WiFi passords, typically `**/my_private_config.h` excluded in `.gitignore`) 
see [my_config.h](./main/my_config.h).

## Defaults

The default users and passwords are the same as in the [linux server.c example](https://github.com/wolfSSL/wolfssh/blob/8a714b2864e6b5c623da2851af5b5c2d0f9b186b/examples/server/server.c#L412):

User: `jill` password: `upthehill`
User: `jack` password: `fetchapail`

The default port for this demo is `22222`.

Example to connect from linux:

```
ssh jill@192.168.75.39 -p 22222
```

## Quick Start

For convenience ONLY, there's a [static copy of wolfSSL components](https://github.com/gojimmypi/wolfssh/tree/ESP8266_Development/examples/Espressif-component-static).

DO NOT USE those static components for anything other than this demo. 
At some point, the code could contain critical, unresolved CVEs that are fixed 
in the current release. To ensure proper security,
install recent code into the Espressif components directory and 
delete your local copy found in `examples/Espressif-component-static`, 
then remove these lines from the [Makefile](./Makefile):

```
EXTRA_COMPONENT_DIRS = ../Espressif-component-static/
CPPFLAGS += -DWOLFSSL_STALE_EXAMPLE=YES
CFLAGS   += -DWOLFSSL_STALE_EXAMPLE=YES
```

WSL Quick Start, use the [ESPPORT](https://github.com/espressif/esp-idf/issues/1026#issuecomment-331307660) with make:

{% include code_header.html %}
```bash
# change to whatever directory you use for projects
cd /mnt/c/workspace/

git clone https://github.com/gojimmypi/wolfssh.git
cd ./wolfssh
git checkout ESP8266_Development
cd ./examples/ESP8266-SSH-Server

# Reminder that WSL USB devices are called /dev/ttySn and not /dev/TTYUSBn
# For example, on Windows, COM15 is ttyS15 in WSL.
make flash ESPPORT=/dev/ttyS15

```

## Operational Status

The USB port used to program the device should show only a small amount of text at boot time
before the console output is routed to `UART1`. Here is some sample boot text (74800 baud, 8N1):

```
 ets Jan  8 2013,rst cause:2, boot mode:(3,6)

load 0x40100000, len 7288, room 16
tail 8
chksum 0xe4
load 0x3ffe8408, len 24, room 0
tail 8
chksum 0x6d
load 0x3ffe8420, len 3328, room 0
tail 0
chksum 0xab
csum 0xab
```

If everything has gone well, the `Tx` pin of `UART1` (board pin label `2` for `GPIO2`) 
should show a startup message similar to this when pressing the reset button  (74800 baud, 8N1): 

```
ets Jan  8 2013,rst cause:2, boot mode:(3,6)

load 0x40100000, len 7288, room 16
tail 8
chksum 0xe4
load 0x3ffe8408, len 24, room 0
tail 8
chksum 0x6d
load 0x3ffe8420, len 3328, room 0
tail 0
chksum 0xab
csum 0xa
I (44) boot: ESP-IDF v3.4-59-gbbde375b-dirty 2nd stage bootloader
I (45) boot: compile time 19:01:25
I (45) qio_mode: Enabling default flash chip QIO
I (53) boot: SPI Speed      : 40MHz
I (60) boot: SPI Mode       : QIO
I (66) boot: SPI Flash Size : 2MB
I (72) boot: Partition Table:
I (77) boot: ## Label            Usage          Type ST Offset   Length
I (89) boot:  0 nvs              WiFi data        01 02 00009000 00006000
I (100) boot:  1 phy_init         RF data          01 01 0000f000 00001000
I (112) boot:  2 factory          factory app      00 00 00010000 000f0000
I (123) boot: End of partition table
I (130) esp_image: segment 0: paddr=0x00010010 vaddr=0x40210010 size=0x7bfbc (50       7836) map
I (316) esp_image: segment 1: paddr=0x0008bfd4 vaddr=0x4028bfcc size=0x17d40 ( 9       7600) map
I (350) esp_image: segment 2: paddr=0x000a3d1c vaddr=0x3ffe8000 size=0x0070c (         1804) load
I (351) esp_image: segment 3: paddr=0x000a4430 vaddr=0x40100000 size=0x00080 (          128) load
I (362) esp_image: segment 4: paddr=0x000a44b8 vaddr=0x40100080 size=0x05950 ( 2       2864) load
I (382) boot: Loaded app from partition at offset 0x10000
I (407) SSH Server main: Begin main init.
I (408) SSH Server main: wolfSSH debugging on.
I (410) SSH Server main: wolfSSL debugging on.
I (414) wolfssl: Debug ON
I (419) SSH Server main: Begin UART_NUM_0 driver install.
I (429) uart: queue free spaces: 100
I (435) SSH Server main: Done: UART_NUM_0 driver install.
I (444) SSH Server main: Begin uart_enable_swap to UART #2 on pins 13 and 15.
I (456) SSH Server main: Done with uart_enable_swap.
I (465) SSH Server main: Setting up nvs flash for WiFi.
I (474) SSH Server main: Begin setup WiFi STA.
I (483) system_api: Base MAC address is not set, read default base MAC address from EFUSE
I (496) system_api: Base MAC address is not set, read default base MAC address from EFUSE
phy_version: 1167.0, 14a6402, Feb 17 2022, 11:32:25, RTOS new
I (563) phy_init: phy ver: 1167_0
I (581) wifi station: wifi_init_sta finished.
I (704) wifi:state: 0 -> 2 (b0)
I (707) wifi:state: 2 -> 3 (0)
I (710) wifi:state: 3 -> 5 (10)
I (729) wifi:connected with YOURSSID, aid = 1, channel 4, HT20, bssid = YOURMACADDRESS
I (1479) tcpip_adapter: sta ip: 192.168.75.39, mask: 255.255.255.0, gw: 192.168.75.1
I (1482) wifi station: got ip:192.168.75.39
I (1486) wifi station: connected to ap SSID:YOURSSID password:YOURPASSWORD
I (1498) SSH Server main: End setup WiFi STA.
I (1506) wolfssl: sntp_setservername:
I (1512) wolfssl: pool.ntp.org
I (1518) wolfssl: time.nist.gov
I (1524) wolfssl: utcnist.colorado.edu
I (1531) wolfssl: sntp_init done.
I (1537) wolfssl: inet_pton
I (1542) wolfssl: wolfSSL Entering wolfCrypt_Init
I (1551) wolfssl: wolfSSH Server main loop heartbeat!
```

Note in particular the key information after line `I (1479)`:

```
I (729) wifi:connected with YOURSSID, aid = 1, channel 4, HT20, bssid = YOURMACADDRESS
I (1479) tcpip_adapter: sta ip: 192.168.75.39, mask: 255.255.255.0, gw: 192.168.75.1
I (1482) wifi station: got ip:192.168.75.39
I (1486) wifi station: connected to ap SSID:YOURSSID password:YOURPASSWORD
```

The SSH address to use for the connection is Message `I 1482` of this example: `192.168.75.39`

When the SSH server is running, but nothing interesting is happening, the main thread will continued to periodically
show a message:

```
I (2621868) wolfssl: wolfSSH Server main loop heartbeat!
```

When a new connection is made, there will be some wolfSSL diagnostic messages on the console UART1 (UART #1):
```
I (2662183) wolfssl: server_worker started.
I (2662185) wolfssl: Start NonBlockSSH_accept
I (2662414) wolfssl: wolfSSL Entering GetAlgoId
I (2663406) wolfssl: wolfSSL Entering wc_ecc_shared_secret_gen_sync
I (2664326) wolfssl: wolfSSL Leaving wc_ecc_shared_secret_gen_sync, return 0
I (2664329) wolfssl: wolfSSL Leaving wc_ecc_shared_secret_ex, return 0

```

Once an SSH to UART connection is established, and text sent or received from the target device, the data will
be echoed on the console port (`UART0` = `UART #2`) for example when a carriage return is detected:

```
I (2734282) wolfssl: Tx UART!
I (2736914) wolfssl: UART Send Data
I (2736916) TX_TASK: Wrote 1 bytes
I (2736932) wolfssl: UART Rx Data!
I (2736935) RX_TASK: Read 1 bytes: ''
I (2736942) wolfssl: Tx UART!
```


<br />

## Known Issues

If improper GPIO lines are selected, the UART initialization may hang.

When plugged into a PC that goes to sleep and powers down the USB power, the ESP32 device seems to sometimes crash and does not always recover when PC power resumes.

Only one connection is allowed at the time. There may be a delay when an existing connected is unexpecteedly terminated before a new connection can be made.


<br />

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
The ESP32 is 3.3V and typically not 5V tolerant. No ground connection will often cause garbage characters on the UART.

The error `serialException: could not open port` typically means that something else is using the COM port on Windows. 
Check for running instances of Putty, etc.

```
  File "C:\SysGCC\esp32\esp-idf\v4.4\python-env\lib\site-packages\serial\serialwin32.py", line 64, in open
    raise SerialException("could not open port {!r}: {!r}".format(self.portstr, ctypes.WinError()))
serial.serialutil.SerialException: could not open port 'COM9': PermissionError(13, 'Access is denied.', None, 5)
```

<br />

For any technical queries specific to the ESP 32, please open an [issue](https://github.com/espressif/esp-idf/issues) on GitHub.

For any issues related to wolfSSL, please open an [issue](https://github.com/wolfssl/wolfssl/issues) on GitHub, 
visit the [wolfSSL support forum](https://www.wolfssl.com/forums/),
send an email to [support](mailto:support@wolfssl.com),   
or [contact us](https://www.wolfssl.com/contact/).
