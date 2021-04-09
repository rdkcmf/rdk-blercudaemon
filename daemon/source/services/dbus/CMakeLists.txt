################################################################################
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
################################################################################

target_sources( services

                PRIVATE
                   "${CMAKE_CURRENT_LIST_DIR}/blercucontroller_proxy.cpp"
                   "${CMAKE_CURRENT_LIST_DIR}/blercudevice_proxy.cpp"
                   "${CMAKE_CURRENT_LIST_DIR}/blercucontroller1_adaptor.cpp"
                   "${CMAKE_CURRENT_LIST_DIR}/blercudevice1_adaptor.cpp"
                   "${CMAKE_CURRENT_LIST_DIR}/blercuinfrared1_adaptor.cpp"
                   "${CMAKE_CURRENT_LIST_DIR}/blercuupgrade1_adaptor.cpp"
                   "${CMAKE_CURRENT_LIST_DIR}/blercuvoice1_adaptor.cpp"

                   "${CMAKE_CURRENT_LIST_DIR}/blercucontroller_proxy.h"
                   "${CMAKE_CURRENT_LIST_DIR}/blercudevice_proxy.h"
                   "${CMAKE_CURRENT_LIST_DIR}/blercucontroller1_adaptor.h"
                   "${CMAKE_CURRENT_LIST_DIR}/blercudevice1_adaptor.h"
                   "${CMAKE_CURRENT_LIST_DIR}/blercuinfrared1_adaptor.h"
                   "${CMAKE_CURRENT_LIST_DIR}/blercuupgrade1_adaptor.h"
                   "${CMAKE_CURRENT_LIST_DIR}/blercuvoice1_adaptor.h"
                )

if( BUILD_TYPE STREQUAL "DEBUG" )

    target_sources( services

                    PRIVATE
                        "${CMAKE_CURRENT_LIST_DIR}/blercudebug1_adaptor.cpp"
                        "${CMAKE_CURRENT_LIST_DIR}/blercuhcicapture1_adaptor.cpp"

                        "${CMAKE_CURRENT_LIST_DIR}/blercudebug1_adaptor.h"
                        "${CMAKE_CURRENT_LIST_DIR}/blercuhcicapture1_adaptor.h"
                   )

endif()

target_include_directories( services

                PRIVATE
                   ${Qt5DBus_INCLUDE_DIRS}
                )
