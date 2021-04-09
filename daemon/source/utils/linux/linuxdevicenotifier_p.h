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
//  linuxdevicenotifier_p.h
//  SkyBluetoothRcu
//

#ifndef LINUXDEVICENOTIFIER_P_H
#define LINUXDEVICENOTIFIER_P_H

#include "linuxdevicenotifier.h"

#include <QObject>
#include <QList>
#include <QHash>
#include <QString>


class QSocketNotifier;

struct udev;
struct udev_monitor;
struct udev_enumerate;


class LinuxDeviceNotifierImpl : public LinuxDeviceNotifier
{

public:
	LinuxDeviceNotifierImpl(Type type, int netNsFd, QObject *parent);
	~LinuxDeviceNotifierImpl();

	bool isValid() const override;

	void addTagMatchFilter(const QString &tag) override;
	void addSubsystemMatchFilter(LinuxDevice::SubSystem subSystem) override;
	void removeAllFilters() override;

	QList<LinuxDevice> listDevices() const override;
	QList<LinuxDevice> listDevices(LinuxDevice::SubSystem subSystem) const override;

	QString devPath() const override;

private:
	void onMonitorActivated(int udevFd);

	void createUdevMonitor(Type type);

private:
	const Type m_type;
	struct udev *m_udevHandle;
	struct udev_monitor *m_udevMonitor;
	struct udev_enumerate *m_udevEnumerate;

	int m_udevMonitorFd;
	QSocketNotifier *m_udevMonitorNotifier;

private:
	static const QHash<LinuxDevice::SubSystem, QString> m_subSystemNames;
};


#endif // !defined(LINUXDEVICENOTIFIER_P_H)
