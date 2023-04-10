#
#  Copyright (C) 2006-2023 wolfSSL Inc.
#
#  This file is part of wolfSSH.
#
#  wolfSSH is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation; either version 3 of the License, or
#  (at your option) any later version.
#
#  wolfSSH is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with wolfSSH.  If not, see <http://www.gnu.org/licenses/>.
#
# cmake for wolfssh Espressif projects
#
# Version 5.6.0.002x
#
# See https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/build-system.html
#

cmake_minimum_required(VERSION 3.16)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DWOLFSSL_USER_SETTINGS")
set(CMAKE_CURRENT_SOURCE_DIR ".")
set(COMPONENT_REQUIRES wolfssl lwip)

if(CMAKE_BUILD_EARLY_EXPANSION)

    idf_component_register(
                            REQUIRES "${COMPONENT_REQUIRES}"
                            PRIV_REQUIRES esp_timer driver wolfssl # this will typically only be needed for wolfSSH benchmark
                           )

else()
    # set the root of wolfSSH:
    #   set(WOLFSSH_ROOT  "C:/some path/with/spaces")
    #   set(WOLFSSH_ROOT  "/mnt/c/some path/with/spaces")
    #   or use this logic to  assign value from Environment Variable,
    #   or  assume this is an example 7 subdirectories below:
    if( "$ENV{WOLFSSH_ROOT}" STREQUAL "" )
        message(STATUS "Environment Variable WOLFSSH_ROOT not set.")

        message(STATUS "CMAKE_CURRENT_SOURCE_DIR = ${CMAKE_CURRENT_SOURCE_DIR}")
        get_filename_component(THIS_DIR "${CMAKE_CURRENT_SOURCE_DIR}" ABSOLUTE)
        message(STATUS "THIS_DIR = ${THIS_DIR}")

        # We are currently in [root]/ide/Espressif/ESP-IDF/examples/wolfssh_test/components/wolfssh
        # The root of wolfSSH is 7 directories up from here:
        get_filename_component(WOLFSSH_ROOT  "../../../../../../../" ABSOLUTE)
    else()
        set(WOLFSSH_ROOT "$ENV{WOLFSSH_ROOT}")
        if( EXISTS "${WOLFSSH_ROOT}" )
            get_filename_component(WOLFSSH_ROOT  "$ENV{WOLFSSH_ROOT}" ABSOLUTE)
            message(STATUS "Found WOLFSSH_ROOT via Environment Variable:")
        else()
            message(FATAL_ERROR "WOLFSSH_ROOT Environment Variable defined, but path not found:")
            message(STATUS "$ENV{WOLFSSH_ROOT}")
        endif()
    endif()
    message(STATUS "WOLFSSH_ROOT = ${WOLFSSH_ROOT}")
    message(STATUS "WOLFSSH WOLFSSL_ROOT = ${WOLFSSL_ROOT}")


    # Espressif may take several passes through this makefile. Check to see if we found IDF
    string(COMPARE EQUAL "${PROJECT_SOURCE_DIR}" "" WOLFSSH_FOUND_IDF)

    # get a list of all wolfcrypt assembly files; we'll exclude them as they don't target Xtensa
    # file(GLOB EXCLUDE_ASM *.S)
    # file(GLOB_RECURSE EXCLUDE_ASM ${CMAKE_SOURCE_DIR} "${WOLFSSH_ROOT}/wolfcrypt/src/*.S")

        message(STATUS "IDF_PATH = $ENV{IDF_PATH}")
        message(STATUS "WOLFSSH_ROOT = ${WOLFSSH_ROOT}")
        message(STATUS "PROJECT_SOURCE_DIR = ${PROJECT_SOURCE_DIR}")
        message(STATUS "EXCLUDE_ASM = ${EXCLUDE_ASM}")

    set(INCLUDE_PATH \"${WOLFSSH_ROOT}/\"
       )


    # yes, there's a wolfssh directory in the wolfssh repo
    set(WOLFSSH_COMPONENT_SRCDIRS "\"${WOLFSSH_ROOT}/src/\""
    #                      \"${WOLFSSH_ROOT}/\"

       ) # WOLFSSH_COMPONENT_SRCDIRS

    message(STATUS "WOLFSSH_COMPONENT_SRCDIRS = ${WOLFSSH_COMPONENT_SRCDIRS}")




    set(WOLFSSH_PROJECT_DIR "${CMAKE_HOME_DIRECTORY}/components/wolfssh")

    # check to see if there's both a local copy and EDP-IDF copy of the wolfssh and/or wolfssh components
    if( EXISTS "${WOLFSSH_PROJECT_DIR}" AND EXISTS "$ENV{IDF_PATH}/components/wolfssh/" )
        #
        # wolfSSH found in both ESP-IDF and local project - needs to be resolved by user
        #
        message(STATUS "")
        message(STATUS "**************************************************************************************")
        message(STATUS "")
        message(STATUS "Error: Found components/wolfssh in both local project and IDF_PATH")
        message(STATUS "")
        message(STATUS "To proceed: ")
        message(STATUS "")
        message(STATUS "Remove either the local project component: ${WOLFSSH_PROJECT_DIR} ")
        message(STATUS "or the Espressif shared component installed at: $ENV{IDF_PATH}/components/wolfssh/ ")
        message(STATUS "")
        message(FATAL_ERROR "Please use wolfSSh in either local project or Espressif components, but not both.")
        message(STATUS "")
        message(STATUS "**************************************************************************************")
        message(STATUS "")

        # Optional: if you change the above FATAL_ERROR to STATUS you can warn at runtime with this macro definition:
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DWOLFSSH_MULTI_INSTALL_WARNING")

    else()
        if( EXISTS "$ENV{IDF_PATH}/components/wolfssh/" )
            #
            # wolfSSH found in ESP-IDF components and is assumed to be already configured in user_settings.h via setup.
            #
            message(STATUS "")
            message(STATUS "Using components/wolfssh in IDF_PATH = $ENV{IDF_PATH}")
            message(STATUS "")
        else()
            #
            # wolfSSH is not an ESP-IDF component. We need to now determine if it is local and if so if it is part of the wolfSSH repo
            # or if  wolfSSH is simply installed as a local component.
            #

            if( EXISTS "${WOLFSSH_PROJECT_DIR}" )
                #
                # wolfSSH found in local project.
                #
                if( EXISTS "${WOLFSSH_PROJECT_DIR}/wolfssh/" )
                    message(STATUS "")
                    message(STATUS "Using installed project ./components/wolfssh in CMAKE_HOME_DIRECTORY = ${CMAKE_HOME_DIRECTORY}")
                    message(STATUS "")
                    #
                    # Note we already checked above and confirmed there's not another wolfSSH installed in the ESP-IDF components.
                    #
                    # We won't do anything else here, as it will be assumed the original install completed successfully.
                    #
                else() # full wolfSSH not installed in local project
                    #
                    # This is the developer repo mode. wolfSSH will be assumed to be not installed to ESP-IDF nor local project
                    # In this configuration, we are likely running a wolfSSH example found directly in the repo.
                    #
                    message(STATUS "")
                    message(STATUS "Using developer repo ./components/wolfssh in CMAKE_HOME_DIRECTORY = ${CMAKE_HOME_DIRECTORY}")
                    message(STATUS "")

                endif()

            else()
                # we did not find a ./components/wolfssh/include/ directory from this pass of cmake.
                if($WOLFSSH_FOUND_IDF)
                    message(STATUS "")
                    message(STATUS "WARNING: wolfSSH not found.")
                    message(STATUS "")
                else()
                    # probably needs to be re-parsed by Espressif
                    message(STATUS "wolfSSH found IDF. Project Source:${PROJECT_SOURCE_DIR}")
                endif() # else we have not found ESP-IDF yet
            endif() # else not a local wolfSSH component

        endif() #else not an ESP-IDF component
    endif() # else not local copy and EDP-IDF wolfSSH


    if(IS_DIRECTORY ${IDF_PATH}/components/freertos/FreeRTOS-Kernel/)
        set(WOLFSSH_COMPONENT_ADD_INCLUDEDIRS
           "."
           "./include" # this is the location of wolfssh user_settings.h
           "\"${WOLFSSH_ROOT}\""
           "\"${WOLFSSH_ROOT}/wolfssh/\""
           )
    else()

       set(WOLFSSH_COMPONENT_ADD_INCLUDEDIRS
           "."
           "./include" # this is the location of wolfssh user_settings.h
           "\"${WOLFSSH_ROOT}\""
           "\"${WOLFSSH_ROOT}/wolfssh/\""
          )
    endif()


    set(WOLFSSH_COMPONENT_SRCEXCLUDE ""
        )

    spaces2list(COMPONENT_REQUIRES)

    separate_arguments(WOLFSSH_COMPONENT_SRCDIRS         NATIVE_COMMAND "${WOLFSSH_COMPONENT_SRCDIRS}")
    separate_arguments(WOLFSSH_COMPONENT_SRCEXCLUDE      NATIVE_COMMAND "${WOLFSSH_COMPONENT_SRCEXCLUDE}")
    separate_arguments(WOLFSSH_COMPONENT_ADD_INCLUDEDIRS NATIVE_COMMAND "${WOLFSSH_COMPONENT_ADD_INCLUDEDIRS}")

    #
    # See https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/build-system.html#example-component-requirements
    #
    message(STATUS "WOLFSSH_COMPONENT_SRCDIRS=${WOLFSSH_COMPONENT_SRCDIRS}")
    message(STATUS "WOLFSSH_COMPONENT_ADD_INCLUDEDIRS=${WOLFSSH_COMPONENT_ADD_INCLUDEDIRS}")


    # check to see if there's both a local copy and EDP-IDF copy of the wolfssh components
    if( EXISTS "${WOLFSSH_PROJECT_DIR}" AND EXISTS "$ENV{IDF_PATH}/components/wolfssh/" )
        message(STATUS "")
        message(STATUS "")
        message(STATUS "********************************************************************")
        message(STATUS "WARNING: Found components/wolfssh in both local project and IDF_PATH")
        message(STATUS "********************************************************************")
        message(STATUS "")
    endif()
    # end multiple component check

    message(STATUS "WOLFSSH_COMPONENT_ADD_INCLUDEDIRS = ${WOLFSSH_COMPONENT_ADD_INCLUDEDIRS}")
    idf_component_register(
                            SRC_DIRS "${WOLFSSH_COMPONENT_SRCDIRS}"
                            INCLUDE_DIRS "${WOLFSSH_COMPONENT_ADD_INCLUDEDIRS}"
                            REQUIRES "${COMPONENT_REQUIRES}"
                            EXCLUDE_SRCS "${WOLFSSH_COMPONENT_SRCEXCLUDE}"
                            REQUIRES "${COMPONENT_REQUIRES}"
                            PRIV_REQUIRES esp_timer driver  # this will typically only be needed for wolfSSH benchmark
                           )
endif() # NOT CMAKE_BUILD_EARLY_EXPANSION


#
# LIBWOLFSSH_SAVE_INFO(VAR_OUPUT THIS_VAR VAR_RESULT)
#
# Save the THIS_VAR as a string in a macro called VAR_OUPUT
#
# VAR_OUPUT:  the name of the macro to define
# THIS_VAR:   the OUTPUT_VARIABLE result from a execute_process()
# VAR_RESULT: the RESULT_VARIABLE from a execute_process(); "0" if successful.
#
function ( LIBWOLFSSH_SAVE_INFO VAR_OUPUT THIS_VAR VAR_RESULT )
    # is the RESULT_VARIABLE output value 0? If so, IS_VALID_VALUE is true.
    string(COMPARE EQUAL "${VAR_RESULT}" "0" IS_VALID_VALUE)

    # if we had a successful operation, save the THIS_VAR in VAR_OUPUT
    if(${IS_VALID_VALUE})
        # strip newline chars in THIS_VAR parameter and save in VAR_VALUE
        string(REPLACE "\n" ""  VAR_VALUE  ${THIS_VAR})

        # we'll could percolate the value to the parent for possible later use
        # set(${VAR_OUPUT} ${VAR_VALUE} PARENT_SCOPE)

        # but we're only using it here in this function
        set(${VAR_OUPUT} ${VAR_VALUE})

        # we'll print what we found to the console
        message(STATUS "Found ${VAR_OUPUT}=${VAR_VALUE}")

        # the interesting part is defining the VAR_OUPUT name a value to use in the app
        add_definitions(-D${VAR_OUPUT}=\"${VAR_VALUE}\")
    else()
        # if we get here, check the execute_process command and parameters.
        message(STATUS "LIBWOLFSSH_SAVE_INFO encountered a non-zero VAR_RESULT")
        set(${VAR_OUPUT} "Unknown")
    endif()
endfunction() # LIBWOLFSSH_SAVE_INFO

if(NOT CMAKE_BUILD_EARLY_EXPANSION AND NOT ("${WOLFSSH_ROOT}" STREQUAL "") )
    set (git_cmd "git")

    # LIBWOLFSSL_VERSION_GIT_ORIGIN: git config --get remote.origin.url
    execute_process(WORKING_DIRECTORY ${WOLFSSH_ROOT} COMMAND ${git_cmd} "config" "--get" "remote.origin.url" OUTPUT_VARIABLE TMP_OUT RESULT_VARIABLE TMP_RES ERROR_QUIET  )
    LIBWOLFSSH_SAVE_INFO(LIBWOLFSSL_VERSION_GIT_ORIGIN "${TMP_OUT}" "${TMP_RES}")

    # LIBWOLFSSL_VERSION_GIT_BRANCH: git rev-parse --abbrev-ref HEAD
    execute_process(WORKING_DIRECTORY ${WOLFSSH_ROOT} COMMAND ${git_cmd} "rev-parse" "--abbrev-ref" "HEAD" OUTPUT_VARIABLE TMP_OUT RESULT_VARIABLE TMP_RES ERROR_QUIET  )
    LIBWOLFSSH_SAVE_INFO(LIBWOLFSSL_VERSION_GIT_BRANCH "${TMP_OUT}" "${TMP_RES}")

    # LIBWOLFSSL_VERSION_GIT_HASH: git rev-parse HEAD
    execute_process(WORKING_DIRECTORY ${WOLFSSH_ROOT} COMMAND ${git_cmd} "rev-parse" "HEAD" OUTPUT_VARIABLE TMP_OUT RESULT_VARIABLE TMP_RES ERROR_QUIET  )
    LIBWOLFSSH_SAVE_INFO(LIBWOLFSSL_VERSION_GIT_HASH "${TMP_OUT}" "${TMP_RES}")

    # LIBWOLFSSL_VERSION_GIT_SHORT_HASH: git rev-parse --short HEAD
    execute_process(WORKING_DIRECTORY ${WOLFSSH_ROOT} COMMAND ${git_cmd} "rev-parse" "--short" "HEAD" OUTPUT_VARIABLE TMP_OUT RESULT_VARIABLE TMP_RES ERROR_QUIET )
    LIBWOLFSSH_SAVE_INFO(LIBWOLFSSL_VERSION_GIT_SHORT_HASH "${TMP_OUT}" "${TMP_RES}")

    # LIBWOLFSSL_VERSION_GIT_HASH_DATE git show --no-patch --no-notes --pretty=\'\%cd\'
    execute_process(WORKING_DIRECTORY ${WOLFSSH_ROOT} COMMAND ${git_cmd} "show" "--no-patch" "--no-notes" "--pretty=\'\%cd\'" OUTPUT_VARIABLE TMP_OUT RESULT_VARIABLE TMP_RES  )
    LIBWOLFSSH_SAVE_INFO(LIBWOLFSSL_VERSION_GIT_HASH_DATE "${TMP_OUT}" "${TMP_RES}")
endif()