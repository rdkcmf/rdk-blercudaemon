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
                   "${CMAKE_CURRENT_LIST_DIR}/gatt_services.cpp"
                   "${CMAKE_CURRENT_LIST_DIR}/gatt_audioservice.cpp"
                   "${CMAKE_CURRENT_LIST_DIR}/gatt_audiopipe.cpp"
                   "${CMAKE_CURRENT_LIST_DIR}/gatt_batteryservice.cpp"
                   "${CMAKE_CURRENT_LIST_DIR}/gatt_deviceinfoservice.cpp"
                   "${CMAKE_CURRENT_LIST_DIR}/gatt_findmeservice.cpp"
                   "${CMAKE_CURRENT_LIST_DIR}/gatt_infraredservice.cpp"
                   "${CMAKE_CURRENT_LIST_DIR}/gatt_infraredsignal.cpp"
                   "${CMAKE_CURRENT_LIST_DIR}/gatt_touchservice.cpp"
                   "${CMAKE_CURRENT_LIST_DIR}/gatt_upgradeservice.cpp"

                   "${CMAKE_CURRENT_LIST_DIR}/gatt_services.h"
                   "${CMAKE_CURRENT_LIST_DIR}/gatt_audioservice.h"
                   "${CMAKE_CURRENT_LIST_DIR}/gatt_audiopipe.h"
                   "${CMAKE_CURRENT_LIST_DIR}/gatt_batteryservice.h"
                   "${CMAKE_CURRENT_LIST_DIR}/gatt_deviceinfoservice.h"
                   "${CMAKE_CURRENT_LIST_DIR}/gatt_findmeservice.h"
                   "${CMAKE_CURRENT_LIST_DIR}/gatt_infraredservice.h"
                   "${CMAKE_CURRENT_LIST_DIR}/gatt_infraredsignal.h"
                   "${CMAKE_CURRENT_LIST_DIR}/gatt_touchservice.h"
                   "${CMAKE_CURRENT_LIST_DIR}/gatt_upgradeservice.h"
                )

# The following define is for the early version of the EC101 RCU that didn't
# have the Infrared service implemented
# target_compile_definitions( blercu PRIVATE EC101_WORKAROUND_MISSING_IR_SERVICE )
