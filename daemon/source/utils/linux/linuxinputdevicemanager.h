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
//  linuxinputdevicemanager.h
//  SkyBluetoothRcu
//

#ifndef LINUXINPUTDEVICEMANAGER_H
#define LINUXINPUTDEVICEMANAGER_H

#include "../inputdevicemanager.h"

#include <QList>
#include <QHash>
#include <QEvent>
#include <QReadWriteLock>


class LinuxDeviceNotifier;
class LinuxDevice;


class LinuxInputDeviceManager : public InputDeviceManager
{

public:
	explicit LinuxInputDeviceManager(const QSharedPointer<LinuxDeviceNotifier> &notifier,
	                                 QObject *parent = nullptr);
	~LinuxInputDeviceManager() final = default;

public:
	void dump(Dumper out) const override;

	QList<InputDeviceInfo> inputDevices() const override;
	InputDeviceInfo findInputDevice(const BleAddress &address) const override;
	InputDeviceInfo findInputDevice(const QString &name) const override;

	QSharedPointer<InputDevice> getDevice(const BleAddress &address) const override;
	QSharedPointer<InputDevice> getDevice(const QString &name) const override;
	QSharedPointer<InputDevice> getDevice(const InputDeviceInfo &info) const override;


public:
	bool isValid() const;

private:
	void onInputDeviceAdded(const LinuxDevice &device);
	void onInputDeviceRemoved(const LinuxDevice &device);

private:
	const QSharedPointer<LinuxDeviceNotifier> m_notifier;

};

#endif // !defined(LINUXINPUTDEVICEMANAGER_H)
