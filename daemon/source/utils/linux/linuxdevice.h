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
//  linuxdevice.h
//  SkyBluetoothRcu
//

#ifndef LINUXDEVICE_H
#define LINUXDEVICE_H

#include <QDebug>
#include <QString>
#include <QMap>

#include <sys/types.h>


struct udev_device;


class LinuxDevice
{
public:
	enum SubSystem {
		UnknownSubSystem,
		InputSubSystem,
		HidRawSubSystem
	};
	Q_ENUMS(SubSystem);

protected:
	friend class LinuxDeviceNotifier;
	friend class LinuxDeviceNotifierImpl;
	LinuxDevice(struct udev_device *dev);

public:
	LinuxDevice();
	LinuxDevice(const LinuxDevice &other);
	~LinuxDevice();

	bool isValid() const;

	SubSystem subSystem() const;
	QString name() const;
	QString path() const;
	dev_t number() const;
	uint major() const;
	uint minor() const;

private:
	SubSystem m_subSystem;
	QString m_name;
	dev_t m_number;

	QString m_basePath;
	mutable QString m_path;

private:
	friend QDebug operator<<(QDebug dbg, const LinuxDevice &device);
	static const QHash<QString, SubSystem> m_subSystemNames;
};

QDebug operator<<(QDebug dbg, const LinuxDevice &device);

Q_DECLARE_METATYPE(LinuxDevice);


#endif // !defined(LINUXDEVICE_H)
