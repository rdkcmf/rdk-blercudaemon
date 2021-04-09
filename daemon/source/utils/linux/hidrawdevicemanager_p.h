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
//  hidrawdevicemanager_p.h
//  SkyBluetoothRcu
//

#ifndef HIDRAWDEVICEMANAGER_P_H
#define HIDRAWDEVICEMANAGER_P_H

#include "hidrawdevicemanager.h"

#include <QTimer>
#include <QMap>
#include <QString>
#include <QSharedPointer>


class HidRawDevice;
class LinuxDevice;
class LinuxDeviceNotifier;


class HidRawDeviceManagerImpl : public HidRawDeviceManager
{
public:
	HidRawDeviceManagerImpl(const QSharedPointer<const LinuxDeviceNotifier> &devNotifier,
	                        QObject *parent = nullptr);
	~HidRawDeviceManagerImpl();

	QSharedPointer<HidRawDevice> open(const QByteArray& physicalAddress,
	                                  HidRawDevice::OpenMode mode) const override;

	QSet<QByteArray> physicalAddresses(bool convertToLowerCase = true) const override;

private:
	void onDeviceAdded(const LinuxDevice &device);
	void onDeviceRemoved(const LinuxDevice &device);
	void syncHidRawDeviceMap();

private:
	const QSharedPointer<const LinuxDeviceNotifier> m_deviceNotifier;

	const int m_syncTimerInterval;

	QTimer m_syncTimer;
	QMap<QByteArray, QString> m_hidrawDeviceMap;
};


#endif // !defined(HIDRAWDEVICEMANAGER_P_H)
