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
//  linuxinputdevicemanager.cpp
//  SkyBluetoothRcu
//

#include "linuxinputdevicemanager.h"
#include "linuxinputdeviceinfo.h"
#include "linuxinputdevice.h"
#include "linuxdevicenotifier.h"

#include "logging.h"

#include <QMutex>


QSharedPointer<InputDeviceManager> InputDeviceManager::create(QObject *parent)
{
	static QMutex notifierLock;
	static QSharedPointer<LinuxDeviceNotifier> notifier;

	{
		QMutexLocker locker(&notifierLock);
		if (!notifier) {
			notifier = LinuxDeviceNotifier::create(LinuxDeviceNotifier::UDev, -1);
			notifier->addSubsystemMatchFilter(LinuxDevice::InputSubSystem);
		}
	}


	QSharedPointer<LinuxInputDeviceManager> manager =
		QSharedPointer<LinuxInputDeviceManager>::create(notifier, parent);
	if (!manager || !manager->isValid())
		manager.reset();

	return manager;
}



// -----------------------------------------------------------------------------
/*!
	\internal


 */
LinuxInputDeviceManager::LinuxInputDeviceManager(const QSharedPointer<LinuxDeviceNotifier> &notifier,
                                                 QObject *parent)
	: InputDeviceManager(parent)
{

	// connect to the signals notifing input devices added or removed
	QObject::connect(notifier.data(), &LinuxDeviceNotifier::deviceAdded,
	                 this, &LinuxInputDeviceManager::onInputDeviceAdded);
	QObject::connect(notifier.data(), &LinuxDeviceNotifier::deviceRemoved,
	                 this, &LinuxInputDeviceManager::onInputDeviceRemoved);

}

// -----------------------------------------------------------------------------
/*!

 */
bool LinuxInputDeviceManager::isValid() const
{
	return true;
}

// -----------------------------------------------------------------------------
/*!
	\fn void InputDeviceManager::dump(Dumper out) const

	Currently not implemented on Linux.

 */
void LinuxInputDeviceManager::dump(Dumper out) const
{
	Q_UNUSED(out)
}

// -----------------------------------------------------------------------------
/*!
	\fn QList<InputDeviceInfo> InputDeviceManager::inputDevices() const

	Returns the list of input devices currently attached.

 */
QList<InputDeviceInfo> LinuxInputDeviceManager::inputDevices() const
{
	QList<InputDeviceInfo> devices;

	// lambda called for every input device in the system, it receives the
	// device node fd and path and if it checks out the devices list is updated
	// with the info
	std::function<void(int, const QString&)> func =
		[&devices](int fd, const QString &path)
		{
			QSharedPointer<LinuxInputDeviceInfo> device =
				QSharedPointer<LinuxInputDeviceInfo>::create(fd, path);
			if (device && !device->isNull())
				devices.append(InputDeviceInfo(device));
		};

	// run the lambda over all devices
	LinuxInputDeviceInfo::forEachInputDevice(func);

	return devices;
}

// -----------------------------------------------------------------------------
/*!
	\fn InputDeviceInfo InputDeviceManager::findInputDevice(const BleAddress &address) const

	Attempts to find an input device using the \a address of the device.


 */
InputDeviceInfo LinuxInputDeviceManager::findInputDevice(const BleAddress &address) const
{
	QSharedPointer<LinuxInputDeviceInfo> device;

	// lambda called for every input device in the system, it receives the
	// device node fd and path and if it checks out the devices list is updated
	// with the info
	std::function<void(int, const QString&)> func =
		[&device, &address](int fd, const QString &path)
		{
			QSharedPointer<LinuxInputDeviceInfo> info =
				QSharedPointer<LinuxInputDeviceInfo>::create(fd, path);
			if (info && !info->isNull() && info->matches(address))
				device = info;
		};

	// run the lambda over all devices
	LinuxInputDeviceInfo::forEachInputDevice(func);

	// if the device shared pointer was sent it means we found a matching device
	if (!device)
		return InputDeviceInfo();
	else
		return InputDeviceInfo(device);
}

// -----------------------------------------------------------------------------
/*!
	\fn InputDeviceInfo InputDeviceManager::findInputDevice(const QString &name) const

	Attempts to find an input device using the \a name of the device.

	For android only the \a name will be used for the search, this is because
	it's not possible to get the bluetooth address of the input device from the
	android InputManager object.

 */
InputDeviceInfo LinuxInputDeviceManager::findInputDevice(const QString &name) const
{
	QSharedPointer<LinuxInputDeviceInfo> device;

	// lambda called for every input device in the system, it receives the
	// device node fd and path and if it checks out the devices list is updated
	// with the info
	std::function<void(int, const QString&)> func =
		[&device, &name](int fd, const QString &path)
		{
			QSharedPointer<LinuxInputDeviceInfo> info =
				QSharedPointer<LinuxInputDeviceInfo>::create(fd, path);
			if (info && !info->isNull() && (info->name() == name))
				device = info;
		};

	// run the lambda over all devices
	LinuxInputDeviceInfo::forEachInputDevice(func);

	// if the device shared pointer was sent it means we found a matching device
	if (!device)
		return InputDeviceInfo();
	else
		return InputDeviceInfo(device);
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Called when the device notifier advises us that a new input device has been
	added to the system.  We filter the event so we only emit \a deviceAdded()
	signals if the device added is an generic input type and it has the
	physical address property.

 */
void LinuxInputDeviceManager::onInputDeviceAdded(const LinuxDevice &device)
{
	// filter out invalid devices
	if (!device.isValid() || (device.subSystem() != LinuxDevice::InputSubSystem))
		return;

	qInfo() << "input device added :" << device;

	// filter out devices with major / minor numbers not in the input
	// event device range
	if (!LinuxInputDeviceInfo::isInputEventDeviceNumber(device.number()))
		return;

	QSharedPointer<LinuxInputDeviceInfo> deviceInfo =
		QSharedPointer<LinuxInputDeviceInfo>::create(device);

	// emit the device added signal with the device number and physical address
	emit deviceAdded(InputDeviceInfo(deviceInfo));
}

// -----------------------------------------------------------------------------
/*!
	\internal

 */
void LinuxInputDeviceManager::onInputDeviceRemoved(const LinuxDevice &device)
{
	// filter out invalid devices
	if (!device.isValid() || (device.subSystem() != LinuxDevice::InputSubSystem))
		return;

	qInfo() << "input device removed :" << device;

	// filter out devices with major / minor numbers not in the input
	// event device range
	if (!LinuxInputDeviceInfo::isInputEventDeviceNumber(device.number()))
		return;

	QSharedPointer<LinuxInputDeviceInfo> deviceInfo =
		QSharedPointer<LinuxInputDeviceInfo>::create(device);

	// emit the device added signal with the device number and physical address
	emit deviceRemoved(InputDeviceInfo(deviceInfo));
}

// -----------------------------------------------------------------------------
/*!
	Attempts to get the input device wrapper for the device with the given
	rcu address.

	If no input device exists with the address a null shared pointer is returned.
 */
QSharedPointer<InputDevice> LinuxInputDeviceManager::getDevice(const BleAddress &address) const
{
	QSharedPointer<LinuxInputDevice> device;

	// lambda called for every input device in the system, it receives the
	// device node fd and path and if it checks out the devices list is updated
	// with the info
	std::function<void(int, const QString &)> func =
		[&device, &address](int fd, const QString &path) {
			QSharedPointer<LinuxInputDeviceInfo> info =
				QSharedPointer<LinuxInputDeviceInfo>::create(fd, path);
			if (info && !info->isNull() && info->matches(address))
				device = QSharedPointer<LinuxInputDevice>::create(fd);
		};

	// run the lambda over all devices
	LinuxInputDeviceInfo::forEachInputDevice(func);

	// if the device shared pointer was set it means we found a matching device
	return device;
}

// -----------------------------------------------------------------------------
/*!
	Attempts to get the input device wrapper for the device with the given
	\a name.

	If no input device exists with the \a name a null shared pointer is returned.
 */
QSharedPointer<InputDevice> LinuxInputDeviceManager::getDevice(const QString &name) const
{
	QSharedPointer<LinuxInputDevice> device;

	// lambda called for every input device in the system, it receives the
	// device node fd and path and if it checks out the devices list is updated
	// with the info
	std::function<void(int, const QString &)> func =
		[&device, &name](int fd, const QString &path) {
			QSharedPointer<LinuxInputDeviceInfo> info =
				QSharedPointer<LinuxInputDeviceInfo>::create(fd, path);
			if (info && !info->isNull() && (info->name() == name))
				device = QSharedPointer<LinuxInputDevice>::create(fd);
		};

	// run the lambda over all devices
	LinuxInputDeviceInfo::forEachInputDevice(func);

	// if the device shared pointer was set it means we found a matching device
	return device;
}

// -----------------------------------------------------------------------------
/*!
	Attempts to get the input device wrapper for the device with the given
	\a info.

	If no input device exists with matching \a info then a null shared pointer
	is returned.
 */
QSharedPointer<InputDevice> LinuxInputDeviceManager::getDevice(const InputDeviceInfo &info) const
{
	QSharedPointer<LinuxInputDevice> device;

	// lambda called for every input device in the system, it receives the
	// device node fd and path and if it checks out the devices list is updated
	// with the info
	std::function<void(int, const QString &)> func =
		[&device, &info](int fd, const QString &path) {

			if (device.isNull()) {
				QSharedPointer<LinuxInputDeviceInfo> lnxInfo =
					QSharedPointer<LinuxInputDeviceInfo>::create(fd, path);

				if (lnxInfo && !lnxInfo->isNull()) {
					if (InputDeviceInfo(lnxInfo) == info)
						device = QSharedPointer<LinuxInputDevice>::create(fd);
				}
			}
		};

	// run the lambda over all devices
	LinuxInputDeviceInfo::forEachInputDevice(func);
	if (!device)
		qWarning() << "failed to get device with info" << info;

	// if the device shared pointer was set it means we found a matching device
	return device;
}
