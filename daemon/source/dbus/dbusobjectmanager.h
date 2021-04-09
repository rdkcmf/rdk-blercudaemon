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
//  dbusobjectmanager.h
//  BleRcuDaemon
//

#ifndef DBUSOBJECTMANAGER_H
#define DBUSOBJECTMANAGER_H

#include <QObject>
#include <QString>
#include <QVariant>

#include <QDBusConnection>
#include <QDBusObjectPath>
#include <QDBusPendingReply>
#include <QDBusAbstractAdaptor>
#include <QDBusAbstractInterface>


typedef QMap<QString, QVariantMap> DBusInterfaceList;
typedef QMap<QDBusObjectPath, DBusInterfaceList> DBusManagedObjectList;

Q_DECLARE_METATYPE(DBusInterfaceList)
Q_DECLARE_METATYPE(DBusManagedObjectList)



class DBusObjectManagerInterface: public QDBusAbstractInterface
{
	Q_OBJECT

public:
	static inline const char *staticInterfaceName()
	{ return "org.freedesktop.DBus.ObjectManager"; }

public:
	DBusObjectManagerInterface(const QString &service, const QString &path,
	                           const QDBusConnection &connection,
	                           QObject *parent = nullptr);
	~DBusObjectManagerInterface();

public slots:
	inline QDBusPendingReply<DBusManagedObjectList> GetManagedObjects()
	{
		QList<QVariant> argumentList;
		return asyncCallWithArgumentList(QStringLiteral("GetManagedObjects"), argumentList);
	}

signals:
	void InterfacesAdded(const QDBusObjectPath &objectPath, DBusInterfaceList interfacesAndProperties);
	void InterfacesRemoved(const QDBusObjectPath &objectPath, const QStringList &interfaces);
};


class DBusObjectManagerAdaptor : public QDBusAbstractAdaptor
{
	Q_OBJECT

	Q_CLASSINFO("D-Bus Interface", "org.freedesktop.DBus.ObjectManager")
	Q_CLASSINFO("D-Bus Introspection", ""
	            "  <interface name=\"org.freedesktop.DBus.ObjectManager\">\n"
	            "    <method name=\"GetManagedObjects\">\n"
	            "      <arg direction=\"out\" type=\"a{oa{sa{sv}}}\" name=\"object_paths_interfaces_and_properties\"/>\n"
	            "    </method>\n"
	            "    <signal name=\"InterfacesAdded\">\n"
	            "      <arg type=\"o\" name=\"object_path\"/>\n"
	            "      <arg type=\"a{sa{sv}}\" name=\"interfaces_and_properties\"/>\n"
	            "    </signal>\n"
	            "    <signal name=\"InterfacesRemoved\">\n"
	            "      <arg type=\"o\" name=\"object_path\"/>\n"
	            "      <arg type=\"as\" name=\"interfaces\"/>\n"
	            "    </signal>\n"
	            "  </interface>\n"
	            "")
public:
	DBusObjectManagerAdaptor(QObject *parent);
	virtual ~DBusObjectManagerAdaptor();

public slots:
	DBusManagedObjectList GetManagedObjects();

signals:
	void InterfacesAdded(const QDBusObjectPath &object_path,
	                     DBusInterfaceList interfaces_and_properties);
	void InterfacesRemoved(const QDBusObjectPath &object_path,
	                       const QStringList &interfaces);

private:
	QVariantMap getAllProperties(const QDBusAbstractAdaptor* const adaptor) const;
	DBusInterfaceList getAllInterfaces(const QObject* const object) const;

};


#endif // !defined(DBUSOBJECTMANAGER_H)
