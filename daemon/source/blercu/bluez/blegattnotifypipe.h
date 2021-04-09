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
//  blegattnotifypipe.h
//  SkyBluetoothRcu
//

#ifndef BLEGATTNOTIFYPIPE_H
#define BLEGATTNOTIFYPIPE_H

#include <QObject>
#include <QByteArray>
#include <QDBusUnixFileDescriptor>


class UnixPipeNotifier;


class BleGattNotifyPipe : public QObject
{
	Q_OBJECT

public:
	BleGattNotifyPipe(const QDBusUnixFileDescriptor &notfiyPipeFd,
	                  quint16 mtu, QObject *parent = nullptr);
	~BleGattNotifyPipe();

public:
	bool isValid() const;

signals:
	void notification(const QByteArray &value);
	void closed();

private:
	void onActivated(int pipeFd);

private:
	int m_pipeFd;
	UnixPipeNotifier *m_notifier;

	size_t m_bufferSize;
	quint8 *m_buffer;
};


#endif // !defined(BLEGATTNOTIFYPIPE_H)
