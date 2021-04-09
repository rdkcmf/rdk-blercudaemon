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

# Enables Qt file, line and function names in the logging output
add_definitions( -DQT_MESSAGELOGCONTEXT )

# The following is a bit of a hack, qt uses __PRETTY_FUNCTION__ in the logging
# code which is correct, however it leads to very long function names in C++
# and since we already have the file and line number it's more concise to just
# use the method name, so the following line overrides __PRETTY_FUNCTION__ to
# just use the old 'C' code __func__ macro
set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -U __PRETTY_FUNCTION__" )
set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D __PRETTY_FUNCTION__=__func__" )


# Add the top level source directory to the include paths
include_directories( source )


# Add all the sub-dirs / modules
add_subdirectory( source/blercu )
add_subdirectory( source/dbus )
add_subdirectory( source/readline )
add_subdirectory( source/utils )



# Creates and names a library, sets it as either STATIC
# or SHARED, and provides the relative paths to its source code.
# You can define multiple libraries, and CMake builds them for you.
# Gradle automatically packages shared libraries with your APK.

add_executable( # Sets the name of the library.
        BleRcuConsole

        # Provides a relative path to your source file(s).
        source/main.cpp
        source/console.cpp
        source/console.h
        source/base_cmdhandler.cpp
        source/base_cmdhandler.h
        source/blercu_cmdhandler.cpp
        source/blercu_cmdhandler.h

        # The objects from the sub-directories
        $<TARGET_OBJECTS:tools_dbus>
        $<TARGET_OBJECTS:tools_blercu>
        $<TARGET_OBJECTS:tools_readline>
        $<TARGET_OBJECTS:tools_utils>

        )

target_compile_definitions(
        BleRcuConsole

        PRIVATE
        -DBLUETOOTHRCU_VERSION=\"${BLUETOOTHRCU_MAJOR_VERSION}.${BLUETOOTHRCU_MINOR_VERSION}.${BLUETOOTHRCU_MICRO_VERSION}\"
        )


# Specifies libraries CMake should link to your target library. You
# can link multiple libraries, such as libraries you define in this
# build script, prebuilt third-party libraries, or system libraries.

target_link_libraries( # Specifies the target executable
        BleRcuConsole

        # Links the Qt5 libraries
        Qt5::Core
        Qt5::DBus

        # Adds pthread support (if it's a separate library on target)
        Threads::Threads

        # Need dlopen support
        ${CMAKE_DL_LIBS}

        )


# Specifies the install targets; this includes where to install the actual
# binary and any other support files (ie. dbus config files)

install(
        TARGETS BleRcuConsole

        RUNTIME
        DESTINATION bin/

        )



