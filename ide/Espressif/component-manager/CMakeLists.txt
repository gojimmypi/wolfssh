#
# Copyright (C) 2006-2022 wolfSSL Inc.  All rights reserved.
#
# This file is part of wolfSSH.
#
# Contact licensing@wolfssl.com with any questions or comments.
#
# https://www.wolfssl.com
#/

#
# This is the CMakeLists.txt for the Espressif ESP-IDF wolfMQTT component
#
cmake_minimum_required(VERSION 3.5)

# These flags apply only to compiling this component independently.
# be sure to include this like in the root CMakeFiles.txt
# for proper application-wide use of WOLFSSL_USER_SETTINGS:
#
#   add_compile_definitions("WOLFSSL_USER_SETTINGS")
#   add_compile_definitions("WOLFMQTT_USER_SETTINGS")
#
# if using ESP-IDF prior to 4.4, see:
# https://github.com/wolfSSL/wolfssl/issues/5727
if (CONFIG_DEBUG_WOLFMQTT)
    message(STATUS "Config DEBUG_WOLFMQTT is enabled for components/wolfmqtt")
endif()

if (WOLFSSL_USER_SETTINGS)
    message(STATUS "Component wolfmqtt: Found predefined WOLFSSL_USER_SETTINGS")
else()
    # wolfSSL settings should have occurred in project makefiles
    #message(WARNING "Component wolfmqtt: Setting WOLFSSL_USER_SETTINGS \
                     #(should have been a project-wide setting)")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DWOLFSSL_USER_SETTINGS")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DWOLFSSL_USER_SETTINGS")
endif()

if (WOLFMQTT_USER_SETTINGS)
    message(STATUS "Component wolfmqtt: Found predefined WOLFMQTT_USER_SETTINGS")
else()
    # wolfSSL settings should have occurred in project makefiles. See
    message(WARNING "Component wolfmqtt: Setting WOLFMQTT_USER_SETTINGS
                     (should have been a project-wide setting)")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DWOLFMQTT_USER_SETTINGS")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DWOLFMQTT_USER_SETTINGS")
endif()

if(CONFIG_USE_WOLFMQTT_TLS)
    if (CONFIG_DEBUG_WOLFMQTT)
        message(STATUS "Config USE_WOLFMQTT_TLS is enabled for components/wolfmqtt")
    endif()
    set(MQTT_TLS_DEFINE "ENABLE_MQTT_TLS")
    add_compile_definitions("${MQTT_TLS_DEFINE}")
else()
    if (CONFIG_DEBUG_WOLFMQTT)
        message(STATUS "Config USE_WOLFMQTT_TLS is NOT enabled for components/wolfmqtt")
    endif()
    set(MQTT_TLS_DEFINE "")
endif()

message(STATUS "wolfmqtt CMAKE_C_FLAGS = ${CMAKE_C_FLAGS}")

set(CMAKE_CURRENT_SOURCE_DIR ".")
set(WOLFSSL_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
set(INCLUDE_PATH ${WOLFSSL_ROOT})

    set(COMPONENT_SRCDIRS "./src/"
                          )


set(COMPONENT_REQUIRES lwip wolfssl)

set(COMPONENT_ADD_INCLUDEDIRS
    "."
    "./include"
    )


# list(REMOVE_ITEM ASM_FILES  ".*\\.(asm|S)")

message(STATUS "WOLFSSL_ROOT = ${WOLFSSL_ROOT}")
message(STATUS "ASM_FILES = ${ASM_FILES}")


#idf_component_register(
                        #SRC_DIRS "${COMPONENT_SRCDIRS}"
                        #INCLUDE_DIRS "${COMPONENT_ADD_INCLUDEDIRS}"
                        #REQUIRES "${COMPONENT_REQUIRES}"
                        #EXCLUDE_SRCS "${COMPONENT_SRCEXCLUDE}"
                        ## SRCS wolfssl/wolfcrypt/test/test.c
                        #)
if(CMAKE_BUILD_EARLY_EXPANSION)
    message(STATUS "wolfssl component CMAKE_BUILD_EARLY_EXPANSION:")
    idf_component_register(
                            REQUIRES "${COMPONENT_REQUIRES}"
                           )
else()
    idf_component_register(
                            SRC_DIRS "${COMPONENT_SRCDIRS}"
                            INCLUDE_DIRS "${COMPONENT_ADD_INCLUDEDIRS}"
                            REQUIRES "${COMPONENT_REQUIRES}"
                            EXCLUDE_SRCS "${COMPONENT_SRCEXCLUDE}"
                            PRIV_REQUIRES wolfssl # this will typically only be needed for wolfSSL benchmark
                           )
endif()

# check to see if there's both a local copy and EDP-IDF copy of the wolfmqtt components
if( EXISTS "${CMAKE_HOME_DIRECTORY}/components/wolfmqtt/" AND EXISTS "$ENV{IDF_PATH}/components/wolfmqtt/" )
    message(STATUS  "")
    message(WARNING "WARNING: Found components/wolfmqtt in both local project and IDF_PATH")
    message(STATUS  "")
endif()
# end multiple component check

