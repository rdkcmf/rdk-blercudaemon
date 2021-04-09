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
//  dbusproperties.cpp
//  SkyBluetoothRcu
//

#include "dbusproperties.h"

#include <QMetaClassInfo>
#include <QDebug>


DBusPropertieInterface::DBusPropertieInterface(const QString &service,
                                               const QString &path,
                                               const QDBusConnection &connection,
                                               QObject *parent)
	: QDBusAbstractInterface(service, path, staticInterfaceName(), connection, parent)
{
}

DBusPropertieInterface::~DBusPropertieInterface()
{
}




DBusPropertiesAdaptor::DBusPropertiesAdaptor(QObject *parent)
	: QDBusAbstractAdaptor(parent)
{
	setAutoRelaySignals(true);
}

DBusPropertiesAdaptor::~DBusPropertiesAdaptor()
{
}


QDBusAbstractAdaptor* DBusPropertiesAdaptor::findAdaptor(const QString &interface)
{
	static const QString classInfoIFaceName("D-Bus Interface");

	// get all the adaptors
	const QList<QDBusAbstractAdaptor*> allAdaptors =
		parent()->findChildren<QDBusAbstractAdaptor*>();

	// iterate through them to find one that has Q_CLASSINFO("D-Bus Interface", "<interface>")
	for (QDBusAbstractAdaptor* const &adaptor : allAdaptors) {

		// get the class info which contains the adaptors interface
		const QMetaObject *meta = adaptor->metaObject();
		for (int i = 0; i < meta->classInfoCount(); i++) {

			const QMetaClassInfo info = meta->classInfo(i);
			if ((info.name() == classInfoIFaceName) &&
			    (info.value() == interface)) {

				// set have a match, this adpator is for the given interface
				return adaptor;
			}

		}

	}

	return nullptr;
}

// -----------------------------------------------------------------------------
/**
 *	@brief Method call org.freedesktop.DBus.Properties.Get
 *
 *	Gets a single property of the parent object's interface.  We use the Qt
 *	meta system to find dbus adaptors (interfaces) attached to the parent object
 *	and then if we find a match for the given adaptor then we query that for
 *	the given property.
 *
 *
 */
QDBusVariant DBusPropertiesAdaptor::Get(const QString &interface, const QString &name)
{
	// sanity check we not trying to recursively get properites on ourselves
	if (Q_UNLIKELY(interface == DBusPropertieInterface::staticInterfaceName())) {
		return QDBusVariant();
	}

	// try and find an adaptor attached to the parent object for the given
	// interface
	QDBusAbstractAdaptor *adaptor = findAdaptor(interface);
	if (!adaptor) {
		qWarning() << "failed to find adaptor for interface" << interface;
		return QDBusVariant();
	}

	// next check if the adaptor has the given property
	QVariant property = adaptor->property(name.toLatin1());
	if (property.isNull() || !property.isValid()) {
		qWarning() << "dbus requested unknown property" << name;
		return QDBusVariant();
	}

	return QDBusVariant(property);
}

// -----------------------------------------------------------------------------
/**
 *	@brief Method call org.freedesktop.DBus.Properties.Set
 *
 *
 *
 */
void DBusPropertiesAdaptor::Set(const QString &interface, const QString &name, const QDBusVariant &value)
{
	// sanity check we not trying to recursively get properites on ourselves
	if (Q_UNLIKELY(interface == DBusPropertieInterface::staticInterfaceName()))
		return;

	// try and find an adaptor attached to the parent object for the given
	// interface
	QDBusAbstractAdaptor *adaptor = findAdaptor(interface);
	if (!adaptor) {
		qWarning() << "failed to find adaptor for interface" << interface;
		return;
	}

	// next check if the adaptor has the given property
	if (!adaptor->setProperty(name.toLatin1(), value.variant()))
		qWarning() << "dbus requested to write property" << name << "with value"
		           << value.variant() << "failed";
}

// -----------------------------------------------------------------------------
/**
 *	@brief Method call org.freedesktop.DBus.Properties.GetAll
 *
 *
 *
 */
QVariantMap DBusPropertiesAdaptor::GetAll(const QString &interface)
{
	QVariantMap properties;

	// sanity check we not trying to recursively get properites on ourselves
	if (Q_UNLIKELY(interface == DBusPropertieInterface::staticInterfaceName())) {
		return properties;
	}

	// try and find an adaptor attached to the parent object for the given
	// interface
	QDBusAbstractAdaptor *adaptor = findAdaptor(interface);
	if (!adaptor) {
		qWarning() << "failed to find adaptor for interface" << interface;
		return properties;
	}

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

