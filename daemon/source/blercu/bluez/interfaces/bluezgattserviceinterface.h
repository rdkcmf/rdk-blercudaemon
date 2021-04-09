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
//  bluezgattserviceinterface.h
//  SkyBluetoothRcu
//

#ifndef BLUEZGATTSERVICEINTERFACE_H
#define BLUEZGATTSERVICEINTERFACE_H

#include "dbus/dbusabstractinterface.h"

#include <QObject>
#include <QString>
#include <QVariant>

#include <QtDBus>


class BluezGattServiceInterface : public DBusAbstractInterface
{
	Q_OBJECT

public:
	static inline const char *staticInterfaceName()
	{ return "org.bluez.GattService1"; }

public:
	BluezGattServiceInterface(const QString &service, const QString &path,
	                          const QDBusConnection &connection,
	                          QObject *parent = nullptr)
		: DBusAbstractInterface(service, path, staticInterfaceName(), connection, parent)
	{ }

	~BluezGattServiceInterface() final = default;

	Q_PROPERTY(QString UUID READ uuid)
	inline QString uuid() const
	{ return qvariant_cast< QString >(property("UUID")); }

	Q_PROPERTY(bool Primary READ primary)
	inline bool primary() const
	{ return qvariant_cast< bool >(property("Primary")); }

	Q_PROPERTY(QDBusObjectPath Device READ device)
	inline QDBusObjectPath device() const
	{ return qvariant_cast< QDBusObjectPath >(property("Device")); }

public slots:

signals:

};

#endif // !defined(BLUEZGATTSERVICEINTERFACE_H)
