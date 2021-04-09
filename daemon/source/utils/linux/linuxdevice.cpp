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
//  linuxdevice.cpp
//  SkyBluetoothRcu
//

#include "linuxdevice.h"
#include "logging.h"

#include <QDirIterator>

#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>

#if defined(HAVE_LIBUDEV)
#  if defined(SKY_FUSION_PLATFORM)
#    include <udev/libudev.h>
#  else
#    include <libudev.h>
#  endif
#endif



const QHash<QString, LinuxDevice::SubSystem> LinuxDevice::m_subSystemNames = {
	{   QStringLiteral("input"),    LinuxDevice::InputSubSystem     },
	{   QStringLiteral("hidraw"),   LinuxDevice::HidRawSubSystem    },

};


LinuxDevice::LinuxDevice()
	: m_subSystem(UnknownSubSystem)
	, m_number(0)
{
}

LinuxDevice::LinuxDevice(struct udev_device *dev)
	: m_subSystem(UnknownSubSystem)
	, m_number(0)
	, m_basePath("/dev/")
{
#if !defined(HAVE_LIBUDEV)

	Q_UNUSED(dev);

#else // defined(HAVE_LIBUDEV)

	m_number = udev_device_get_devnum(dev);

	const char *subSystem = udev_device_get_subsystem(dev);
	if (subSystem && m_subSystemNames.contains(subSystem))
		m_subSystem = m_subSystemNames[subSystem];

	const char *devName = udev_device_get_devpath(dev);
	if (devName)
		m_name = devName;

	// get the device node path
	const char *devNodePath = udev_device_get_devnode(dev);
	if (devNodePath) {

		// we can't always trust it so we check the path exists and it points to
		// a char device node with the right major / minor number
		struct stat statBuf;
		if ((stat(devNodePath, &statBuf) == 0) &&
		    S_ISCHR(statBuf.st_mode) &&
		    (statBuf.st_rdev == m_number)) {

			// it checks out so store the path
			m_path = devNodePath;
		}
	}

#if defined(SKY_FUSION_PLATFORM)
	// if failed to get the actual path then at least get the base path from
	// udev
	if (m_path.isEmpty()) {

		// try and get the base path from udev
		const char *path = nullptr;
		struct udev *udev = udev_device_get_udev(dev);
		if (udev == nullptr)
			qWarning("failed to get udev from device");
		else if ((path = udev_get_dev_path(udev)) == nullptr)
			qWarning("failed to get udev base /dev path");
		else
			m_basePath = path;
	}
#endif // defined(SKY_FUSION_PLATFORM)

#endif // defined(HAVE_LIBUDEV)
}

LinuxDevice::LinuxDevice(const LinuxDevice &other)
	: m_subSystem(other.m_subSystem)
	, m_name(other.m_name)
	, m_number(other.m_number)
	, m_path(other.m_path)
{
}

LinuxDevice::~LinuxDevice()
{
}

bool LinuxDevice::isValid() const
{
	return (m_number != 0);
}

LinuxDevice::SubSystem LinuxDevice::subSystem() const
{
	return m_subSystem;
}

QString LinuxDevice::name() const
{
	return m_name;
}

QString LinuxDevice::path() const
{
	// sanity check
	if (Q_UNLIKELY(m_number == 0)) {
		qWarning("device invalid");
		return QString();
	}

	// check if we've already fetched the path
	if (!m_path.isEmpty())
		return m_path;

	// find all device nodes in the /dev/ directory
	QDirIterator it(m_basePath, QDir::System, QDirIterator::Subdirectories);
	while (it.hasNext()) {

		// get the full path of the device node
		const QString path = it.next();

		// get the stats of the file and confirm it's a device node and see
		// if it has a matching number
		struct stat statBuf;
		if (stat(path.toLatin1().constData(), &statBuf) != 0) {
			// qErrnoWarning(errno, "failed to stat '%s'", path.toLatin1().data());
			continue;
		}

		if (S_ISCHR(statBuf.st_mode) && (statBuf.st_rdev == m_number)) {
			// FIXME: not thread safe
			m_path = path;
			break;
		}
	}

	return m_path;
}

dev_t LinuxDevice::number() const
{
	return m_number;
}

uint LinuxDevice::major() const
{
	return ::major(m_number);
}

uint LinuxDevice::minor() const
{
	return ::minor(m_number);
}

QDebug operator<<(QDebug dbg, const LinuxDevice &device)
{
	if (!device.isValid()) {
		dbg << "LinuxDevice(invalid)";
	} else {

		// perform a reverse lookup to get the subsystem name
		QByteArray subSystem("unknown");
		QHash<QString, LinuxDevice::SubSystem>::const_iterator it = LinuxDevice::m_subSystemNames.begin();
		for (; it != LinuxDevice::m_subSystemNames.end(); ++it) {
			if (it.value() == device.subSystem()) {
				subSystem = it.key().toLatin1();
				break;
			}
		}

		// construct the string and write to the debug output
		char buf[128];
		snprintf(buf, sizeof(buf), "LinuxDevice(number=%02u:%02u, name='%s',"
		                           " subsystem=%s, path='%s')",
		         device.major(), device.minor(),
		         device.name().toLatin1().constData(),
		         subSystem.constData(),
		         device.path().toLatin1().constData());

		dbg << buf;
	}

	return dbg;
}
