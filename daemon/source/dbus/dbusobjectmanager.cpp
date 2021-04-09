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
//  dbusobjectmanager.cpp
//  BleRcuDaemon
//

#include "dbusobjectmanager.h"

#include <QDebug>
#include <QAtomicInteger>
#include <QMetaClassInfo>

#include <QDBusArgument>
#include <QDBusMetaType>



// -----------------------------------------------------------------------------
/**
 *	@brief Registers the types used for this dbus interface
 *
 *	The function contains an atomic check to ensure that the types are only
 *	registered once
 *
 */
static void registerObjectManagerTypes()
{
	static QAtomicInteger<bool> isDone(false);

	if (isDone.testAndSetAcquire(false, true)) {
		qDBusRegisterMetaType<DBusInterfaceList>();
		qDBusRegisterMetaType<DBusManagedObjectList>();
	}
}



// -----------------------------------------------------------------------------
/**
 *	@brief
 *
 *
 *
 */
DBusObjectManagerInterface::DBusObjectManagerInterface(const QString &service,
                                                       const QString &path,
                                                       const QDBusConnection &connection,
                                                       QObject *parent)
	: QDBusAbstractInterface(service, path, staticInterfaceName(), connection, parent)
{
	registerObjectManagerTypes();
}

DBusObjectManagerInterface::~DBusObjectManagerInterface()
{
}


// -----------------------------------------------------------------------------
/**
 *	@brief
 *
 *
 *
 */
DBusObjectManagerAdaptor::DBusObjectManagerAdaptor(QObject *parent)
	: QDBusAbstractAdaptor(parent)
{
	registerObjectManagerTypes();
}

DBusObjectManagerAdaptor::~DBusObjectManagerAdaptor()
{
}

// -----------------------------------------------------------------------------
/**
 *	@brief
 *
 *
 *
 */
QVariantMap DBusObjectManagerAdaptor::getAllProperties(const QDBusAbstractAdaptor* const adaptor) const
{
	QVariantMap properties;

	// get the meta object and a list of all it's properties
	const QMetaObject *meta = adaptor->metaObject();
	for (int i = 0; i < meta->propertyCount(); i++) {

		const QMetaProperty propertyMeta = meta->property(i);
		if (propertyMeta.isValid()) {

			const QString propertyName = propertyMeta.name();
			const QVariant propertyValue = propertyMeta.read(adaptor);

			if (propertyValue.isValid())
				properties.insert(propertyName, propertyValue);

		}
		
	}

	return properties;
}

// -----------------------------------------------------------------------------
/**
 *	@brief
 *
 *
 *
 */
DBusInterfaceList DBusObjectManagerAdaptor::getAllInterfaces(const QObject* const object) const
{
	static const QString classInfoIFaceName("D-Bus Interface");

	DBusInterfaceList interfaces;

	// get all the adaptors
	const QList<QDBusAbstractAdaptor*> allAdaptors =
		object->findChildren<QDBusAbstractAdaptor*>();

	// iterate through them to find one that has Q_CLASSINFO("D-Bus Interface", "<interface>")
	for (QDBusAbstractAdaptor* const &adaptor : allAdaptors) {

		// get the class info which contains the adaptors interface
		const QMetaObject *meta = adaptor->metaObject();
		for (int i = 0; i < meta->classInfoCount(); i++) {

			const QMetaClassInfo info = meta->classInfo(i);
			if (info.name() == classInfoIFaceName) {

				// get the interface name
				const QString interface(info.value());
				if (!interface.isNull() && !interface.isEmpty()) {

					// get all the properties
					const QVariantMap properties = getAllProperties(adaptor);

					// add the interface and properties
					interfaces.insert(interface, properties);
				}
			}
		}
	}

	return interfaces;
}

// -----------------------------------------------------------------------------
/**
 *	@brief DBus method call where we need to return all the dbus 'objects'
 *
 *
 *
 */
DBusManagedObjectList DBusObjectManagerAdaptor::GetManagedObjects()
{
	static const QString pathPropertyName("objectPath");

	DBusManagedObjectList objectList;

	// iterate through children of the parent and find any that have dbus
	// adaptor children, i.e. dbus objects
	const QObjectList &children = parent()->children();

	for (QObject* const &child : children) {

		// check if the child has a property called 'objectPath', it's present
		// indicates it is a dbus object
		const QMetaObject *meta = child->metaObject();
		for (int i = 0; i < meta->propertyCount(); i++) {

			const QMetaProperty propertyMeta = meta->property(i);
			if (propertyMeta.isValid() && (propertyMeta.name() == pathPropertyName)) {

				const QVariant propertyValue = propertyMeta.read(child);
				if (propertyValue.isValid() && !propertyValue.isNull() &&
					propertyValue.canConvert<QDBusObjectPath>()) {

					// get the object path
					const QDBusObjectPath objectPath = qvariant_cast<QDBusObjectPath>(propertyValue);

					// then gather all the adaptors / interfaces on the object
					const DBusInterfaceList interfaces = getAllInterfaces(child);

					// add the object
					objectList.insert(objectPath, interfaces);
				}

			}

		}

	}

	return objectList;
}

