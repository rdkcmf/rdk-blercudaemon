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
//  hidrawdevice.cpp
//  SkyBluetoothRcu
//

#include "hidrawdevice.h"
#include "hidrawdevice_p.h"
#include "logging.h"

#include <QSocketNotifier>

#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/types.h>

#if defined(__linux__)
#  include <linux/types.h>
#  include <linux/input.h>
#  include <linux/hidraw.h>
#else
   struct __attribute__((packed)) hidraw_devinfo {
       uint32_t bustype;
       int16_t vendor;
       int16_t product;
   };
#  define HIDIOCGRAWINFO      _IOR('H', 0x03, struct hidraw_devinfo)
#  define HIDIOCGRAWPHYS(len) _IOC(IOC_OUT, 'H', 0x05, len)
#  define BUS_USB             0x03
#  define BUS_HIL             0x04
#  define BUS_BLUETOOTH       0x05
#  define BUS_VIRTUAL         0x06
#endif


// -----------------------------------------------------------------------------
/*!
	\class HidRawDevice
	\brief Wrapper around a hidraw device node



 */


// -----------------------------------------------------------------------------
/*!
	Creates a HidRawDevice object attached to the hidraw device node given
	the file descriptor of an open device in the \a hidrawDevFd argument.

	This constructor dup's the supplied descriptor internally, so it is safe
	to close \a hidrawDevFd after creating the device.

	Use the HidRawDevice::isValid function to check that the file was opened
	correctly.

 */
HidRawDeviceImpl::HidRawDeviceImpl(int hidrawDevFd, QObject *parent)
	: HidRawDevice(parent)
	, m_hidrawDevFd(-1)
	, m_reportFilter(0)
	, m_minorNumber(-1)
{
	// check the file is a device node and it's major number matches the
	// hidraw type
	struct stat stat;
	if (fstat(hidrawDevFd, &stat) != 0) {
		qErrnoWarning(errno, "failed to fstat the file desciptor");
		return;
	}

	if (!S_ISCHR(stat.st_mode)) {
		qWarning("file descriptor doesn't correspond to a devnode");
		return;
	}

	// dup the hidraw device descriptor
	m_hidrawDevFd = fcntl(hidrawDevFd, F_DUPFD_CLOEXEC, 3);
	if (m_hidrawDevFd < 0) {
		qErrnoWarning(errno, "failed to dup hidraw fd");
		return;
	}

	// store the minor number
	m_minorNumber = ::minor(stat.st_rdev);

	// put in non-blocking mode
	int flags = fcntl(m_hidrawDevFd, F_GETFL);
	if (flags < 0)
		flags = 0;
	if (fcntl(m_hidrawDevFd, F_SETFL, flags | O_NONBLOCK) < 0)
		qErrnoWarning(errno, "failed to set non-blocking mode on the fd");

	// finish off the init by getting the device details
	init();
}

// -----------------------------------------------------------------------------
/*!
	Creates a HidRawDevice object attached to the hidraw device node given
	in the \a hidrawDevPath argument.

	Use the HidRawDevice::isValid function to check that the file was opened
	correctly.

 */
HidRawDeviceImpl::HidRawDeviceImpl(const QString &hidrawDevPath,
                                   OpenMode openMode, QObject *parent)
	: HidRawDevice(parent)
	, m_hidrawDevFd(-1)
	, m_reportFilter(0)
	, m_minorNumber(-1)
{
	// convert the path to ascii
	char path[PATH_MAX];
	strncpy(path, hidrawDevPath.toLatin1().constData(), PATH_MAX);
	path[(PATH_MAX - 1)] = '\0';

	// we always open in non-blocking mode
	int openFlags = O_CLOEXEC | O_NONBLOCK;
	switch (openMode) {
		case HidRawDevice::ReadOnly:    openFlags |= O_RDONLY;    break;
		case HidRawDevice::WriteOnly:   openFlags |= O_WRONLY;    break;
		case HidRawDevice::ReadWrite:   openFlags |= O_RDWR;      break;
	}

	// open the device node
	int devFd = open(path, openFlags);
	if (devFd < 0) {
		qErrnoWarning(errno, "failed to open hidraw device at '%s'", path);
		return;
	}

	// check the file is a device node and it's major number matches the
	// hidraw type
	struct stat stat;
	if (fstat(devFd, &stat) != 0) {
		qErrnoWarning(errno, "failed to fstat the file at '%s'", path);
		close(devFd);
		return;
	}

	if (!S_ISCHR(stat.st_mode)) {
		qWarning("file at '%s' is not a devnode", path);
		close(devFd);
		return;
	}

	m_hidrawDevFd = devFd;

	// store the minor number
	m_minorNumber = ::minor(stat.st_rdev);

	// finish off the init by getting the device details
	init();
}

// -----------------------------------------------------------------------------
/*!
	Releases this object's resources.
 */
HidRawDeviceImpl::~HidRawDeviceImpl()
{
	// closes the fd and remove the QSocketNotifier's
	term();
}

// -----------------------------------------------------------------------------
/*!
	Gets the info on the hidraw device info and if ok attaches two listeners
	for changes on the device.

 */
void HidRawDeviceImpl::init()
{
	// now get the details of the hidraw device
	if (!getInfo(m_hidrawDevFd, &m_busType, &m_vendor, &m_product) ||
	    !getPhysicalAddress(m_hidrawDevFd, &m_physicalAddress)) {

		if (close(m_hidrawDevFd) != 0)
			qErrnoWarning(errno, "failed to close hidraw file descriptor");

		m_hidrawDevFd = -1;
		m_minorNumber = -1;
		return;
	}


	// setup a notifier to listen for when there is data to read
	m_readNotifier = QSharedPointer<QSocketNotifier>(
				new QSocketNotifier(m_hidrawDevFd, QSocketNotifier::Read),
				&QObject::deleteLater);

	QObject::connect(m_readNotifier.data(), &QSocketNotifier::activated,
	                 this, &HidRawDeviceImpl::onReadActivated);

	// and another for an exception (TODO: test what this actually is)
	m_exceptionNotifier = QSharedPointer<QSocketNotifier>(
				new QSocketNotifier(m_hidrawDevFd, QSocketNotifier::Exception),
				&QObject::deleteLater);

	QObject::connect(m_exceptionNotifier.data(), &QSocketNotifier::activated,
	                 this, &HidRawDeviceImpl::onExceptionActivated);

	qInfo("created hidraw device object with fd %d", m_hidrawDevFd);
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Removes and destroys the \a QSocktNotifier's if they still attached and then
	closes the file descriptor
 */
void HidRawDeviceImpl::term()
{
	// disable the notifiers if still valid
	if (m_readNotifier && m_readNotifier->isEnabled())
		m_readNotifier->setEnabled(false);
	if (m_exceptionNotifier && m_exceptionNotifier->isEnabled())
		m_exceptionNotifier->setEnabled(false);

	// destroy the notifiers (actually they have QObject::deleteLater as their
	// deleter so they won't be destroyed till the next cycle of the event loop)
	m_readNotifier.clear();
	m_exceptionNotifier.clear();

	// close the hidraw device
	if ((m_hidrawDevFd >= 0) && (close(m_hidrawDevFd) != 0))
		qErrnoWarning(errno, "failed to close hidraw file descriptor");

	m_minorNumber = -1;
	m_hidrawDevFd = -1;
}

bool HidRawDeviceImpl::isValid() const
{
	return (m_hidrawDevFd >= 0);
}

// -----------------------------------------------------------------------------
/*!
	\fn uint HidRawDevice::minorNumber() const

	Returns the minor number of the hidraw device node.  If the device is not
	valid the return value is undefined.
 */
int HidRawDeviceImpl::minorNumber() const
{
	return m_minorNumber;
}

// -----------------------------------------------------------------------------
/*!
	\fn HidRawDevice::BusType HidRawDevice::busType() const

	Returns the underlying bus type that is providing the HID events to this
	device.
 */
HidRawDevice::BusType HidRawDeviceImpl::busType() const
{
	return m_busType;
}

// -----------------------------------------------------------------------------
/*!
	\fn quint16 HidRawDevice::vendor() const

	Returns the 16-bit vendor id set for the hidraw device.

	\see product(), pnpId()
 */
quint16 HidRawDeviceImpl::vendor() const
{
	return m_vendor;
}

// -----------------------------------------------------------------------------
/*!
	\fn quint16 HidRawDevice::product() const

	Returns the 16-bit product id set for the hidraw device.

	\see vendor(), pnpId()
 */
quint16 HidRawDeviceImpl::product() const
{
	return m_product;
}

// -----------------------------------------------------------------------------
/*!
	\fn PnPId HidRawDevice::pnpId() const

	Returns the PnP info for the hidraw device.
 
	\see vendor(), product()
 */
PnPId HidRawDeviceImpl::pnpId() const
{
	return PnPId(m_vendor, m_product);
}

// -----------------------------------------------------------------------------
/*!
	\fn QByteArray HidRawDevice::physicalAddress() const

	Returns the physical address stored in the hidraw device.  By convention the
	bluetooth daemon sets this to be the bdaddr of the connected device as
	a string in the usual colon delimited format, i.e. "12:34:56:78:9a:bc"

 */
QByteArray HidRawDeviceImpl::physicalAddress() const
{
	return m_physicalAddress;
}

// -----------------------------------------------------------------------------
/*!
	\internal

	This is typically called if a read/write on the hidraw file descriptor
	returns \c ENODEV.

 */
void HidRawDeviceImpl::deviceRemoved()
{
	qInfo() << "hidraw device was removed";

	// closes the fd and remove the QSocketNotifier's
	term();

	// emit a signal to let anyone know that we've gone bye bye
	emit closed();
}

// -----------------------------------------------------------------------------
/*!
	Sends a report with \a data on report id equal to \a number.

	Returns \c true if the report was successifully written to the hid raw
	device.
 */
bool HidRawDeviceImpl::write(uint number, const quint8* data, int dataLen)
{
	if (Q_UNLIKELY(m_hidrawDevFd < 0)) {
		qWarning() << "invalid hidraw file descriptor";
		return false;
	}

	quint8 buf[32];

	if (Q_UNLIKELY(dataLen > int(sizeof(buf) - 1))) {
		qWarning() << "report to large";
		return false;
	}

	buf[0] = static_cast<quint8>(number);
	memcpy(buf + 1, data, dataLen);

	qDebug() << "writting data" << arrayToHex(buf, dataLen + 1);

	ssize_t written = TEMP_FAILURE_RETRY(::write(m_hidrawDevFd, buf, (dataLen + 1)));
	if (written < 0) {
		qErrnoWarning(errno, "failed to write to hidraw device");

		// check if this error indicates that the device has disappeared from
		// beneath us
		if ((errno == ENODEV) || (errno == ENXIO) || (errno == EIO))
			deviceRemoved();

		return false;

	} else if (written != (1 + dataLen)) {
		qWarning("failed to write complete report (size %d, actually written %zd)",
		         (1 + dataLen), written);
		return false;
	}

	return true;
}

bool HidRawDeviceImpl::write(uint number, const QVector<quint8> &data)
{
	return this->write(number, data.data(), data.size());
}

bool HidRawDeviceImpl::write(uint number, const QByteArray &data)
{
	return this->write(number, reinterpret_cast<const quint8*>(data.data()),
	                   data.size());
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Called by the SocketNotifer when we can read from the hidraw fd.

 */
void HidRawDeviceImpl::onReadActivated(int hidrawDevFd)
{
	// sanity check
	if (Q_UNLIKELY(hidrawDevFd != m_hidrawDevFd)) {
		qCritical("odd hidraw fds don't match (actual %d, expected %d)",
		          hidrawDevFd, m_hidrawDevFd);
		return;
	}

	// attempt to read a single report
	quint8 buf[32];
	ssize_t rd = TEMP_FAILURE_RETRY(::read(m_hidrawDevFd, buf, sizeof(buf)));
	if (rd < 0) {

		// ignore EAGAIN as we opened in non-blocking mode
		if (errno == EAGAIN)
			return;

		// log the error and then check if the device has disappeared
		if ((errno == ENODEV) || (errno == ENXIO) || (errno == EIO))
			deviceRemoved();
		else
			qErrnoWarning(errno, "failed to read from hidraw device");

	} else if (rd < 1) {
		qWarning() << "failed to read the report id";

	} else {

		// the first byte is always the report number
		const quint8 reportId = buf[0];

		// qDebug() << "read" << arrayToHex(buf + 1, rd - 1)
		//          << "from report id" << reportId;

		// only emit the report signal if the given report id is enabled
		if ((reportId < 32) && (m_reportFilter & (1UL << reportId))) {
			QByteArray reportData(reinterpret_cast<const char*>(buf + 1), int(rd - 1));
			emit report(reportId, reportData);
		}
	}
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Called by the SocketNotifer when there is an 'exception', currently this
	doesn't seem to ever fire.

 */
void HidRawDeviceImpl::onExceptionActivated(int hidrawDevFd)
{
	qInfo() << "exception notifier";

	// sanity check
	if (Q_UNLIKELY(hidrawDevFd != m_hidrawDevFd)) {
		qCritical("odd hidraw fds don't match (actual %d, expected %d)",
		          hidrawDevFd, m_hidrawDevFd);
		return;
	}

	// try an empty read to see if the device is still there
	quint8 ignore;
	ssize_t rd = TEMP_FAILURE_RETRY(::read(m_hidrawDevFd, &ignore, 0));
	if (rd < 0) {
		qErrnoWarning(errno, "empty read failed");
		deviceRemoved();
	}
}

// -----------------------------------------------------------------------------
/*!
	Enables the \a HidRawDevice::report() signal for reports with the given
	\a id.  Only report id's from 0 to 31 can be enabled / disabled.

 */
void HidRawDeviceImpl::enableReport(uint id)
{
	if (Q_UNLIKELY(id > 31)) {
		qWarning("invalid report id %u", id);
		return;
	}

	m_reportFilter |= (0x1UL << id);
}

// -----------------------------------------------------------------------------
/*!
	Disables the \a HidRawDevice::report() signal for reports with the given
	\a id.  Only report id's from 0 to 31 can be enabled / disabled.

 */
void HidRawDeviceImpl::disableReport(uint id)
{
	if (Q_UNLIKELY(id > 31)) {
		qWarning("invalid report id %u", id);
		return;
	}

	m_reportFilter &= ~(0x1UL << id);
}

// -----------------------------------------------------------------------------
/*!
	Checks if the report with \a id is enabled for emitting the
	\a HidRawDevice::report() signal.

 */
bool HidRawDeviceImpl::reportEnabled(uint id) const
{
	if (Q_UNLIKELY(id > 31)) {
		qWarning("invalid report id %u", id);
		return false;
	}

	return (m_reportFilter & (0x1UL << id)) ? true : false;
}

// -----------------------------------------------------------------------------
/*!
	Static function that return info on a hidraw device given an open file
	descriptor to it.

 */
bool HidRawDeviceImpl::getInfo(int hidDevFd, BusType *busType, quint16 *vendor,
                               quint16 *product)
{
	// sanity check
	if (Q_UNLIKELY(hidDevFd < 0)) {
		qWarning("invalid file descriptor");
		return false;
	}

	struct hidraw_devinfo info;
	bzero(&info, sizeof(struct hidraw_devinfo));

	// query the data straight from the driver
	if (ioctl(hidDevFd, HIDIOCGRAWINFO, &info) < 0) {
		qErrnoWarning(errno, "failed to get info from hidraw device");
		return false;
	}

	// convert the bus type
	if (busType != nullptr) {
		switch (info.bustype) {
			case BUS_USB:
				*busType = HidRawDevice::USB;
				break;
			case BUS_HIL:
				*busType = HidRawDevice::HIL;
				break;
			case BUS_BLUETOOTH:
				*busType = HidRawDevice::Bluetooth;
				break;
			case BUS_VIRTUAL:
				*busType = HidRawDevice::Virtual;
				break;
			default:
				*busType = HidRawDevice::Other;
				break;
		}
	}

	// and the vendor and product ids
	if (vendor != nullptr)
		*vendor = info.vendor;

	if (product != nullptr)
		*product = info.product;

	return true;
}

// -----------------------------------------------------------------------------
/**
 *	@brief
 *
 *
 */
bool HidRawDeviceImpl::getPhysicalAddress(int hidDevFd, QByteArray *phyAddress)
{
	// sanity check
	if (Q_UNLIKELY(hidDevFd < 0)) {
		qWarning("invalid file descriptor");
		return false;
	}

	QByteArray buffer(256, 0x00);

	// query the data straight from the driver
	int ret = ioctl(hidDevFd, HIDIOCGRAWPHYS(buffer.size()), buffer.data());
	if (ret < 0) {
		qErrnoWarning(errno, "failed to get physical address from hidraw device");
		return false;
	}

	// the returned value is the number of bytes copied into the buffer, however
	// it may contain a null, so check for that and strip off if present
	while ((ret > 0) && (buffer[(ret - 1)] == '\0'))
		ret--;

	buffer.resize(ret);

	// finally return the value
	if (phyAddress != nullptr)
		*phyAddress = buffer;

	return true;
}

// -----------------------------------------------------------------------------
/*!
	Debugging function used to simply return the information about the hidraw
	device.

 */
QDebug operator<<(QDebug dbg, const HidRawDevice &device)
{
	if (!device.isValid()) {
		dbg << "HidRawDevice(invalid)";
	} else {
		char buf[128];
		snprintf(buf, sizeof(buf), "HidRawDevice(%s, 0x%04x:0x%04x, '%s')",
		         (device.busType() == HidRawDevice::Bluetooth) ? "bluetooth" :
		         (device.busType() == HidRawDevice::HIL)       ? "HIL" :
		         (device.busType() == HidRawDevice::USB)       ? "USB" :
		         (device.busType() == HidRawDevice::Virtual)   ? "virtual" :
		         (device.busType() == HidRawDevice::Other)     ? "other" : "unknown",
		         device.vendor(), device.product(),
		         device.physicalAddress().data());
		dbg << buf;
	}

	return dbg;
}


