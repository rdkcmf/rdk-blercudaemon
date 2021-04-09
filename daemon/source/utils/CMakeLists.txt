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

add_library( utils

                OBJECT

                   logging.cpp
                   dumper.cpp
                   bleuuid.cpp
                   bleaddress.cpp
                   bleconnectionparameters.cpp
                   hcisocket.cpp
                   promise.cpp
                   future.cpp
                   futureaggregator.cpp
                   filedescriptor.cpp
                   unixpipenotifier.cpp
                   unixpipesplicer.cpp
                   statemachine.cpp
                   adpcmcodec.cpp
                   edid.cpp
                   crc32.cpp
                   unixsignalnotifier.cpp
                   unixsignalnotifier_p.h
                   fwimagefile.cpp
                   threadrtsched.cpp
                   threadrtsched.h
                   inputdeviceinfo.cpp

                   logging.h
                   dumper.h
                   bleuuid.h
                   bleaddress.h
                   bleconnectionparameters.h
                   hcisocket.h
                   promise.h
                   future.h
                   futureaggregator.h
                   filedescriptor.h
                   unixpipenotifier.h
                   unixpipesplicer.h
                   statemachine.h
                   voicecodec.h
                   adpcmcodec.h
                   edid.h
                   crc32.h
                   unixsignalnotifier.h
                   fwimagefile.h
                   inputdevicemanager.h
                   inputdeviceinfo.h
                   inputdevice.h
                   hidrawdevice.h
                   hidrawdevicemanager.h
                )

if( ANDROID )

target_sources( utils

                PRIVATE
                    ${CMAKE_CURRENT_LIST_DIR}/android/androidbroadcastreceiver.cpp
                    ${CMAKE_CURRENT_LIST_DIR}/android/androidinputdevicemanager.cpp
                    ${CMAKE_CURRENT_LIST_DIR}/android/androidinputdeviceinfo.cpp
                    ${CMAKE_CURRENT_LIST_DIR}/android/androidparcel.cpp

                PUBLIC
                    ${CMAKE_CURRENT_LIST_DIR}/android/androidbroadcastreceiver.h
                    ${CMAKE_CURRENT_LIST_DIR}/android/androidinputdevicemanager.h
                    ${CMAKE_CURRENT_LIST_DIR}/android/androidinputdeviceinfo.h
                    ${CMAKE_CURRENT_LIST_DIR}/android/androidparcel.h
                )

elseif( LINUX )

target_sources( utils

                PRIVATE
                     ${CMAKE_CURRENT_LIST_DIR}/linux/containerhelpers.cpp
                     ${CMAKE_CURRENT_LIST_DIR}/linux/containerhelpers_p.h
                     ${CMAKE_CURRENT_LIST_DIR}/linux/linuxdevice.cpp
                     ${CMAKE_CURRENT_LIST_DIR}/linux/linuxdevicenotifier.cpp
                     ${CMAKE_CURRENT_LIST_DIR}/linux/linuxdevicenotifier_p.h
                     ${CMAKE_CURRENT_LIST_DIR}/linux/linuxinputdevicemanager.cpp
                     ${CMAKE_CURRENT_LIST_DIR}/linux/linuxinputdeviceinfo.cpp
                     ${CMAKE_CURRENT_LIST_DIR}/linux/linuxinputdevice.cpp
                     ${CMAKE_CURRENT_LIST_DIR}/linux/hidrawdevicemanager.cpp
                     ${CMAKE_CURRENT_LIST_DIR}/linux/hidrawdevicemanager_p.h
                     ${CMAKE_CURRENT_LIST_DIR}/linux/hidrawdevice.cpp
                     ${CMAKE_CURRENT_LIST_DIR}/linux/hidrawdevice_p.h

                PUBLIC
                     ${CMAKE_CURRENT_LIST_DIR}/linux/containerhelpers.h
                     ${CMAKE_CURRENT_LIST_DIR}/linux/linuxdevice.h
                     ${CMAKE_CURRENT_LIST_DIR}/linux/linuxdevicenotifier.h
                     ${CMAKE_CURRENT_LIST_DIR}/linux/linuxinputdevicemanager.h
                     ${CMAKE_CURRENT_LIST_DIR}/linux/linuxinputdeviceinfo.h
                     ${CMAKE_CURRENT_LIST_DIR}/linux/linuxinputdevice.h

                )

endif()

target_include_directories( utils

                PRIVATE
                     ${Qt5Core_INCLUDE_DIRS}

                )
