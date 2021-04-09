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
//  blegattprofile.cpp
//  SkyBluetoothRcu
//

#include "blegattprofile_p.h"
#include "blegattservice_p.h"
#include "blegattcharacteristic_p.h"
#include "blegattdescriptor_p.h"

#include "interfaces/bluezadapterinterface.h"
#include "interfaces/bluezgattserviceinterface.h"
#include "interfaces/bluezgattcharacteristicinterface.h"
#include "interfaces/bluezgattdescriptorinterface.h"

#include "utils/logging.h"
#include "dbus/dbusobjectmanager.h"

#include <QTimer>
#include <QDBusPendingCallWatcher>


BleGattProfileBluez::BleGattProfileBluez(const QDBusConnection &bluezDBusConn,
                                         const QDBusObjectPath &bluezDBusPath,
                                         QObject *parent)
	: BleGattProfile(parent)
	, m_dbusConn(bluezDBusConn)
	, m_dbusPath(bluezDBusPath)
	, m_bluezVersion(5, 47)
	, m_valid(false)
{

	m_valid = true;
}

BleGattProfileBluez::~BleGattProfileBluez()
{
}

// -----------------------------------------------------------------------------
/*!
	\overload


 */
bool BleGattProfileBluez::isValid() const
{
	return m_valid;
}

// -----------------------------------------------------------------------------
/*!
	\overload

	Returns \c true if the profile doesn't contain any services.

 */
bool BleGattProfileBluez::isEmpty() const
{
	return m_services.isEmpty();
}

// -----------------------------------------------------------------------------
/*!
	\overload

	Called from the GattServices class when it is restarting services and wants
	the profile.  However on Android we always keep the profile in sync so there
	is no need to do anything here, except clients expect that the updateComplete
	signal is emitted once done.

 */
void BleGattProfileBluez::updateProfile()
{
	// clear the old data first, will also clean-up any slot receiver details
	// that haven't yet been called
	m_services.clear();

	// construct a method call to get all the objects
	QDBusMessage request = QDBusMessage::createMethodCall(QStringLiteral("org.bluez"),
	                                                      QStringLiteral("/"),
	                                                      QStringLiteral("org.freedesktop.DBus.ObjectManager"),
	                                                      QStringLiteral("GetManagedObjects"));

	// send a request to the vendor to get the current tv code
	QDBusPendingCall pendingReply = m_dbusConn.asyncCall(request);
	QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(pendingReply, this);

	// install the functor on the completion of the request
	QObject::connect(watcher, &QDBusPendingCallWatcher::finished,
	                 this, &BleGattProfileBluez::onGetObjectsReply);
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Called when a reply (or timeout error) is received to our query to get the
	objects in the bluez dbus tree.

 */
void BleGattProfileBluez::onGetObjectsReply(QDBusPendingCallWatcher *call)
{
	// clean up the pending call on the next time through the event loop
	call->deleteLater();

	// check for an error and log it
	QDBusPendingReply<DBusManagedObjectList> reply = *call;
	if (reply.isError()) {

		const QDBusError error = reply.error();
		qWarning() << "failed to get bluez object list" << error;

		return;
	}

	const QString adapterInterfaceName = BluezAdapterInterface::staticInterfaceName();

	const DBusManagedObjectList objects = reply.value();
	const QString bluezDevicePathStr = m_dbusPath.path();

	// we are looking for objects that have the following interfaces
	//   org.bluez.GattService1
	//   org.bluez.GattCharacteristic1
	//   org.bluez.GattDescriptor1
	//
	// we initial scan everything into a flat list, then we populate the tree;
	// services first, then characteristics and finally descriptors

	QVector< QSharedPointer<BleGattCharacteristicBluez> > characteristics;
	QVector< QSharedPointer<BleGattDescriptorBluez> > descriptors;

	characteristics.reserve(32);
	descriptors.reserve(64);

	DBusManagedObjectList::const_iterator object = objects.begin();
	for (; object != objects.end(); ++object) {

		// get the object path and interfaces
		const QString path = object.key().path();
		const DBusInterfaceList &interfaces = object.value();

		// if the object contains the "org.bluez.Adapter1" interface then
		// we read it so that we can get the version of bluez, this is needed
		// later when using some of the GATT APIs
		if (interfaces.contains(adapterInterfaceName))
			updateBluezVersion(interfaces[adapterInterfaceName]);

		// check the object path is under the one we are looking for, i.e. the
		// object belongs to this rcu device
		if (!path.startsWith(bluezDevicePathStr))
			continue;

		DBusInterfaceList::const_iterator interface = interfaces.begin();
		for (; interface != interfaces.end(); ++interface) {

			// get the interface name and properties
			const QString &name = interface.key();
			const QVariantMap &properties = interface.value();

			// check if a service
			if (name == BluezGattServiceInterface::staticInterfaceName()) {

				QSharedPointer<BleGattServiceBluez> service =
						QSharedPointer<BleGattServiceBluez>::create(m_dbusConn, path, properties);
				if (service && service->isValid())
					m_services.insertMulti(service->uuid(), service);
				else
					qError("failed to create BleGattServiceBluez object");

			// check is a characteristic
			} else if (name == BluezGattCharacteristicInterface::staticInterfaceName()) {

				QSharedPointer<BleGattCharacteristicBluez> characteristic =
						QSharedPointer<BleGattCharacteristicBluez>::create(m_dbusConn, path, properties);
				if (characteristic && characteristic->isValid())
					characteristics.append(characteristic);
				else
					qError("failed to create BleGattCharacteristicBluez object");

			// check if a descriptor
			} else if (name == BluezGattDescriptorInterface::staticInterfaceName()) {

				QSharedPointer<BleGattDescriptorBluez> descriptor =
						QSharedPointer<BleGattDescriptorBluez>::create(m_dbusConn, path, properties);
				if (descriptor && descriptor->isValid())
					descriptors.append(descriptor);
				else
					qError("failed to create BleGattDescriptorBluez object");

			}
		}

	}


	// add the descriptors to their parent characteristic
	for (const QSharedPointer<BleGattDescriptorBluez> &descriptor : descriptors) {

		// get the path to the parent characteristic in string form
		const QDBusObjectPath &parentPath = descriptor->m_characteristicPath;

		// find the parent characteristic
		bool found = false;
		for (QSharedPointer<BleGattCharacteristicBluez> &characteristic : characteristics) {
			if (parentPath == characteristic->m_path) {
				descriptor->m_characteristic = characteristic;
				characteristic->addDescriptor(descriptor);
				found = true;
				break;
			}
		}

		// check we found the parent characteristic, if not something has gone
		// wrong in bluez
		if (Q_UNLIKELY(!found)) {
			qWarning() << "failed to find parent gatt characteristic for descriptor"
			           << descriptor->m_uuid << "@" << descriptor->m_path.path();
		}
	}


	// and then add the characteristics to their parent services
	for (const QSharedPointer<BleGattCharacteristicBluez> &characteristic : characteristics) {

		// for each characteristic update the bluez version so the correct
		// version of the dbus API is used
		characteristic->setBluezVersion(m_bluezVersion);

		// get the path to the parent characteristic in string form
		const QDBusObjectPath &parentPath = characteristic->m_servicePath;

		// find the parent service
		bool found = false;
		for (QSharedPointer<BleGattServiceBluez> &service : m_services) {
			if (parentPath == service->m_path) {
				characteristic->m_service = service;
				service->addCharacteristic(characteristic);
				found = true;
				break;
			}
		}

		// check we found the parent service, if not something has gone wrong
		// in bluez
		if (Q_UNLIKELY(!found)) {
			qWarning() << "failed to find parent gatt service for characteristic"
			           << characteristic->m_uuid << "@" << characteristic->m_path.path();
		}
	}


	// (we shouldn't have but as a final sanity) check that the services we have
	// all list their device as the one we're targeting. We remove any that
	// don't and log an error
	QMultiMap<BleUuid, QSharedPointer<BleGattServiceBluez>>::iterator it =
		m_services.begin();
	while (it != m_services.end()) {

		const QSharedPointer<BleGattServiceBluez> &service = it.value();

		if (Q_UNLIKELY(service->m_devicePath != m_dbusPath)) {
			qWarning() << "service with uuid"
			           << service->m_uuid << "@"
			           << service->m_devicePath.path()
			           << "unexpectedly does not belong to the target device";
			it = m_services.erase(it);
		} else {
			++it;
		}
	}


	// debugging
	dumpGattTree();

	// finally finished so notify the original caller that all objects are
	// fetched, use a singleShot timer so the event is triggered from the
	// main event loop
#if QT_VERSION >= QT_VERSION_CHECK(5, 4, 0)
	QTimer::singleShot(0, this, &BleGattProfile::updateCompleted);
#else
	{
		QTimer* timer = new QTimer(this);
		QObject::connect(timer, &QTimer::timeout, this, &BleGattProfile::updateCompleted);
		QObject::connect(timer, &QTimer::timeout, timer, &QObject::deleteLater);
		timer->setSingleShot(true);
		timer->start(0);
	}
#endif
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Scans the dbus properties of the org.bluez.Adapter1 interface for the
	"Modalias" property and then reads the value to determine the version
	of bluez we are using.

	This is needed to determine the API used for some of the GATT operations;
	the API changed between 5.47 and 5.48 but the interface version number
	didn't ... hence need to know which version we're talking to.

 */
void BleGattProfileBluez::updateBluezVersion(const QVariantMap &properties)
{
	const QVariant modalias = properties.value(QStringLiteral("Modalias"));
	if (modalias.isNull() || !modalias.canConvert<QString>()) {
		qWarning("failed to get 'Modalias' property");
		return;
	}

	const QString strModalias = modalias.toString();

	char type[64];
	int vendor, product, version;

	int fields = sscanf(qPrintable(strModalias), "%63[^:]:v%04Xp%04Xd%04X",
	                    type, &vendor, &product, &version);
	if (fields != 4) {
		qWarning("failed to parse 'Modalias' property value '%s'",
		         qPrintable(strModalias));

	} else if ((vendor != 0x1d6b) || (product != 0x0246)) {
		qWarning("invalid vendor (0x%04x) or product (0x%04x) 'Modalias' value",
		         vendor, product);

	} else if ((version >> 8) != 5) {
		qWarning("unexpected 'Modalias' major version number (0x%04x)",
		         version);

	} else {
		m_bluezVersion = QVersionNumber((version >> 8), (version & 0xff));
		qDebug("found bluez version '%s'", qPrintable(m_bluezVersion.toString()));
	}
}

// -----------------------------------------------------------------------------
/*!
	Debugging function that dumps out details on the services, characteristics
	& descriptors found in the last update.

 */
void BleGattProfileBluez::dumpGattTree()
{
	for (const QSharedPointer<BleGattServiceBluez> &service : m_services) {

		qDebug() << "+-- Service:" << service->uuid();
		qDebug() << ".   +-- Path:" << service->m_path.path();
		qDebug() << ".   +-- Primary:" << service->primary();
		qDebug() << ".   +-- InstanceId:" << service->instanceId();

		for (const QSharedPointer<BleGattCharacteristicBluez> &characteristic : service->m_characteristics) {

			qDebug() << ".   +-- Characteristic:" << characteristic->uuid();
			qDebug() << ".   .   +-- Path:" << characteristic->m_path.path();
			qDebug() << ".   .   +-- Flags:" << characteristic->flags();
			qDebug() << ".   .   +-- InstanceId:" << characteristic->instanceId();

			for (const QSharedPointer<BleGattDescriptorBluez> &descriptor : characteristic->m_descriptors) {

				qDebug() << ".   .   +-- Descriptor:" << descriptor->uuid();
				qDebug() << ".   .   .   +-- Path:" << descriptor->m_path.path();
				qDebug() << ".   .   .   +-- Flags:" << descriptor->flags();
			}
		}
	}
}

// -----------------------------------------------------------------------------
/*!


 */
QList< QSharedPointer<BleGattService> > BleGattProfileBluez::services() const
{
	QList< QSharedPointer<BleGattService> > services;
	services.reserve(m_services.size());

	for (QSharedPointer<BleGattServiceBluez> service : m_services)
		services.append(service);

	return services;
}

// -----------------------------------------------------------------------------
/*!


 */
QList< QSharedPointer<BleGattService> > BleGattProfileBluez::services(const BleUuid &serviceUuid) const
{
	QList< QSharedPointer<BleGattService> > services;
	services.reserve(m_services.size());

	for (QSharedPointer<BleGattServiceBluez> service : m_services) {
		if (service->uuid() == serviceUuid)
			services.append(service);
	}

	return services;
}

// -----------------------------------------------------------------------------
/*!


 */
QSharedPointer<BleGattService> BleGattProfileBluez::service(const BleUuid &serviceUuid) const
{
	return m_services.value(serviceUuid);
}

