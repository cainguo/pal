##
 #######################################################################################################################
 #
 #  Copyright (c) 2019 Advanced Micro Devices, Inc. All Rights Reserved.
 #
 #  Permission is hereby granted, free of charge, to any person obtaining a copy
 #  of this software and associated documentation files (the "Software"), to deal
 #  in the Software without restriction, including without limitation the rights
 #  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 #  copies of the Software, and to permit persons to whom the Software is
 #  furnished to do so, subject to the following conditions:
 #
 #  The above copyright notice and this permission notice shall be included in all
 #  copies or substantial portions of the Software.
 #
 #  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 #  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 #  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 #  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 #  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 #  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 #  SOFTWARE.
 #
 #######################################################################################################################

cmake_minimum_required(VERSION 3.5)
project(GPUOPEN CXX)

# Put ZERO_CHECK, INSTALL, etc default targets in a separate folder in VS solutions
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# Include project-wide and AMD-wide CMake options
include(DevDriver)

if(NOT DEFINED GPUOPEN_LIB_NAME)
    set(GPUOPEN_LIB_NAME gpuopen CACHE STRING "Name of generated gpuopen lib")
endif()

### Create GpuOpen Library #############################################################################################
devdriver_library(${GPUOPEN_LIB_NAME} STATIC)

### Cached Project Options #############################################################################################

if(NOT DEFINED GPUOPEN_CLIENT_INTERFACE_MAJOR_VERSION)
    file(STRINGS inc/gpuopen.h GPUOPEN_MAJOR_VERSION REGEX "^#define GPUOPEN_INTERFACE_MAJOR_VERSION [0-9]+")

    if(GPUOPEN_MAJOR_VERSION STREQUAL "")
        message(STATUS "Failed to find GPUOPEN_INTERFACE_MAJOR_VERSION")
    else()
        string(REGEX REPLACE "^#define GPUOPEN_INTERFACE_MAJOR_VERSION " "" GPUOPEN_MAJOR_VERSION ${GPUOPEN_MAJOR_VERSION})
        message(STATUS "Detected GPUOPEN_INTERFACE_MAJOR_VERSION is " ${GPUOPEN_MAJOR_VERSION})
    endif()
    set(GPUOPEN_CLIENT_INTERFACE_MAJOR_VERSION ${GPUOPEN_MAJOR_VERSION} CACHE STRING "Interface version for gpuopen")
endif()
message(STATUS "Building gpuopen lib as ${GPUOPEN_LIB_NAME} GPUOPEN_CLIENT_INTERFACE_MAJOR_VERSION=${GPUOPEN_CLIENT_INTERFACE_MAJOR_VERSION}")

### Core Component #####################################################################################################
# WA: Check that this target hasn't already been added (Only until we have a global CMake context)
if(NOT TARGET ddCore)
    add_subdirectory(core)
endif()
target_link_libraries(${GPUOPEN_LIB_NAME} PUBLIC ddCore)

### Third Party Components #############################################################################################
add_subdirectory(third_party)

target_link_libraries(${GPUOPEN_LIB_NAME} PRIVATE mpack)
target_link_libraries(${GPUOPEN_LIB_NAME} PUBLIC  rapidjson)
target_link_libraries(${GPUOPEN_LIB_NAME} PUBLIC  metrohash)

### Build Defines ######################################################################################################
target_compile_definitions(${GPUOPEN_LIB_NAME} PUBLIC GPUOPEN_CLIENT_INTERFACE_MAJOR_VERSION=${GPUOPEN_CLIENT_INTERFACE_MAJOR_VERSION})

# Add NDEBUG flag in release builds since several code files depend on it.
target_compile_definitions(${GPUOPEN_LIB_NAME} PRIVATE $<$<CONFIG:Release>:NDEBUG>)

### Include Directories ################################################################################################
target_include_directories(${GPUOPEN_LIB_NAME} PUBLIC inc)
target_include_directories(${GPUOPEN_LIB_NAME} PRIVATE src)

### Sources ############################################################################################################

### Core Components ###
target_sources(${GPUOPEN_LIB_NAME} PRIVATE
    src/ddVersion.cpp
    src/session.cpp
    src/sessionManager.cpp
    src/messageChannel.inl
    src/messageChannel.cpp
    src/baseProtocolServer.cpp
    src/baseProtocolClient.cpp
    src/ddTransferManager.cpp
    src/ddClientURIService.cpp
    src/ddURIRequestContext.cpp
    ## Protocols
    src/protocols/ddTransferServer.cpp
    src/protocols/ddTransferClient.cpp
    src/protocols/ddURIServer.cpp
    src/protocols/ddEventClient.cpp
    src/protocols/ddEventServer.cpp
    src/protocols/ddEventServerSession.cpp
    ## Utility functions and classes
    src/util/ddTextWriter.cpp
    src/util/ddStructuredReader.cpp
)

    target_sources(${GPUOPEN_LIB_NAME} PRIVATE
        src/util/ddJsonWriter.cpp
    )

### Local Transport ###
# Local transports are always supported
if(UNIX)
    target_sources(${GPUOPEN_LIB_NAME} PRIVATE
        src/posix/ddPosixSocket.cpp
        src/socketMsgTransport.cpp
    )

endif()

### Helper Classes ###
if(GPUOPEN_BUILD_SERVER_HELPERS)
    target_sources(${GPUOPEN_LIB_NAME} PRIVATE src/devDriverServer.cpp)
endif()
if(GPUOPEN_BUILD_CLIENT_HELPERS)
    target_sources(${GPUOPEN_LIB_NAME} PRIVATE src/devDriverClient.cpp)
endif()

### Standard Driver Protocols ###
if(GPUOPEN_BUILD_STANDARD_DRIVER_PROTOCOLS)
    set(GPUOPEN_BUILD_PROTOCOL_DRIVERCONTROL_SERVER ON)
    set(GPUOPEN_BUILD_PROTOCOL_SETTINGS_SERVER ON)
    set(GPUOPEN_BUILD_PROTOCOL_LOGGING_SERVER ON)
    set(GPUOPEN_BUILD_PROTOCOL_RGP_SERVER ON)
endif()

### Standard Tool Protocols ###
if(GPUOPEN_BUILD_STANDARD_TOOL_PROTOCOLS)
    set(GPUOPEN_BUILD_PROTOCOL_DRIVERCONTROL_CLIENT ON)
    set(GPUOPEN_BUILD_PROTOCOL_SETTINGS_CLIENT ON)
    set(GPUOPEN_BUILD_PROTOCOL_LOGGING_CLIENT ON)
    set(GPUOPEN_BUILD_PROTOCOL_RGP_CLIENT ON)
    set(GPUOPEN_BUILD_PROTOCOL_URI_CLIENT ON)
    set(GPUOPEN_BUILD_PROTOCOL_CRASHDUMP_SERVER ON)
endif()

### Standard Listener Protocols ###
if(GPUOPEN_BUILD_STANDARD_LISTENER_PROTOCOLS)
    set(GPUOPEN_BUILD_PROTOCOL_LOGGING_SERVER ON)
endif()

### Listener Components ###
if(GPUOPEN_BUILD_LISTENER_CORE)
    target_sources(${GPUOPEN_LIB_NAME} PRIVATE
        src/listener.cpp
        listener/listenerCore.cpp
        listener/routerCore.cpp
        listener/transportThread.cpp
        listener/hostMsgTransport.cpp
        listener/listenerServer.cpp
        listener/ddListenerURIService.cpp
        listener/transports/hostTransport.cpp
        listener/transports/socketTransport.cpp
        listener/clientmanagers/listenerClientManager.cpp
    )

    target_include_directories(${GPUOPEN_LIB_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/listener)
endif()

#################
### Protocols ###
#################

### Driver Control Protocol ###
if(GPUOPEN_BUILD_PROTOCOL_DRIVERCONTROL_SERVER)
    target_sources(${GPUOPEN_LIB_NAME} PRIVATE src/protocols/driverControlServer.cpp)
endif()
if(GPUOPEN_BUILD_PROTOCOL_DRIVERCONTROL_CLIENT)
    target_sources(${GPUOPEN_LIB_NAME} PRIVATE src/protocols/driverControlClient.cpp)
endif()

### Settings Protocol ###
if(GPUOPEN_BUILD_PROTOCOL_SETTINGS_SERVER)
    target_sources(${GPUOPEN_LIB_NAME} PRIVATE src/protocols/settingsServer.cpp)
endif()
if(GPUOPEN_BUILD_PROTOCOL_SETTINGS_CLIENT)
    target_sources(${GPUOPEN_LIB_NAME} PRIVATE src/protocols/settingsClient.cpp)
endif()

### Logging Protocol ###
if(GPUOPEN_BUILD_PROTOCOL_LOGGING_SERVER)
    target_sources(${GPUOPEN_LIB_NAME} PRIVATE src/protocols/loggingServer.cpp)
endif()
if(GPUOPEN_BUILD_PROTOCOL_LOGGING_CLIENT)
    target_sources(${GPUOPEN_LIB_NAME} PRIVATE src/protocols/loggingClient.cpp)
endif()

### RGP Protocol ###
if(GPUOPEN_BUILD_PROTOCOL_RGP_SERVER)
    target_sources(${GPUOPEN_LIB_NAME} PRIVATE src/protocols/rgpServer.cpp)
endif()
if(GPUOPEN_BUILD_PROTOCOL_RGP_CLIENT)
    target_sources(${GPUOPEN_LIB_NAME} PRIVATE src/protocols/rgpClient.cpp)
endif()

### Crash Dump Protocol ###
if(GPUOPEN_BUILD_PROTOCOL_CRASHDUMP_SERVER)
    target_sources(${GPUOPEN_LIB_NAME} PRIVATE src/protocols/ddGpuCrashDumpServer.cpp)
endif()
if(GPUOPEN_BUILD_PROTOCOL_CRASHDUMP_CLIENT)
    target_sources(${GPUOPEN_LIB_NAME} PRIVATE src/protocols/ddGpuCrashDumpClient.cpp)
endif()

### URI Client ###
if(GPUOPEN_BUILD_PROTOCOL_URI_CLIENT)
    target_sources(${GPUOPEN_LIB_NAME} PRIVATE src/protocols/ddURIClient.cpp)
endif()

################
### Services ###
################

    target_sources(${GPUOPEN_LIB_NAME} PRIVATE
        src/protocols/ddSettingsService.cpp
        src/protocols/ddPipelineUriService.cpp
        src/protocols/ddInfoService.cpp
    )

# Find Headers Helper ##############################################################################
macro(target_find_headers _target)
    get_target_property(${_target}_INCLUDES_DIRS ${_target} INCLUDE_DIRECTORIES)

    if(${_target}_INCLUDES_DIRS)
        foreach(_include_dir IN ITEMS ${${_target}_INCLUDES_DIRS})
            file(GLOB_RECURSE _include_files
                LIST_DIRECTORIES false
                "${_include_dir}/*.h"
                "${_include_dir}/*.hpp"
            )

            list(APPEND ${_target}_INCLUDES ${_include_files})
        endforeach()

        target_sources(${_target} PRIVATE ${${_target}_INCLUDES})
    endif()
endmacro()

target_find_headers(${GPUOPEN_LIB_NAME})

