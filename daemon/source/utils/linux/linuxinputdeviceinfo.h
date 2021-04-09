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
//  linuxinputdeviceinfo.h
//  SkyBluetoothRcu
//

#ifndef LINUXINPUTDEVICEINFO_H
#define LINUXINPUTDEVICEINFO_H

#include "inputdeviceinfo.h"

#include <QString>

#include <functional>

#include <sys/types.h>


class LinuxDevice;
class LinuxInputDevice;


class LinuxInputDeviceInfo
{
public:
	static QList<LinuxInputDeviceInfo> availableDevices();

public:
	LinuxInputDeviceInfo();
	explicit LinuxInputDeviceInfo(const LinuxInputDevice &device);
	explicit LinuxInputDeviceInfo(const LinuxDevice &device);
	explicit LinuxInputDeviceInfo(const QString &name);
	LinuxInputDeviceInfo(int fd, const QString &path);
	LinuxInputDeviceInfo(const LinuxInputDeviceInfo &other);
	~LinuxInputDeviceInfo();

	LinuxInputDeviceInfo & operator=(const LinuxInputDeviceInfo &other);

	bool isEqual(const LinuxInputDeviceInfo *other) const;
	bool operator==(const LinuxInputDeviceInfo &other) const;

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

	QString path() const;
	QString name() const;
	QString physicalLocation() const;
	QString uniqueIdentifier() const;

	bool hasBusType() const;
	InputDeviceInfo::BusType busType() const;

	bool hasProductIdentifier() const;
	quint16 productIdentifier() const;

	bool hasVendorIdentifier() const;
	quint16 vendorIdentifier() const;

	bool hasVersion() const;
	quint16 version() const;

	bool matches(const BleAddress &address) const;

private:
	void initFromFd(int fd);

private:
	friend class LinuxInputDevice;
	friend class LinuxInputDeviceManager;

	friend QDebug operator<<(QDebug dbg, const LinuxInputDeviceInfo &info);

	static void forEachInputDeviceImpl(const std::function<void(int, const QString&)> &func);

	template< class Function >
	static inline void forEachInputDevice(Function func)
	{
		forEachInputDeviceImpl(func);
	}

	template< class Function, class... Args >
	static inline void forEachInputDevice(Function&& f, Args&&... args)
	{
		forEachInputDeviceImpl(std::bind(std::forward<Function>(f),
		                                 std::forward<Args>(args)...));
	}

	static bool isInputEventDeviceNumber(dev_t deviceNum);

private:
	bool m_isNull;

	QString m_path;
	QString m_name;
	QString m_physicalLocation;
	QString m_uniqueIdentifier;

	int m_id;

	bool m_pnpValid;
	InputDeviceInfo::BusType m_busType;
	quint16 m_vendorId;
	quint16 m_productId;
	quint16 m_version;

};

QDebug operator<<(QDebug dbg, const LinuxInputDeviceInfo &info);

#endif // LINUXINPUTDEVICEINFO_H
