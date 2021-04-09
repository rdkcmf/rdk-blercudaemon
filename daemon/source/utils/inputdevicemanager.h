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
//  inputdevicemanager.h
//  SkyBluetoothRcu
//

#ifndef INPUTDEVICEMANAGER_H
#define INPUTDEVICEMANAGER_H

#include "inputdevice.h"
#include "inputdeviceinfo.h"
#include "bleaddress.h"
#include "dumper.h"

#include <QObject>
#include <QByteArray>
#include <QSharedPointer>


class InputDeviceManager : public QObject
{
	Q_OBJECT

public:
	static QSharedPointer<InputDeviceManager> create(QObject *parent = nullptr);

protected:
	explicit InputDeviceManager(QObject *parent)
		: QObject(parent)
	{
		qRegisterMetaType<InputDeviceInfo>();
	}

public:
	~InputDeviceManager() override = default;

public:
	virtual void dump(Dumper out) const = 0;

public:
	virtual QList<InputDeviceInfo> inputDevices() const = 0;
	virtual InputDeviceInfo findInputDevice(const BleAddress &address) const = 0;
	virtual InputDeviceInfo findInputDevice(const QString &name) const = 0;

	virtual QSharedPointer<InputDevice> getDevice(const BleAddress &address) const = 0;
	virtual QSharedPointer<InputDevice> getDevice(const QString &name) const = 0;
	virtual QSharedPointer<InputDevice> getDevice(const InputDeviceInfo &info) const = 0;

signals:
	void deviceAdded(const InputDeviceInfo &deviceInfo);
	void deviceRemoved(const InputDeviceInfo &deviceInfo);
};


#endif // !defined(INPUTDEVICEMANAGER_H)
