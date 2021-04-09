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
        "${CMAKE_CURRENT_LIST_DIR}/blercurecovery.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/blegattprofile.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/blegattservice.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/blegattcharacteristic.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/blegattdescriptor.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/blegattnotifypipe.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/blercuadapter.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/blercudevice.cpp"

        "${CMAKE_CURRENT_LIST_DIR}/blercurecovery.h"
        "${CMAKE_CURRENT_LIST_DIR}/blegattprofile_p.h"
        "${CMAKE_CURRENT_LIST_DIR}/blegattservice_p.h"
        "${CMAKE_CURRENT_LIST_DIR}/blegattcharacteristic_p.h"
        "${CMAKE_CURRENT_LIST_DIR}/blegattdescriptor_p.h"
        "${CMAKE_CURRENT_LIST_DIR}/blegattnotifypipe.h"
        "${CMAKE_CURRENT_LIST_DIR}/blercuadapter_p.h"
        "${CMAKE_CURRENT_LIST_DIR}/blercudevice_p.h"
        )

include( ${CMAKE_CURRENT_LIST_DIR}/interfaces/CMakeLists.txt )
