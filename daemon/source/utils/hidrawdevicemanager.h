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
//  hidrawdevicemanager.h
//  SkyBluetoothRcu
//

#ifndef HIDRAWDEVICEMANAGER_H
#define HIDRAWDEVICEMANAGER_H

#include "hidrawdevice.h"

#include <QObject>
#include <QSet>
#include <QByteArray>
#include <QSharedPointer>


class LinuxDeviceNotifier;

class HidRawDeviceManager : public QObject
{
	Q_OBJECT

public:
	static QSharedPointer<HidRawDeviceManager> create(const QSharedPointer<const LinuxDeviceNotifier> &devNotifier,
	                                                  QObject *parent = nullptr);

protected:
	HidRawDeviceManager(QObject *parent)
		: QObject(parent)
	{ }

public:
	virtual ~HidRawDeviceManager()
	{ }

public:
	virtual QSharedPointer<HidRawDevice> open(const QByteArray& physicalAddress,
	                                          HidRawDevice::OpenMode mode = HidRawDevice::ReadWrite) const = 0;

	virtual QSet<QByteArray> physicalAddresses(bool convertToLowerCase = true) const = 0;

signals:
	void deviceAdded(const QByteArray &physicalAddress);
	void deviceRemoved(const QByteArray &physicalAddress);
};


#endif // !defined(HIDRAWDEVICEMANAGER_H)
