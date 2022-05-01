# ESP8266 SSH Server

Connect to Tx/Rx pins on ESP8266 UART via remote SSH.

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
