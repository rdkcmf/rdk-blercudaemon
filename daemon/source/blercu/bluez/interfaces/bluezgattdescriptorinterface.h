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
//  bluezgattdescriptorinterface.h
//  SkyBluetoothRcu
//

#ifndef BLUEZGATTDESCRIPTORINTERFACE_H
#define BLUEZGATTDESCRIPTORINTERFACE_H

#include "dbus/dbusabstractinterface.h"

#include <QObject>
#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <QVariantMap>

#include <QtDBus>


class BluezGattDescriptorInterface : public DBusAbstractInterface
{
	Q_OBJECT

public:
	static inline const char *staticInterfaceName()
	{ return "org.bluez.GattDescriptor1"; }

public:
	BluezGattDescriptorInterface(const QString &service, const QString &path,
	                             const QDBusConnection &connection,
	                             QObject *parent = nullptr)
		: DBusAbstractInterface(service, path, staticInterfaceName(), connection, parent)
	{ }

	~BluezGattDescriptorInterface() final = default;

	Q_PROPERTY(QDBusObjectPath Characteristic READ characteristic)
	inline QDBusObjectPath characteristic() const
	{ return qvariant_cast< QDBusObjectPath >(property("Characteristic")); }

	Q_PROPERTY(QString UUID READ uuid)
	inline QString uuid() const
	{ return qvariant_cast< QString >(property("UUID")); }

	Q_PROPERTY(QByteArray Value READ value NOTIFY valueChanged)
	inline QByteArray value() const
	{ return qvariant_cast< QByteArray >(property("Value")); }

	Q_PROPERTY(QStringList Flags READ flags)
	inline QStringList flags() const
	{ return qvariant_cast< QStringList >(property("Flags")); }

public slots:
	inline QDBusPendingReply<QByteArray> ReadValue(const QVariantMap &options = QVariantMap())
	{
		QList<QVariant> argumentList;
		argumentList << QVariant::fromValue(options);
		return asyncCallWithArgumentList(QStringLiteral("ReadValue"), argumentList);
	}

	inline QDBusPendingReply<> WriteValue(const QByteArray &value, const QVariantMap &options = QVariantMap())
	{
		QList<QVariant> argumentList;
		argumentList << QVariant::fromValue(value);
		argumentList << QVariant::fromValue(options);
		return asyncCallWithArgumentList(QStringLiteral("WriteValue"), argumentList);
	}

signals:
	void valueChanged(const QByteArray &newValue);

};

#endif // !defined(BLUEZGATTDESCRIPTORINTERFACE_H)
