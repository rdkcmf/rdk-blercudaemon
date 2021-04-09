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
                    "${CMAKE_CURRENT_LIST_DIR}/btremotes.qrc"
                    "${CMAKE_CURRENT_LIST_DIR}/blercuasservice.cpp"
                    "${CMAKE_CURRENT_LIST_DIR}/blercustatuswebsocket.cpp"
                    "${CMAKE_CURRENT_LIST_DIR}/blercustatuswebsocket.h"
                    "${CMAKE_CURRENT_LIST_DIR}/blercufwupgradeservice.cpp"
                    "${CMAKE_CURRENT_LIST_DIR}/blercufwupgradeservice.h"

                PUBLIC
                    "${CMAKE_CURRENT_LIST_DIR}/blercuasservice.h"

                )


if( ANDROID )

    target_sources( services

                    PRIVATE

                    )

elseif( LINUX )

    target_sources( services

                    PRIVATE
                        "${CMAKE_CURRENT_LIST_DIR}/dbus/asservice.cpp"
                        "${CMAKE_CURRENT_LIST_DIR}/dbus/asservice.h"
                        "${CMAKE_CURRENT_LIST_DIR}/dbus/asrequest.cpp"
                        "${CMAKE_CURRENT_LIST_DIR}/dbus/asrequest.h"
                        "${CMAKE_CURRENT_LIST_DIR}/dbus/asrequest_p.h"
                        "${CMAKE_CURRENT_LIST_DIR}/dbus/asserviceadaptor.cpp"
                        "${CMAKE_CURRENT_LIST_DIR}/dbus/asserviceadaptor.h"

                    )

    target_include_directories( services

                    PRIVATE
                        ${Qt5DBus_INCLUDE_DIRS}

                    )

endif()
