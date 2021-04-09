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
//  hidrawdevicemanager.cpp
//  SkyBluetoothRcu
//

#include "hidrawdevicemanager_p.h"
#include "hidrawdevice_p.h"
#include "linuxdevicenotifier.h"
#include "logging.h"

#include <QDirIterator>

#include <functional>

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>



// -----------------------------------------------------------------------------
/*!
	\class HidRawDeviceManager
	\brief The HidRawDeviceManager is a helper class for opening and listening
	for changes to hidraw devices.

	There is not a lot to this class, it's just used to wrap up some useful
	functions for opening a hidraw device using the HID physical address and
	also for listening for signals notifying of hidraw devices that have been
	added or removed from the system.

 */



QSharedPointer<HidRawDeviceManager> HidRawDeviceManager::create(const QSharedPointer<const LinuxDeviceNotifier> &devNotifier,
                                                                QObject *parent)
{
	return QSharedPointer<HidRawDeviceManagerImpl>::create(devNotifier, parent);
}


// -----------------------------------------------------------------------------
/*!
	Constructs HidRawDeviceManager object.

	This creates a udev socket notifier so ideally you should only create one
	of these objects to avoid overuse of udev / uevent sockets.

 */
HidRawDeviceManagerImpl::HidRawDeviceManagerImpl(const QSharedPointer<const LinuxDeviceNotifier> &devNotifier,
                                                 QObject *parent)
	: HidRawDeviceManager(parent)
	, m_deviceNotifier(devNotifier)
	, m_syncTimerInterval(5000)
{
	// setup the timer to fire periodically to re-scan for hidraw device nodes
	m_syncTimer.setSingleShot(false);
	m_syncTimer.setInterval(m_syncTimerInterval);

	// attach to the timer's timeout event
	QObject::connect(&m_syncTimer, &QTimer::timeout,
	                 this, &HidRawDeviceManagerImpl::syncHidRawDeviceMap);

	// and start the timer
	m_syncTimer.start();


	// connect to the device added and the device removed signals
	QObject::connect(m_deviceNotifier.data(), &LinuxDeviceNotifier::deviceAdded,
	                 this, &HidRawDeviceManagerImpl::onDeviceAdded);
	QObject::connect(m_deviceNotifier.data(), &LinuxDeviceNotifier::deviceRemoved,
	                 this, &HidRawDeviceManagerImpl::onDeviceRemoved);


	// perform the initial scan of device nodes
	syncHidRawDeviceMap();
}

HidRawDeviceManagerImpl::~HidRawDeviceManagerImpl()
{
}

// -----------------------------------------------------------------------------
/*!
	\fn QSet<QByteArray> HidRawDeviceManager::physicalAddresses(bool convertToLowerCase) const

	Returns a set of all hidraw device's physical addresses that are currently
	available.

	If \a convertToLowerCase is \c true (the default) then the returned physical
	address byte arrays will contain the value read from the driver converted to
	all lower case.

 */
QSet<QByteArray> HidRawDeviceManagerImpl::physicalAddresses(bool convertToLowerCase) const
{
	QSet<QByteArray> phyAddresses;

	// find all device nodes that match the path "/dev/hidraw*"
	QDirIterator it(m_deviceNotifier->devPath(), { "hidraw*" }, QDir::System);
	while (it.hasNext()) {

		// get the full path of the device node
		const QString path = it.next();

		// try and open the file
		int fd = ::open(path.toLatin1().data(), O_CLOEXEC | O_NONBLOCK);
		if (fd < 0) {
			qErrnoWarning(errno, "failed to open '%s'", path.toLatin1().constData());
			continue;
		}

		// attempt to get the physical address associated with this device
		QByteArray address;
		if (HidRawDeviceImpl::getPhysicalAddress(fd, &address)) {

			if (convertToLowerCase)
				phyAddresses.insert(address.toLower());
			else
				phyAddresses.insert(address);
		}

		// close the fd as we no longer need it
		if (::close(fd) != 0)
			qErrnoWarning(errno, "failed to close '%s'", path.toLatin1().constData());
	}

	return phyAddresses;
}

// -----------------------------------------------------------------------------
/*!
	Tries to open a hidraw device that has the given \a physicalAddress. For us
	the physical address should be the BDADDR of the bluetooth device.  The
	physical addresses are assumed to be strings and a caseless string compare
	is used to determine if the device matches.

	If no device is found then an empty shared pointer is returned.
 */
QSharedPointer<HidRawDevice> HidRawDeviceManagerImpl::open(const QByteArray& physicalAddress,
                                                           HidRawDevice::OpenMode mode) const
{
	const QString requestedPhyAddress = physicalAddress.toLower();

	qInfo() << "trying to open hidraw device with physical address"
	        << requestedPhyAddress;

	// we always open in non-blocking mode
	int openFlags = O_CLOEXEC | O_NONBLOCK;
	switch (mode) {
		case HidRawDevice::ReadOnly:    openFlags |= O_RDONLY;    break;
		case HidRawDevice::WriteOnly:   openFlags |= O_WRONLY;    break;
		case HidRawDevice::ReadWrite:   openFlags |= O_RDWR;      break;
	}

	QSharedPointer<HidRawDevice> hidrawDevice;

	// find all device nodes that match the path "/dev/hidraw*"
	QDirIterator it(m_deviceNotifier->devPath(), { "hidraw*" }, QDir::System);
	while (it.hasNext()) {

		// get the full path of the device node
		const QString path = it.next();

		// try and open the file
		int fd = ::open(path.toLatin1().data(), openFlags);
		if (fd < 0) {
			qErrnoWarning(errno, "failed to open '%s'", path.toLatin1().data());
			continue;
		}

		// attempt to get the physical address associated with this device
		QByteArray address;
		if (HidRawDeviceImpl::getPhysicalAddress(fd, &address)) {

			const QByteArray actualPhyAddress = address.toLower();

			// compare the physical addresses
			if (actualPhyAddress == requestedPhyAddress) {
				qInfo() << "found matching hidraw device @" << path;
				hidrawDevice = QSharedPointer<HidRawDeviceImpl>::create(fd);
			}

		}

		// close the fd as we no longer need it
		if (::close(fd) != 0)
			qErrnoWarning(errno, "failed to close '%s'", path.toLatin1().data());

		// check if we managed to find a device
		if (hidrawDevice && hidrawDevice->isValid())
			break;
	}

	return hidrawDevice;
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Synchronises the hidraw devices actually in /dev/ with our internal map.
	This is called after we get a notification from udev that a device has been
	added or removed.

 */
void HidRawDeviceManagerImpl::syncHidRawDeviceMap()
{
	// map to store the current list of hidraw devices
	QMap<QByteArray, QString> hidrawDeviceMap;

	// find all device nodes that match the path "/dev/hidraw*"
	QDirIterator it(m_deviceNotifier->devPath(), { "hidraw*" }, QDir::System);
	while (it.hasNext()) {

		// get the full path of the device node
		const QString path = it.next();

		// try and open the file
		int fd = ::open(path.toLatin1().constData(), O_CLOEXEC | O_NONBLOCK);
		if (fd < 0) {
			qErrnoWarning(errno, "failed to open '%s'", path.toLatin1().data());
			continue;
		}

		// attempt to get the physical address associated with this device
		QByteArray address;
		if (HidRawDeviceImpl::getPhysicalAddress(fd, &address)) {

			const QByteArray actualPhyAddress = address.toLower();
			// qDebug("hidraw device @ '%s' has physical address '%s'",
			//        path.toLatin1().constData(), actualPhyAddress.data());

			hidrawDeviceMap[actualPhyAddress] = path;
		}

		// close the fd as we no longer need it
		if (::close(fd) != 0)
			qErrnoWarning(errno, "failed to close '%s'", path.toLatin1().data());
	}

	// create a list to store the signals to send, we can't call the signals
	// directly while processing the maps below as the slots may callback into
	// this class and use the map, thereby invalidating the iterators
	QList< std::function<void()> > signalsToSend;


	// now compare the current list with the map stored in the class
	QMap<QByteArray, QString>::iterator itor = m_hidrawDeviceMap.begin();
	while (itor != m_hidrawDeviceMap.end()) {

		const QByteArray &existingPhysAddress = itor.key();
		const QString &existingDevPath = itor.value();

		if (!hidrawDeviceMap.contains(existingPhysAddress) ||
			(hidrawDeviceMap[existingPhysAddress] != existingDevPath)) {

			qMilestone() << "hidraw device @" << existingDevPath
			             << "with physical address" << existingPhysAddress
			             << "has been removed";

			// queue the signal to emit
			signalsToSend.append(std::bind(&HidRawDeviceManager::deviceRemoved,
			                               this, QByteArray(existingPhysAddress)));

			// we no longer have this device, or it's moved to a different path,
			// so remove from the map
			itor = m_hidrawDeviceMap.erase(itor);

		} else {

			// we still have this device so remove from the new list
			hidrawDeviceMap.remove(existingPhysAddress);

			// and move on
			++itor;

		}
	}

	// any device left in the hidrawDeviceMap must be a new device so add to
	// our internal map and generate a signal
	itor = hidrawDeviceMap.begin();
	while (itor != hidrawDeviceMap.end()) {

		const QByteArray &newPhysAddress = itor.key();
		const QString &newDevPath = itor.value();

		qMilestone() << "hidraw device @" << newDevPath
		             << "with physical address" << newPhysAddress
		             << "has been added";

		m_hidrawDeviceMap[newPhysAddress] = newDevPath;

		// queue the added signal to emit
		signalsToSend.append(std::bind(&HidRawDeviceManager::deviceAdded,
		                               this, QByteArray(newPhysAddress)));

		++itor;
	}


	// send any / all queue signals
	for (const std::function<void()> &signal : signalsToSend)
		emit signal();

}

// -----------------------------------------------------------------------------
/*!
	Called when the device notifier advises us that a new hidraw device has been
	added to the system.

	We filter the event so we only emit \a deviceAdded() signals if the device
	added is really a hidraw device.  If it is then we try and open the device
	to get it's physical address, we then emit the \a deviceAdded signal with
	the physical address retrieved.

 */
void HidRawDeviceManagerImpl::onDeviceAdded(const LinuxDevice &device)
{
	if (device.subSystem() != LinuxDevice::HidRawSubSystem)
		return;

	qDebug() << "device added :" << device;

	syncHidRawDeviceMap();
}

// -----------------------------------------------------------------------------
/*!
	Called when the device notifier advises us that a new hidraw device has been
	added to the system.

	We filter the event so we only emit \a deviceAdded() signals if the device
	added is really a hidraw device.  If it is then we try and open the device
	to get it's physical address, we then emit the \a deviceAdded signal with
	the physical address retrieved.

 */
void HidRawDeviceManagerImpl::onDeviceRemoved(const LinuxDevice &device)
{
	if (device.subSystem() != LinuxDevice::HidRawSubSystem)
		return;

	qDebug() << "device removed :" << device;

	syncHidRawDeviceMap();
}


