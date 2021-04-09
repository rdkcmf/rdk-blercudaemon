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
//  hcimonitor.h
//  BleRcuDaemon
//

#ifndef HCIMONITOR_H
#define HCIMONITOR_H

#include <QIODevice>


class HciMonitorPrivate;

class HciMonitor
{
public:
	explicit HciMonitor(uint deviceId, int netNsFd = -1,
	                    size_t bufferSize = (2 * 1024 * 1024));
	explicit HciMonitor(int hciSocketFd, size_t bufferSize);
	~HciMonitor();

public:
	bool isValid() const;

	int snapLength() const;
	void setSnapLength(int length);

	void clear();

	qint64 dumpBuffer(QIODevice *output, bool includeHeader = true,
	                  bool clearBuffer = true);

private:
	HciMonitorPrivate *_d;
};

#endif // !defined(HCIMONITOR_H)
