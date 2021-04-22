#######################################################################
# If not stated otherwise in this file or this component's LICENSE file the
# following copyright and licenses apply:
#
# Copyright 2017-2020 Sky UK
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#######################################################################
# Sets the minimum version of CMake required to build.
#
# Note: there is a bug in cmake version 3.8 -> 3.10 that causes the build
# to fail when automoc is enabled (https://gitlab.kitware.com/cmake/cmake/commit/952fc65d140a1b36fc2e580d7ca89de231a04412)
# hence why the minimum version is set to 3.11
cmake_minimum_required( VERSION 3.10.3 )

# Project setup
project( BluetoothRcu VERSION 2.0.3 )

# Set the major and minor version numbers (also used by plugins)
set( BLUETOOTHRCU_MAJOR_VERSION 2 )
set( BLUETOOTHRCU_MINOR_VERSION 0 )
set( BLUETOOTHRCU_MICRO_VERSION 3 )



# CMake seems to mix between different case for the build type,
# so convert it to all uppercase and use that from now on
if( NOT CMAKE_BUILD_TYPE )
    set( CMAKE_BUILD_TYPE "Debug")
endif()
string( TOUPPER "${CMAKE_BUILD_TYPE}" BUILD_TYPE )


# Set the 'AI' build type defines for removing some of the code on release builds
add_definitions( -DAI_DEBUG=1 -DAI_RELEASE=2 )
if( BUILD_TYPE STREQUAL "RELEASE" )
    add_definitions( -DAI_BUILD_TYPE=AI_RELEASE )
elseif( BUILD_TYPE STREQUAL "DEBUG" )
    add_definitions( -DAI_BUILD_TYPE=AI_DEBUG )
else()
    message( FATAL_ERROR "BUILD_TYPE '${BUILD_TYPE}' not supported" )
endif()


# Add definition to determine RDK build
if( RDK STREQUAL "TRUE" )
set( RDK TRUE )
add_definitions( -DRDK )
endif()

if ( ENABLE_IRPAIRING )
    add_definitions( -DENABLE_IRPAIRING=1 )
    add_definitions( -DENABLE_PAIRING_SERVER=1 )
endif()


# Enable C++14 support. The following tells cmake to always add the -std=c++14
# flag (nb - if CMAKE_CXX_EXTENSIONS is ON then we'll get -std=gnu++14 instead)
set( CMAKE_CXX_STANDARD          14  )
set( CMAKE_CXX_STANDARD_REQUIRED ON  )
set( CMAKE_CXX_EXTENSIONS        OFF )



# Set linux variable if building for linux systems (cmake doesn't do this
# by default)
string( TOUPPER "${CMAKE_SYSTEM_NAME}" SYSTEM_NAME )
if( ${SYSTEM_NAME} STREQUAL "LINUX" )
    set( LINUX TRUE )
endif()


# Enable GNU extensions on linux builds
if( LINUX )
    add_definitions( -D_GNU_SOURCE )
endif()


# Thread is just the generic lib link for the pthread libraries (on platforms
# that don't have a separate pthread library this is a NOP)
find_package( Threads REQUIRED )


# Sets the path to the Qt5 libraries and headers.
# You'll need to modify the QT5_SDK path below to point to your Qt5 SDK install

set( Qt5_DIR "${QT5_ABI_SDK}/lib/cmake/Qt5" )
set( Qt5Core_DIR "${QT5_ABI_SDK}/lib/cmake/Qt5Core" )
set( Qt5DBus_DIR "${QT5_ABI_SDK}/lib/cmake/Qt5DBus" )
set( Qt5Network_DIR "${QT5_ABI_SDK}/lib/cmake/Qt5Network" )
set( Qt5WebSockets_DIR "${QT5_ABI_SDK}/lib/cmake/Qt5WebSockets" )


# Find includes in corresponding build directories
set( CMAKE_INCLUDE_CURRENT_DIR ON )

# Instruct CMake to run moc automatically when needed.
set( CMAKE_AUTOMOC ON )

# Instruct CMake to run rcc automatically when needed and not to compress the resources
set( CMAKE_AUTORCC ON )
set( CMAKE_AUTORCC_OPTIONS "-no-compress" )

# Find the Qt5 library
find_package( Qt5Core REQUIRED )
find_package( Qt5DBus REQUIRED )
find_package( Qt5Network REQUIRED )
find_package( Qt5WebSockets REQUIRED )

# Add our local cmake directory to search for components
set( CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_LIST_DIR}/cmake" )



# The following disables an annoying warning "<blah> will change in GCC X.XX"
add_compile_options( -Wno-psabi )


# Add the daemon
add_subdirectory( daemon )

# Add the tools on debug builds
if( BUILD_TYPE STREQUAL "DEBUG" )
    add_subdirectory( tools )
endif()


