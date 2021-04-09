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
//  dbusproperties.h
//  BleRcuDaemon
//

#ifndef DBUSPROPERTIES_H
#define DBUSPROPERTIES_H

#include <QObject>
#include <QString>
#include <QVariant>

#include <QDBusVariant>
#include <QDBusConnection>
#include <QDBusPendingReply>
#include <QDBusAbstractAdaptor>
#include <QDBusAbstractInterface>



class DBusPropertieInterface : public QDBusAbstractInterface
{
	Q_OBJECT

public:
	static inline const char *staticInterfaceName()
	{ return "org.freedesktop.DBus.Properties"; }

public:
	DBusPropertieInterface(const QString &service, const QString &path,
	                       const QDBusConnection &connection, QObject *parent = nullptr);
	~DBusPropertieInterface();

public slots:
	inline QDBusPendingReply<QDBusVariant> Get(const QString &interface, const QString &name)
	{
		QList<QVariant> argumentList;
		argumentList << QVariant::fromValue(interface) << QVariant::fromValue(name);
		return asyncCallWithArgumentList(QStringLiteral("Get"), argumentList);
	}

	inline QDBusPendingReply<QVariantMap> GetAll(const QString &interface)
	{
		QList<QVariant> argumentList;
		argumentList << QVariant::fromValue(interface);
		return asyncCallWithArgumentList(QStringLiteral("GetAll"), argumentList);
	}

	inline QDBusPendingReply<> Set(const QString &interface, const QString &name, const QDBusVariant &value)
	{
		QList<QVariant> argumentList;
		argumentList << QVariant::fromValue(interface) << QVariant::fromValue(name) << QVariant::fromValue(value);
		return asyncCallWithArgumentList(QStringLiteral("Set"), argumentList);
	}

signals:
	void PropertiesChanged(const QString &interface,
	                       const QVariantMap &changedProperties,
	                       const QStringList &invalidatedProperties);
};



class DBusPropertiesAdaptor : public QDBusAbstractAdaptor
{
	Q_OBJECT
	Q_CLASSINFO("D-Bus Interface", "org.freedesktop.DBus.Properties")
	Q_CLASSINFO("D-Bus Introspection", ""
	            "  <interface name=\"org.freedesktop.DBus.Properties\">\n"
	            "    <method name=\"Get\">\n"
	            "      <arg direction=\"in\" type=\"s\" name=\"interface\"/>\n"
	            "      <arg direction=\"in\" type=\"s\" name=\"name\"/>\n"
	            "      <arg direction=\"out\" type=\"v\" name=\"value\"/>\n"
	            "    </method>\n"
	            "    <method name=\"Set\">\n"
	            "      <arg direction=\"in\" type=\"s\" name=\"interface\"/>\n"
	            "      <arg direction=\"in\" type=\"s\" name=\"name\"/>\n"
	            "      <arg direction=\"in\" type=\"v\" name=\"value\"/>\n"
	            "    </method>\n"
	            "    <method name=\"GetAll\">\n"
	            "      <arg direction=\"in\" type=\"s\" name=\"interface\"/>\n"
	            "      <arg direction=\"out\" type=\"a{sv}\" name=\"properties\"/>\n"
	            "    </method>\n"
	            "    <signal name=\"PropertiesChanged\">\n"
	            "      <arg type=\"s\" name=\"interface\"/>\n"
	            "      <arg type=\"a{sv}\" name=\"changed_properties\"/>\n"
	            "      <arg type=\"as\" name=\"invalidated_properties\"/>\n"
	            "    </signal>\n"
	            "  </interface>\n"
	            "")
public:
	DBusPropertiesAdaptor(QObject *parent);
	virtual ~DBusPropertiesAdaptor();

public slots:
	QDBusVariant Get(const QString &interface, const QString &name);
	QVariantMap GetAll(const QString &interface);
	void Set(const QString &interface, const QString &name, const QDBusVariant &value);

signals:
	void PropertiesChanged(const QString &interface, const QVariantMap &changedProperties,
	                       const QStringList &invalidatedProperties);

private:
	QDBusAbstractAdaptor* findAdaptor(const QString &interface);

};


#endif // DBUSPROPERTIES_H
