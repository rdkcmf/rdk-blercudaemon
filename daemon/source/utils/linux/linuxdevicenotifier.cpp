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
//  linuxdevicenotifier.cpp
//  SkyBluetoothRcu
//

#include "linuxdevicenotifier_p.h"
#include "containerhelpers.h"
#include "logging.h"

#include <QSocketNotifier>
#include <QMutex>

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/socket.h>

#if defined(HAVE_LIBUDEV)
#  include <linux/netlink.h>
#  if defined(SKY_FUSION_PLATFORM)
#    include <udev/libudev.h>
#  else
#    include <libudev.h>
#  endif
#endif


// -----------------------------------------------------------------------------
/*!
	\class LinuxDeviceNotifier
	\brief Wrapper around the libudev monitor functions.



 */



static void registerLinuxDeviceType()
{
	static int typeId = -1;
	static QMutex typeLock;

	QMutexLocker locker(&typeLock);

	if (Q_UNLIKELY(typeId == -1)) {
		typeId = qRegisterMetaType<LinuxDevice>();
	}
}


const QHash<LinuxDevice::SubSystem, QString> LinuxDeviceNotifierImpl::m_subSystemNames = {
	{   LinuxDevice::InputSubSystem,    QStringLiteral("input"),    },
	{   LinuxDevice::HidRawSubSystem,   QStringLiteral("hidraw"),   },

};


// -----------------------------------------------------------------------------
/*!
	Creates a new DeviceNotifier object.

	This creates a udev socket notifier so ideally you should only create one
	of these objects to avoid overuse of udev / uevent sockets.

	\a type specifies the type of notifier, \l{LinuxDeviceNotifier::UDev} will
	create a socket that listens for events from the \c udevd daemon, if
	\l{LinuxDeviceNotifier::Kernel} you'll get events straight from the kernel.

	\a netNsFd defines the network namespace to create the udev netlink socket
	in, if -1 then the socket is created in the current network namespace of
	the process.  You'd typically use the netNsFd if running inside a
	container, where you'd provide the host's network namespace and therefore
	you'd be able to get all udev events.

 */
QSharedPointer<LinuxDeviceNotifier> LinuxDeviceNotifier::create(Type type,
                                                                int netNsFd,
                                                                QObject *parent)
{
	return QSharedPointer<LinuxDeviceNotifierImpl>::create(type, netNsFd, parent);
}

// -----------------------------------------------------------------------------
/*!
	Constructs DeviceNotifier object.

	This creates a udev socket notifier so ideally you should only create one
	of these objects to avoid overuse of udev / uevent sockets.

 */
LinuxDeviceNotifierImpl::LinuxDeviceNotifierImpl(Type type, int netNsFd,
                                                 QObject *parent)
	: LinuxDeviceNotifier(parent)
	, m_type(type)
	, m_udevHandle(nullptr)
	, m_udevMonitor(nullptr)
	, m_udevEnumerate(nullptr)
	, m_udevMonitorFd(-1)
{

	// registers the LinuxDevice object type with Qt's meta type system
	registerLinuxDeviceType();

#if !defined(HAVE_LIBUDEV)

	Q_UNUSED(netNsFd);

#else // defined(HAVE_LIBUDEV)

	// create a handle to the library
	m_udevHandle = udev_new();
	if (!m_udevHandle) {
		qCritical("failed to new create udev handle");
		return;
	}

	// create a udev enumeration object for the listDevices method
	m_udevEnumerate = udev_enumerate_new(m_udevHandle);
	if (!m_udevEnumerate) {
		qCritical("failed to create udev enumerate object");
		return;
	}

	// create a udev monitor to detect new hidraw devices. Nb the second argument
	// should be either 'udev' or 'kernel' and corresponds to who we are
	// listening to, if listening to the 'kernel' we get the raw events at the
	// same time the udev daemon does, if listening to 'udev' we get the events
	// once udev has processed them.
	m_udevMonitor = udev_monitor_new_from_netlink(m_udevHandle,
	                                              (type == UDev) ? "udev"
	                                                             : "kernel");
	if (!m_udevMonitor) {
		qCritical("failed to create udev monitor");
		return;
	}

	// get the fd for the socket notifier
	m_udevMonitorFd = udev_monitor_get_fd(m_udevMonitor);
	if (m_udevMonitorFd < 0) {
		qCritical("failed to get the fd of the udev monitor (%d)", m_udevMonitorFd);
		return;
	}

	// hack alert: if a network namespace was supplied we need to open the
	// netlink socket within that, however in our version of libudev there
	// is no api to do that (newer versions have a udev_monitor_new_from_netlink_fd()
	// function). However we can cheat by replacing the monitor fd with our
	// own custom socket using dup3, this works because the socket is not
	// actually bound and used until udev_monitor_enable_receiving() is called.
	if (netNsFd >= 0) {

		int udevSock = createSocketInNs(netNsFd, PF_NETLINK,
		                                (SOCK_DGRAM | SOCK_CLOEXEC),
		                                NETLINK_KOBJECT_UEVENT);
		if (udevSock < 0) {
			qErrnoWarning(errno, "failed to create udev netlink socket");
			m_udevMonitorFd = -1;
			return;
		}

		m_udevMonitorFd = TEMP_FAILURE_RETRY(dup3(udevSock, m_udevMonitorFd, O_CLOEXEC));
		if (m_udevMonitorFd < 0) {
			qErrnoWarning(errno, "failed to dup3 the fd of the udev monitor");
			close(udevSock);
			return;
		}

		if (close(udevSock) != 0)
			qErrnoWarning(errno, "failed to close new udev socket");
	}

	// set a decent sized receive buffer
	udev_monitor_set_receive_buffer_size(m_udevMonitor, 1*1024*1024);

	// create a notifier on the monitor fd
	m_udevMonitorNotifier = new QSocketNotifier(m_udevMonitorFd, QSocketNotifier::Read);

	// and then connect to it's activate signal
	QObject::connect(m_udevMonitorNotifier, &QSocketNotifier::activated,
	                 this, &LinuxDeviceNotifierImpl::onMonitorActivated);


	// enable the monitor
	int ret = udev_monitor_enable_receiving(m_udevMonitor);
	if (ret != 0) {
		qCritical("failed to enable udev monitor (%d)", ret);
		return;
	}

#endif // defined(HAVE_LIBUDEV)

}

LinuxDeviceNotifierImpl::~LinuxDeviceNotifierImpl()
{
	if (m_udevMonitorNotifier) {
		delete m_udevMonitorNotifier;
		m_udevMonitorNotifier = nullptr;
	}

#if defined(HAVE_LIBUDEV)

	m_udevMonitorFd = -1;

	if (m_udevMonitor) {
		udev_monitor_filter_remove(m_udevMonitor);
		udev_monitor_unref(m_udevMonitor);
		m_udevMonitor = nullptr;
	}

	if (m_udevEnumerate) {
		udev_enumerate_unref(m_udevEnumerate);
		m_udevEnumerate = nullptr;
	}

	if (m_udevHandle) {
		udev_unref(m_udevHandle);
		m_udevHandle = nullptr;
	}

#endif // defined(HAVE_LIBUDEV)

}

// -----------------------------------------------------------------------------
/*!
	\fn bool LinuxDeviceNotifier::isValid() const

	Returns \c true if the notifier was created successifully and is currently
	listening for events.

 */
bool LinuxDeviceNotifierImpl::isValid() const
{
	return m_udevMonitor && m_udevEnumerate && m_udevHandle && (m_udevMonitorFd >= 0);
}

// -----------------------------------------------------------------------------
/*!
	\fn LinuxDeviceNotifier::addTagMatchFilter(const QString &tag)

	Adds a filter to allow events with the supplied \a tag to generate
	\a deviceAdded and \a deviceRemoved signals.

	This filter is efficiently executed inside the kernel, meaning subscribers
	will usually not be woken up for devices which do not match.

 */
void LinuxDeviceNotifierImpl::addTagMatchFilter(const QString &tag)
{
#if !defined(HAVE_LIBUDEV)

	Q_UNUSED(tag);

#else // defined(HAVE_LIBUDEV)

	int ret;

	// add the filter to the enumerator
	if (Q_UNLIKELY(m_udevEnumerate == nullptr)) {
		qWarning("invalid udev monitor object");

	} else {
		ret = udev_enumerate_add_match_tag(m_udevEnumerate,
		                                   tag.toLocal8Bit().data());
		if (ret < 0)
			qWarning("failed to install enumerate tag match (%d)", ret);

	}

	// add the filter to the monitor
	if (Q_UNLIKELY(m_udevMonitor == nullptr)) {
		qWarning("invalid udev monitor object");

	} else {

		// add the tag match
		ret = udev_monitor_filter_add_match_tag(m_udevMonitor,
		                                        tag.toLocal8Bit().data());
		if (ret < 0)
			qWarning("failed to install filter tag match (%d)", ret);

		// update the filter on the running monitor
		ret = udev_monitor_filter_update(m_udevMonitor);
		if (ret < 0)
			qWarning("failed to update filter (%d)", ret);

	}

#endif // defined(HAVE_LIBUDEV)
}

// -----------------------------------------------------------------------------
/*!
	\fn LinuxDeviceNotifier::addSubsystemMatchFilter(LinuxDevice::SubSystem subSystem)

	Adds a filter to allow events with the supplied \a subSystem and optional
	\a devType to generate \a deviceAdded and \a deviceRemoved signals.

	This filter is efficiently executed inside the kernel, meaning subscribers
	will usually not be woken up for devices which do not match.

 */
void LinuxDeviceNotifierImpl::addSubsystemMatchFilter(LinuxDevice::SubSystem subSystem)
{
	// sanity check we know about the subsystem
	if (Q_UNLIKELY(!m_subSystemNames.contains(subSystem))) {
		qWarning() << "failed to find subsystem string for" << subSystem;
		return;
	}

	// convert the subsystem to a (ascii) string
	QByteArray subSystemStr = m_subSystemNames[subSystem].toLatin1();


#if defined(HAVE_LIBUDEV)

	int ret;

	// add the filter to the enumerator
	if (Q_UNLIKELY(m_udevEnumerate == nullptr)) {
		qWarning("invalid udev monitor object");

	} else {
		ret = udev_enumerate_add_match_subsystem(m_udevEnumerate,
		                                         subSystemStr.data());
		if (ret < 0)
			qWarning("failed to install enumerate tag match (%d)", ret);
	}

	// add the filter to the monitor
	if (Q_UNLIKELY(m_udevMonitor == nullptr)) {
		qWarning("invalid udev monitor object");

	} else {

		// add the subsystem match
		ret = udev_monitor_filter_add_match_subsystem_devtype(m_udevMonitor,
		                                                      subSystemStr.data(),
		                                                      nullptr);
		if (ret < 0)
			qWarning("failed to install filter subsystem match (%d)", ret);

		// update the filter on the running monitor
		ret = udev_monitor_filter_update(m_udevMonitor);
		if (ret < 0)
			qWarning("failed to update filter (%d)", ret);
	}

#endif // defined(HAVE_LIBUDEV)
}

// -----------------------------------------------------------------------------
/*!
	\fn LinuxDeviceNotifier::removeAllFilters()

	Remove all filters from monitor.

 */
void LinuxDeviceNotifierImpl::removeAllFilters()
{
#if defined(HAVE_LIBUDEV)

	// FIXME: need to remove the filters from the enumerate object

	if (Q_UNLIKELY(m_udevMonitor == nullptr)) {
		qWarning("invalid udev monitor object");

	} else {

		// try and remove all the filters
		int ret = udev_monitor_filter_remove(m_udevMonitor);
		if (ret < 0)
			qWarning("failed to remove monitor filter (%d)", ret);

		// update the filter on the running monitor
		ret = udev_monitor_filter_update(m_udevMonitor);
		if (ret < 0)
			qWarning("failed to update filter (%d)", ret);

	}

#endif // defined(HAVE_LIBUDEV)
}

// -----------------------------------------------------------------------------
/*!
	\fn LinuxDeviceNotifier::listDevices(LinuxDevice::SubSystem subSystem) const

	Enumerates over all the devices on the system and returns a list of any that
	belong to the \a subSystem sub system.

	This function is subject to any filters installed by the
	\a LinuxDeviceNotifier::addSubsystemMatchFilter() or
	\a LinuxDeviceNotifier::addTagMatchFilter() methods.

 */
QList<LinuxDevice> LinuxDeviceNotifierImpl::listDevices(LinuxDevice::SubSystem subSystem) const
{
	QList<LinuxDevice> deviceList;

#if !defined(HAVE_LIBUDEV)

	Q_UNUSED(subSystem);

#else // defined(HAVE_LIBUDEV)

	if (Q_UNLIKELY(m_udevEnumerate == nullptr)) {
		qWarning("invalid udev monitor object");
		return deviceList;
	}

	int ret = udev_enumerate_scan_devices(m_udevEnumerate);
	if (ret < 0) {
		qWarning("failed to to scan devices (%d)", ret);
		return deviceList;
	}

	// get the devices found, can be null if none found
	struct udev_list_entry *devices = udev_enumerate_get_list_entry(m_udevEnumerate);
	if (!devices)
		return deviceList;

	
	// iterate through the list of devices
	struct udev_list_entry *device;
	udev_list_entry_foreach(device, devices) {

		// get the filename of the /sys entry for the device and create a
		// udev_device object (dev) representing it
		const char *path = udev_list_entry_get_name(device);
		if (!path) {
			qWarning("null path returned for enum entry");
			continue;
		}

		// get the device at the given path
		struct udev_device *dev = udev_device_new_from_syspath(m_udevHandle, path);
		if (!dev) {
			qWarning("null device returned for path '%s'", path);
			continue;
		}

		// wrap one of our LinuxDevice objects around it
		LinuxDevice linuxDevice(dev);
		if (linuxDevice.isValid()) {

			if ((subSystem == LinuxDevice::UnknownSubSystem) ||
			    (subSystem == linuxDevice.subSystem()))
				deviceList.append(std::move(linuxDevice));
		}

		// free the udev device
		udev_device_unref(dev);
	}

#endif // defined(HAVE_LIBUDEV)

	return deviceList;
}

// -----------------------------------------------------------------------------
/*!
	\fn LinuxDeviceNotifier::listDevices() const

	Enumerates over all the devices on the system and returns a list of them.

	This function is subject to any filters installed by the
	\a LinuxDeviceNotifier::addSubsystemMatchFilter() or
	\a LinuxDeviceNotifier::addTagMatchFilter() methods.

 */
QList<LinuxDevice> LinuxDeviceNotifierImpl::listDevices() const
{
	return listDevices(LinuxDevice::UnknownSubSystem);
}

// -----------------------------------------------------------------------------
/*!
	\fn LinuxDeviceNotifier::devPath() const

	Returns the path to the device node directory, typically this would be
	"/dev", but it may not be if the \c UDEV_ROOT environment variable is set
	or the \c udev.conf config file points somewhere else.

 */
QString LinuxDeviceNotifierImpl::devPath() const
{
#if !defined(HAVE_LIBUDEV)

	return QString();

#else

	if (!m_udevHandle)
		return QString();

#if defined(SKY_FUSION_PLATFORM)
	const char *path = udev_get_dev_path(m_udevHandle);
	if (!path) {
		qWarning("invalid dev path returned by libudev");
		return QString();
	}
#else
	const char *path = "/dev/";
#endif

	return QString(path);

#endif // defined(HAVE_LIBUDEV)
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Slot called when the udev monitor receives one or more events.  This
	function then iterates over the events received and may or may not trigger
	a \a deviceAdded or \a deviceRemoved signal.

 */
void LinuxDeviceNotifierImpl::onMonitorActivated(int udevFd)
{
#if !defined(HAVE_LIBUDEV)

	Q_UNUSED(udevFd);

#else

	// sanity check the socket fd
	if (m_udevMonitorFd != udevFd) {
		qWarning("mismatched udev monitor fd (expected=%d, actual=%d)",
		         m_udevMonitorFd, udevFd);
		return;
	}

	// get the device that was added / removed / modified
	struct udev_device *device = udev_monitor_receive_device(m_udevMonitor);
	if (Q_LIKELY(device != nullptr)) {

		// what to do if we don't know the action ?
		const char *action = udev_device_get_action(device);
		if (!action || (action[0] == '\0')) {
			qWarning("failed to get udev event action");

		} else {

			// debugging to dump out all the properties of the device received
			if (false) {

				// iterate over the list of properties
				struct udev_list_entry *properties = udev_device_get_properties_list_entry(device);
				struct udev_list_entry *property = nullptr;

				udev_list_entry_foreach(property, properties) {

					// get the property details
					const char *propName = udev_list_entry_get_name(property);
					const char *propValue = udev_list_entry_get_value(property);
					qDebug("property name='%s' value='%s'", propName, propValue);
				}

			}

			// wrap the udev device in one of our linux devices and call the
			// relevant signals
			LinuxDevice linuxDevice(device);
			if (linuxDevice.isValid()) {
				
				if (strcmp(action, "add") == 0)
					emit deviceAdded(linuxDevice);
				else if (strcmp(action, "remove") == 0)
					emit deviceRemoved(linuxDevice);
			}

		}

		// free the device
		udev_device_unref(device);
	}
	
#endif // defined(HAVE_LIBUDEV)
}

