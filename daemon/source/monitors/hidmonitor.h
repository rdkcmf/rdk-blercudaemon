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
//  hidmonitor.h
//  BleRcuDaemon
//

#ifndef HIDMONITOR_H
#define HIDMONITOR_H

#include "ringbuffer.h"

#include <QObject>
#include <QIODevice>
#include <QSharedPointer>


class HidRawDeviceManager;
class HidRawDevice;


class HidMonitor : public QObject
{
	Q_OBJECT

public:
	HidMonitor(const QSharedPointer<HidRawDeviceManager> &hidRawManager,
	           size_t bufferSize = (2 * 1024 * 1024), QObject *parent = nullptr);
	~HidMonitor() final;

public:
	bool isValid() const;

	int snapLength() const;
	void setSnapLength(int length);

public slots:
	qint64 dumpBuffer(QIODevice *output, bool clearBuffer = true);


private slots:
	void onDeviceAdded(const QByteArray &physicalAddress);
	void onDeviceRemoved(const QByteArray &physicalAddress);

	void onReport(int minorNumber, uint reportId, const QByteArray &data);

private:
	quint8* reserveBufferSpace(size_t amount);
	quint8* addEvent(quint8 minorNumber, quint8 type, quint8 size);
	void injectEvent(quint8 minorNumber, quint8 type, const QByteArray &data);

private:
	const QSharedPointer<HidRawDeviceManager> m_hidRawManager;
	QList<QSharedPointer<HidRawDevice>> m_hidRawDevices;

	int m_snapLength;

	RingBuffer m_buffer;
};

#endif // !defined(HIDMONITOR_H)
