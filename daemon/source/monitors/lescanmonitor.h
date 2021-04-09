/*
* If not stated otherwise in this file or this component's LICENSE file the
* following copyright and licenses apply:
*
* Copyright 2017-2020 Sky UK
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

//
//  lescanmonitor.h
//  BleRcuDaemon
//

#ifndef LESCANMONITOR_H
#define LESCANMONITOR_H

#include <QtGlobal>

class LEScanMonitorPrivate;

class LEScanMonitor
{
public:
	explicit LEScanMonitor(uint deviceId, int netNsFd = -1);
	~LEScanMonitor();

public:
	bool isValid() const;

private:
	LEScanMonitorPrivate *_d;
};

#endif // !defined(LESCANMONITOR_H)
