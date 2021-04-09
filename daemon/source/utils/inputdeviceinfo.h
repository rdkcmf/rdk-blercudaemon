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
//  inputdeviceinfo.h
//  SkyBluetoothRcu
//

#ifndef INPUTDEVICEINFO_H
#define INPUTDEVICEINFO_H

#include "bleaddress.h"

#include <QString>
#include <QDebug>
#include <QMetaType>
#include <QSharedPointer>



#if defined(Q_OS_ANDROID)

class AndroidInputDeviceInfo;
class AndroidInputDeviceManager;

#elif defined(Q_OS_LINUX)

class LinuxInputDeviceInfo;

#endif


class InputDeviceInfo
{
public:
	InputDeviceInfo();
	InputDeviceInfo(const InputDeviceInfo &other);
	~InputDeviceInfo();

	InputDeviceInfo & operator=(const InputDeviceInfo &other);

	bool operator==(const InputDeviceInfo &other) const;

public:
	enum BusType {
		USB,
		HIL,
		Bluetooth,
		Virtual,
		Other
	};

public:
	bool isNull() const;

	int id() const;
	QString name() const;

	bool hasBusType() const;
	BusType busType() const;

	bool hasProductIdentifier() const;
	quint16 productIdentifier() const;

	bool hasVendorIdentifier() const;
	quint16 vendorIdentifier() const;

	bool hasVersion() const;
	quint16 version() const;

	bool matches(const BleAddress &address) const;


private:

	friend QDebug operator<<(QDebug dbg, const InputDeviceInfo &info);

#if defined(Q_OS_ANDROID)

	friend class AndroidInputDeviceManager;
	InputDeviceInfo(const QSharedPointer<AndroidInputDeviceInfo> &deviceInfo);

	QSharedPointer<const AndroidInputDeviceInfo> _d;

#elif defined(Q_OS_LINUX)

	friend class LinuxInputDeviceManager;
	explicit InputDeviceInfo(const QSharedPointer<LinuxInputDeviceInfo> &deviceInfo);

	QSharedPointer<const LinuxInputDeviceInfo> _d;

#endif // defined(Q_OS_LINUX)

};

Q_DECLARE_METATYPE(InputDeviceInfo);

QDebug operator<<(QDebug dbg, const InputDeviceInfo &info);


#endif // !defined(INPUTDEVICEINFO_H)
