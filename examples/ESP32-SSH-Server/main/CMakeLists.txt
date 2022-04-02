set(srcs "enc28j60_example_main.c" ssh_server.c)

idf_component_register(
                       SRCS "${srcs}"
                       INCLUDE_DIRS "."
                       PRIV_INCLUDE_DIRS
                       	/SysGCC/esp32/esp-idf/v4.4/components/wolfssh
                        /SysGCC/esp32/esp-idf/v4.4/components/wolfssl)
