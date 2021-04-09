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
//  hcimonitor_p.h
//  BleRcuDaemon
//

#ifndef HCIMONITOR_P_H
#define HCIMONITOR_P_H

#include "ringbuffer.h"

#include <QThread>
#include <QMutex>
#include <QIODevice>



class HciMonitorPrivate : public QThread
{
	Q_OBJECT

public:
	HciMonitorPrivate(int sockFd, size_t bufferSize, QObject *parent = nullptr);
	~HciMonitorPrivate();

public:
	int snapLength() const;
	void setSnapLength(int length);

	void clear();

	qint64 dumpBuffer(QIODevice *output, bool includeHeader = true,
	                  bool clearBuffer = true);

private:
	void run() override;

private:
	bool createBuffer(size_t size);
	void destroyBuffer();

	quint8* reserveBufferSpace(size_t amount);
	bool readHciPacket();

private:
	mutable QMutex m_lock;

	int m_hciSocketFd;
	int m_snapLength;

	RingBuffer m_buffer;
	quint8 m_controlBuffer[128];

	int m_deathFd;
};




#endif // !defined(HCIMONITOR_P_H)
