######################################################################
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
######################################################################

#  Find the systemd library
#
#  SYSTEMD_FOUND - System has libsystemd
#  SYSTEMD_INCLUDE_DIRS - The systemd include directory(ies)
#  SYSTEMD_LIBRARIES - The libraries needed to use systemd
#=============================================================================

find_path( SYSTEMD_INCLUDE_DIR systemd/sd-login.h )
find_library( SYSTEMD_LIBRARY NAMES libsystemd.so.0 libsystemd.so systemd )

# message( "SYSTEMD_INCLUDE_DIR include dir = ${SYSTEMD_INCLUDE_DIR}" )
# message( "SYSTEMD_LIBRARY lib = ${SYSTEMD_LIBRARY}" )


# handle the QUIETLY and REQUIRED arguments and set SYSTEMD_FOUND to TRUE if
# all listed variables are TRUE
include( FindPackageHandleStandardArgs )

find_package_handle_standard_args( SYSTEMD
        DEFAULT_MSG SYSTEMD_LIBRARY SYSTEMD_INCLUDE_DIR )

mark_as_advanced( SYSTEMD_INCLUDE_DIR SYSTEMD_LIBRARY )


if( SYSTEMD_FOUND )
    set( SYSTEMD_INCLUDE_DIRS ${SYSTEMD_INCLUDE_DIR} )
    set( SYSTEMD_LIBRARIES ${SYSTEMD_LIBRARY} )
endif()

if( SYSTEMD_FOUND AND NOT TARGET Systemd::libsystemd )
    add_library( Systemd::libsystemd SHARED IMPORTED )
    set_target_properties( Systemd::libsystemd PROPERTIES
            IMPORTED_LOCATION "${SYSTEMD_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${SYSTEMD_INCLUDE_DIRS}" )
endif()
