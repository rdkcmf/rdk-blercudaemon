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

# Need the rt library for some monotonic clock code
find_library( LIBRT rt )

# On RDK we require libsystemd for logging
if( RDK )
    find_package( systemd REQUIRED )
endif()

# Check if we libudev, it's needed for monitoring for the addition / removal
# of linux input event device nodes
find_package( udev )
if( UDEV_FOUND )
    add_definitions( -DHAVE_LIBUDEV=1 )
endif()


# Enables Qt file, line and function names in the logging output
add_definitions( -DQT_MESSAGELOGCONTEXT )

# The following is a bit of a hack, qt uses __PRETTY_FUNCTION__ in the logging
# code which is correct, however it leads to very long function names in C++
# and since we already have the file and line number it's more concise to just
# use the method name, so the following line overrides __PRETTY_FUNCTION__ to
# just use the old 'C' code __func__ macro
set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -U __PRETTY_FUNCTION__" )
set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D __PRETTY_FUNCTION__=__func__" )


# Add RDK configuration
if( RDK )
    add_definitions( -DENABLE_PAIRING_SERVER=1 )
    add_definitions( -DENABLE_IRPAIRING=1 )
else()
    add_definitions( -DENABLE_BLERCU_CONN_PARAM_CHANGER=1 )
endif()

# Add the top level source directory to the include paths
include_directories( source )


# Add all the sub-dirs / modules
add_subdirectory( source/services )
add_subdirectory( source/utils )
add_subdirectory( source/dbus )
add_subdirectory( source/blercu )
add_subdirectory( source/irdb )
add_subdirectory( source/configsettings )
if( RDK )
    add_subdirectory( source/irpairing )
endif()
if( BUILD_TYPE STREQUAL "DEBUG" )
    add_subdirectory( source/monitors )
endif()



# Creates and names the BleRcuDaemon executable

add_executable(
        BleRcuDaemon

        # Qt Resource files
        resources/resources.rdk.qrc

        # Top level source files
        source/main.cpp
        source/cmdlineoptions.cpp
        source/cmdlineoptions.h

        # The objects from the sub-directories
        $<TARGET_OBJECTS:utils>
        $<TARGET_OBJECTS:dbus>
        $<TARGET_OBJECTS:blercu>
        $<TARGET_OBJECTS:irdb>
        $<TARGET_OBJECTS:sqlite>
        $<TARGET_OBJECTS:services>
        $<TARGET_OBJECTS:configsettings>
        $<$<BOOL:${RDK}>:$<TARGET_OBJECTS:irpairing>>
        $<$<CONFIG:Debug>:$<TARGET_OBJECTS:monitors>>

        )

find_package(BTMGR)
if(BTMGR_FOUND)
    message("Found BTMGR")
else(BTMGR_FOUND)
     message( SEND_ERROR "cannot find BTMGR" )
endif(BTMGR_FOUND)


# Specifies libraries CMake should link to your target. You
# can link multiple libraries, such as libraries you define in this
# build script, prebuilt third-party libraries, or system libraries.

target_link_libraries( # Specifies the target executable
        BleRcuDaemon

        # Links the Qt5 libraries
        Qt5::Core
        Qt5::DBus

        # Link the Qt Network and WebSockets libraries on debug
        # builds for the debugging server
        $<$<CONFIG:Debug>:Qt5::Network>
        $<$<CONFIG:Debug>:Qt5::WebSockets>

        # Link against libsystemd on RDK because we call their journald functions
        $<$<BOOL:${RDK}>:Systemd::libsystemd>

        # Link against libudev
        UDEV::libudev

        # Adds pthread support (if it's a separate library on target)
        Threads::Threads

        # Adds the other system libraries
        ${LIBRT}

        ${BTMGR_LIBRARIES}

        )

target_compile_definitions(
        BleRcuDaemon

        PRIVATE
        -DBLUETOOTHRCU_VERSION=\"${BLUETOOTHRCU_MAJOR_VERSION}.${BLUETOOTHRCU_MINOR_VERSION}.${BLUETOOTHRCU_MICRO_VERSION}\"

        )


# Specifies the install targets; this includes where to install the actual
# binary and any other support files (ie. dbus config files)

install(
        TARGETS BleRcuDaemon

        RUNTIME
        DESTINATION sbin

)

# Install a dbus config file to allow the daemon to talk to bluez and
# register itself on the bus

install(
        FILES       source/dbus/config/com.sky.blercu.conf
        DESTINATION /etc/dbus-1/system.d/

)

# Install a systemd service script to start the daemon at boot time

install(
        FILES       source/services/sky-bluetoothrcu.service
        DESTINATION /lib/systemd/system/

)



