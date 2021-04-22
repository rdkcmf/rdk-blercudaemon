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

target_sources( blercu

                PRIVATE
                   "${CMAKE_CURRENT_LIST_DIR}/blercuservicesfactory.cpp"

                   "${CMAKE_CURRENT_LIST_DIR}/blercuservices.h"
                   "${CMAKE_CURRENT_LIST_DIR}/blercuservicesfactory.h"
                   "${CMAKE_CURRENT_LIST_DIR}/blercuaudioservice.h"
                   "${CMAKE_CURRENT_LIST_DIR}/blercubatteryservice.h"
                   "${CMAKE_CURRENT_LIST_DIR}/blercudeviceinfoservice.h"
                   "${CMAKE_CURRENT_LIST_DIR}/blercufindmeservice.h"
                   "${CMAKE_CURRENT_LIST_DIR}/blercuinfraredservice.h"
                   "${CMAKE_CURRENT_LIST_DIR}/blercutouchservice.h"
                   "${CMAKE_CURRENT_LIST_DIR}/blercuupgradeservice.h"
                   "${CMAKE_CURRENT_LIST_DIR}/blercuremotecontrolservice.h"
                )

include( ${CMAKE_CURRENT_LIST_DIR}/gatt/CMakeLists.txt )

if( NOT ANDROID AND NOT RDK )
    include( ${CMAKE_CURRENT_LIST_DIR}/ipc/CMakeLists.txt )
endif()

