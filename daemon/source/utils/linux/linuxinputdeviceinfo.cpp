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
//  linuxinputdeviceinfo.cpp
//  SkyBluetoothRcu
//

#include "linuxinputdeviceinfo.h"
#include "linuxinputdevice.h"
#include "linuxdevice.h"

#include <QDebug>
#include <QDirIterator>


#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <sys/stat.h>
#include <sys/ioctl.h>

#if defined(Q_OS_LINUX)
#  include <sys/sysmacros.h>
#  include <linux/types.h>
#  include <linux/input.h>
#endif


// -----------------------------------------------------------------------------
/*!
	\internal

	Returns \t true if the supplied device corresponds to an input event device
	node (i.e. /dev/input/eventX) by checking it's major and minor numbers.

 */
bool LinuxInputDeviceInfo::isInputEventDeviceNumber(dev_t deviceNum)
{
	#define DEV_INPUT_EVENT_MAJOR           13
	#define DEV_INPUT_EVENT_MINOR_FIRST     64
	#define DEV_INPUT_EVENT_MINOR_LAST      95

	const int majorNum = major(deviceNum);
	const int minorNum = minor(deviceNum);

	return (majorNum == DEV_INPUT_EVENT_MAJOR) &&
	       (minorNum >= DEV_INPUT_EVENT_MINOR_FIRST) &&
	       (minorNum <= DEV_INPUT_EVENT_MINOR_LAST);
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Executes the supplied function for all input device nodes in the system.

 */
void LinuxInputDeviceInfo::forEachInputDeviceImpl(const std::function<void(int, const QString&)> &func)
{
	// find all device nodes that match the path "/dev/input/event*"
	QDirIterator it("/dev/input", { "event*" }, QDir::System);
	while (it.hasNext()) {

		// get the full path of the device node
		const QString path = it.next();

		// try and open the file
		int fd = ::open(qPrintable(path), O_CLOEXEC | O_NONBLOCK | O_RDONLY);
		if (fd < 0) {
			qErrnoWarning(errno, "failed to open '%s'", qPrintable(path));
			continue;
		}

		// sanity check the device we opened is an input device type
		struct stat buf;
		if ((fstat(fd, &buf) != 0) || !S_ISCHR(buf.st_mode)) {
			qErrnoWarning(errno, "invalid device node @ '%s'", qPrintable(path));
			close(fd);
			continue;
		}

		// check the file is a input device node
		if (isInputEventDeviceNumber(buf.st_rdev)) {

			// call the supplied function
			func(fd, path);

		}

		// close the fd as we no longer need it
		if (::close(fd) != 0)
			qErrnoWarning(errno, "failed to close '%s'", qPrintable(path));
	}

}

// -----------------------------------------------------------------------------
/*!
	Returns a list of available input devices on the system.

 */
QList<LinuxInputDeviceInfo> LinuxInputDeviceInfo::availableDevices()
{
	QList<LinuxInputDeviceInfo> devices;

	// lambda called for every input device in the system, it receives the
	// device node fd and path and if it checks out the devices list is updated
	// with the info
	std::function<void(int, const QString&)> func =
		[&devices](int fd, const QString &path)
		{
			LinuxInputDeviceInfo info(fd, path);
			if (!info.isNull())
				devices.append(info);
		};

	// run the lambda over all devices
	forEachInputDevice(func);

	return devices;
}



LinuxInputDeviceInfo::LinuxInputDeviceInfo()
	: m_isNull(true)
	, m_id(-1)
	, m_pnpValid(false)
	, m_busType(InputDeviceInfo::Other)
	, m_vendorId(0)
	, m_productId(0)
	, m_version(0)
{
}

LinuxInputDeviceInfo::LinuxInputDeviceInfo(const LinuxDevice &device)
	: LinuxInputDeviceInfo()
{
	if (device.isValid()) {

		m_path = device.path();

		int fd = open(qPrintable(m_path), O_CLOEXEC | O_RDWR);
		if (fd < 0) {
			qErrnoWarning(errno, "failed to open input device file @ '%s'",
			              qPrintable(m_path));
			m_path.clear();
			return;
		}

		initFromFd(fd);

		if (close(fd) != 0)
			qErrnoWarning(errno, "failed to close input dev node");

		m_isNull = false;
	}
}

LinuxInputDeviceInfo::LinuxInputDeviceInfo(const LinuxInputDevice &device)
	: LinuxInputDeviceInfo()
{
	if (device.m_fd >= 0) {
		initFromFd(device.m_fd);
		m_isNull = false;
	}
}

LinuxInputDeviceInfo::LinuxInputDeviceInfo(const QString &name)
	: LinuxInputDeviceInfo()
{
#if defined(Q_OS_LINUX)

	// lambda called for every input device in the system, it receives the
	// device node fd and path, the lambda attempts to read the device name
	// and if have a match stores the fd and path internally
	std::function<void(int, const QString&)> func =
		[this, name](int fd, const QString &path)
		{
			//
			char buf[256];
			int ret = ioctl(fd, EVIOCGNAME(sizeof(buf)), buf);

			// the returned value is the number of bytes copied into the buffer,
			// however it may contain a null, so check for that and strip off if
			// present
			while ((ret > 0) && (buf[(ret - 1)] == '\0'))
				ret--;

			// check if the returned name matches, if so populate the class
			// details
			if (ret > 0) {
				const QLatin1String deviceName(buf, ret);
				if (deviceName == name) {
					initFromFd(fd);
					m_path = path;
					m_isNull = false;
				}
			}

		};

	// run the lambda over all devices
	forEachInputDevice(func);

#endif // defined(Q_OS_LINUX)
}

LinuxInputDeviceInfo::LinuxInputDeviceInfo(const LinuxInputDeviceInfo &other)
	: m_isNull(other.m_isNull)
	, m_id(other.m_id)
	, m_path(other.m_path)
	, m_name(other.m_name)
	, m_physicalLocation(other.m_physicalLocation)
	, m_uniqueIdentifier(other.m_uniqueIdentifier)
	, m_pnpValid(other.m_pnpValid)
	, m_busType(other.m_busType)
	, m_vendorId(other.m_vendorId)
	, m_productId(other.m_productId)
	, m_version(other.m_version)
{
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Internal constructor that populates the object based on the open file
	descriptor to an actual input device node.

 */
LinuxInputDeviceInfo::LinuxInputDeviceInfo(int fd, const QString &path)
	: LinuxInputDeviceInfo()
{
	initFromFd(fd);
	m_path = path;
	m_isNull = false;
	// qInfo() << path << *this;
}

// -----------------------------------------------------------------------------
/*!
	Destroys the LinuxInputDeviceInfo object. References to the values in the
	object become invalid.


 */
LinuxInputDeviceInfo::~LinuxInputDeviceInfo()
{
}

// -----------------------------------------------------------------------------
/*!
	Sets the LinuxInputDeviceInfo object to be equal to \a other.


 */
LinuxInputDeviceInfo & LinuxInputDeviceInfo::operator=(const LinuxInputDeviceInfo &other)
{
	if (this == &other)
		return *this;

	m_isNull = other.m_isNull;
	m_id = other.m_id;
	m_path = other.m_path;
	m_name = other.m_name;
	m_physicalLocation = other.m_physicalLocation;
	m_uniqueIdentifier = other.m_uniqueIdentifier;
	m_pnpValid = other.m_pnpValid;
	m_busType = other.m_busType;
	m_vendorId = other.m_vendorId;
	m_productId = other.m_productId;
	m_version = other.m_version;

	return *this;
}

// -----------------------------------------------------------------------------
/*!
	Returns \c true if \a other is equal to this object.


 */
bool LinuxInputDeviceInfo::isEqual(const LinuxInputDeviceInfo *other) const
{
	if (m_isNull || other->m_isNull)
		return false;

	// for now we don't use the dev node path as that can change, instead we
	// only use the details reported by the driver

	if (m_name.isEmpty() != other->m_name.isEmpty())
		return false;
	if (!m_name.isEmpty() && (m_name != other->m_name))
		return false;

	if (m_physicalLocation.isEmpty() != other->m_physicalLocation.isEmpty())
		return false;
	if (!m_physicalLocation.isEmpty() && (m_physicalLocation != other->m_physicalLocation))
		return false;

	if (m_uniqueIdentifier.isEmpty() != other->m_uniqueIdentifier.isEmpty())
		return false;
	if (!m_uniqueIdentifier.isEmpty() && (m_uniqueIdentifier != other->m_uniqueIdentifier))
		return false;

	if (m_pnpValid != other->m_pnpValid)
		return false;
	if (m_pnpValid && ((m_busType != other->m_busType) ||
	                   (m_vendorId != other->m_vendorId) ||
	                   (m_productId != other->m_productId) ||
	                   (m_version != other->m_version))) {
		return false;
	}

	return true;
}

bool LinuxInputDeviceInfo::operator==(const LinuxInputDeviceInfo &other) const
{
	return isEqual(&other);
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Populates the internal details of the class from the supplied open file
	descriptor to an actual linux input device node.

 */
void LinuxInputDeviceInfo::initFromFd(int fd)
{
#if defined(Q_OS_LINUX)

	// get the 'id' which is just the minor device number of the node
	struct stat stat;
	if (fstat(fd, &stat) != 0) {
		qErrnoWarning(errno, "failed to stat input event node");
		return;
	}

	m_id = minor(stat.st_rdev);


	// get the detail strings
	char buf[256];
	const struct {
		unsigned long request;
		QString &field;
	} ioctlOperations[] = {
		{ EVIOCGNAME(sizeof(buf)), m_name },
		{ EVIOCGPHYS(sizeof(buf)), m_physicalLocation },
		{ EVIOCGUNIQ(sizeof(buf)), m_uniqueIdentifier },
	};

	for (auto ioctlOperation : ioctlOperations) {

		int ret = ioctl(fd, ioctlOperation.request, buf);
		if (ret > 0) {

			// qDebug("input value : '%.*s'", ret, buf);

			// the returned value is the number of bytes copied into the buffer,
			// however it may contain a null, so check for that and strip off if
			// present
			while ((ret > 0) && (buf[(ret - 1)] == '\0'))
				ret--;

			ioctlOperation.field = QString::fromLatin1(buf, ret);
		}

	}

	// get the PnP device id
	struct input_id pnpId;
	bzero(&pnpId, sizeof(pnpId));

	int ret = ioctl(fd, EVIOCGID, &pnpId);
	if (ret < 0) {
		qErrnoWarning(errno, "failed to get input device id");

	} else {

		switch (pnpId.bustype) {
			case BUS_USB:       m_busType = InputDeviceInfo::USB;        break;
			case BUS_HIL:       m_busType = InputDeviceInfo::HIL;        break;
			case BUS_BLUETOOTH: m_busType = InputDeviceInfo::Bluetooth;  break;
			case BUS_VIRTUAL:   m_busType = InputDeviceInfo::Virtual;    break;
			default:            m_busType = InputDeviceInfo::Other;      break;
		}

		m_vendorId = pnpId.vendor;
		m_productId = pnpId.product;
		m_version = pnpId.version;

		m_pnpValid = true;
	}

#endif // defined(Q_OS_LINUX)
}


bool LinuxInputDeviceInfo::isNull() const
{
	return m_isNull;
}

int LinuxInputDeviceInfo::id() const
{
	return m_id;
}

QString	LinuxInputDeviceInfo::path() const
{
	return m_path;
}

QString	LinuxInputDeviceInfo::name() const
{
	return m_name;
}

QString	LinuxInputDeviceInfo::physicalLocation() const
{
	return m_physicalLocation;
}

QString	LinuxInputDeviceInfo::uniqueIdentifier() const
{
	return m_uniqueIdentifier;
}

bool LinuxInputDeviceInfo::hasBusType() const
{
	return m_pnpValid;
}

InputDeviceInfo::BusType LinuxInputDeviceInfo::busType() const
{
	return m_busType;
}

bool LinuxInputDeviceInfo::hasProductIdentifier() const
{
	return m_pnpValid;
}

quint16	LinuxInputDeviceInfo::productIdentifier() const
{
	return m_productId;
}

bool LinuxInputDeviceInfo::hasVendorIdentifier() const
{
	return m_pnpValid;
}

quint16	LinuxInputDeviceInfo::vendorIdentifier() const
{
	return m_vendorId;
}

bool LinuxInputDeviceInfo::hasVersion() const
{
	return m_pnpValid;
}

quint16	LinuxInputDeviceInfo::version() const
{
	return m_version;
}

bool LinuxInputDeviceInfo::matches(const BleAddress &address) const
{
	return (m_physicalLocation.compare(address.toString(), Qt::CaseInsensitive) == 0);
}

QDebug operator<<(QDebug dbg, const LinuxInputDeviceInfo &info)
{
	dbg << "name:" << info.m_name
	    << "phys:" << info.m_physicalLocation
	    << "uniq:" << info.m_uniqueIdentifier;

	if (info.m_pnpValid) {
		dbg << "pnpid: [ "
		    << "bus" << info.m_busType
		    << "vendor" << hex << info.m_vendorId
		    << "product" << hex << info.m_productId
		    << "version" << hex << info.m_version
		    << "]";
	}

	return dbg;
}

