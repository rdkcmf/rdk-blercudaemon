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

add_library( irpairing

        OBJECT
        irpairing.h
        irpairing.cpp
        )


target_include_directories( irpairing

        PUBLIC
           ${CMAKE_CURRENT_LIST_DIR}

        PRIVATE
           ${Qt5Core_INCLUDE_DIRS}
        )

if( IR_INPUT_DEVICE_NAME )
    add_definitions(-DIR_INPUT_DEVICE_NAME=\"${IR_INPUT_DEVICE_NAME}\")
endif()

target_include_directories( irpairing PUBLIC ${CMAKE_CURRENT_LIST_DIR} )
