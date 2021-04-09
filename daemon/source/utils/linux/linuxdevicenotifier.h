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
//  linuxdevicenotifier.h
//  SkyBluetoothRcu
//

#ifndef LINUXDEVICENOTIFIER_H
#define LINUXDEVICENOTIFIER_H

#include "linuxdevice.h"

#include <QObject>
#include <QList>
#include <QSharedPointer>


class LinuxDeviceNotifier : public QObject
{
	Q_OBJECT

public:
	enum Type {
		UDev,
		Kernel
	};

	static QSharedPointer<LinuxDeviceNotifier> create(Type type,
	                                                  int netNsFd = -1,
	                                                  QObject *parent = nullptr);

protected:
	LinuxDeviceNotifier(QObject *parent)
		: QObject(parent)
	{ }

public:
	virtual ~LinuxDeviceNotifier() = default;

public:
	virtual bool isValid() const = 0;

	virtual void addTagMatchFilter(const QString &tag) = 0;
	virtual void addSubsystemMatchFilter(LinuxDevice::SubSystem subSystem) = 0;
	virtual void removeAllFilters() = 0;

	virtual QList<LinuxDevice> listDevices() const = 0;
	virtual QList<LinuxDevice> listDevices(LinuxDevice::SubSystem subSystem) const = 0;

	virtual QString devPath() const = 0;

signals:
	void deviceAdded(const LinuxDevice &device);
	void deviceRemoved(const LinuxDevice &device);

};



#endif // !defined(LINUXDEVICENOTIFIER_H)
