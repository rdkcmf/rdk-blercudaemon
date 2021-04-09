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
add_library( tools_blercu

        OBJECT
        blercucontroller1_interface.cpp
        blercucontroller1_interface.h
        blercudebug1_interface.cpp
        blercudebug1_interface.h
        blercudevice1_interface.cpp
        blercudevice1_interface.h
        blercuhcicapture1_interface.cpp
        blercuhcicapture1_interface.h
        blercuinfrared1_interface.cpp
        blercuinfrared1_interface.h
        blercuupgrade1_interface.cpp
        blercuupgrade1_interface.h

        )

target_include_directories( tools_blercu

        PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}

        PRIVATE
        ${Qt5Core_INCLUDE_DIRS}
        ${Qt5DBus_INCLUDE_DIRS}
        )
