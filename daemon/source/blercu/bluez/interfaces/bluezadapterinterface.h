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
//  bluezadapterinterface.h
//  SkyBluetoothRcu
//

#ifndef BLUEZADAPTERINTERFACE_H
#define BLUEZADAPTERINTERFACE_H

#include "dbus/dbusabstractinterface.h"

#include <QObject>
#include <QByteArray>
#include <QList>
#include <QMap>
#include <QString>
#include <QStringList>
#include <QVariant>

#include <QtDBus>

// -----------------------------------------------------------------------------
/**
 *	@class BluezAdapterInterface
 *	@brief Proxy class for dbus interface org.bluez.Adapter1
 */
class BluezAdapterInterface : public DBusAbstractInterface
{
	Q_OBJECT

public:
	static inline const char *staticInterfaceName()
	{ return "org.bluez.Adapter1"; }

public:
	BluezAdapterInterface(const QString &service, const QString &path, const QDBusConnection &connection, QObject *parent = nullptr)
		: DBusAbstractInterface(service, path, staticInterfaceName(), connection, parent)
	{ }

	~BluezAdapterInterface() final = default;

	Q_PROPERTY(QString Address READ address)
	inline QString address() const
	{ return qvariant_cast< QString >(property("Address")); }

	Q_PROPERTY(QString Alias READ alias WRITE setAlias NOTIFY aliasChanged)
	inline QString alias() const
	{ return qvariant_cast< QString >(property("Alias")); }
	inline void setAlias(const QString &value)
	{ setProperty("Alias", QVariant::fromValue(value)); }

	Q_PROPERTY(quint32 Class READ deviceClass NOTIFY deviceClassChanged)
	inline quint32 deviceClass() const
	{ return qvariant_cast< quint32 >(property("Class")); }

	Q_PROPERTY(bool Discoverable READ discoverable WRITE setDiscoverable NOTIFY discoverableChanged)
	inline bool discoverable() const
	{ return qvariant_cast< bool >(property("Discoverable")); }
	inline void setDiscoverable(bool value)
	{ setProperty("Discoverable", QVariant::fromValue(value)); }

	Q_PROPERTY(uint DiscoverableTimeout READ discoverableTimeout WRITE setDiscoverableTimeout NOTIFY discoverableTimeoutChanged)
	inline uint discoverableTimeout() const
	{ return qvariant_cast< uint >(property("DiscoverableTimeout")); }
	inline void setDiscoverableTimeout(uint value)
	{ setProperty("DiscoverableTimeout", QVariant::fromValue(value)); }

	Q_PROPERTY(bool Discovering READ discovering NOTIFY discoveringChanged)
	inline bool discovering() const
	{ return qvariant_cast< bool >(property("Discovering")); }

	Q_PROPERTY(QString Modalias READ modAlias NOTIFY modAliasChanged)
	inline QString modAlias() const
	{ return qvariant_cast< QString >(property("Modalias")); }

	Q_PROPERTY(QString Name READ name NOTIFY nameChanged)
	inline QString name() const
	{ return qvariant_cast< QString >(property("Name")); }

	Q_PROPERTY(bool Pairable READ pairable WRITE setPairable NOTIFY pairableChanged)
	inline bool pairable() const
	{ return qvariant_cast< bool >(property("Pairable")); }
	inline void setPairable(bool value)
	{ setProperty("Pairable", QVariant::fromValue(value)); }

	Q_PROPERTY(quint32 PairableTimeout READ pairableTimeout WRITE setPairableTimeout NOTIFY pairableTimeoutChanged)
	inline quint32 pairableTimeout() const
	{ return qvariant_cast< quint32 >(property("PairableTimeout")); }
	inline void setPairableTimeout(quint32 value)
	{ setProperty("PairableTimeout", QVariant::fromValue(value)); }

	Q_PROPERTY(bool Powered READ powered WRITE setPowered NOTIFY poweredChanged)
	inline bool powered() const
	{ return qvariant_cast< bool >(property("Powered")); }
	inline void setPowered(bool value)
	{ setProperty("Powered", QVariant::fromValue(value)); }

	Q_PROPERTY(QStringList UUIDs READ uuids NOTIFY uuidsChanged)
	inline QStringList uuids() const
	{ return qvariant_cast< QStringList >(property("UUIDs")); }

public slots:
	inline QDBusPendingReply<> RemoveDevice(const QDBusObjectPath &device)
	{
		QList<QVariant> argumentList;
		argumentList << QVariant::fromValue(device);
		return asyncCallWithArgumentList(QStringLiteral("RemoveDevice"), argumentList);
	}

	inline QDBusPendingReply<> SetDiscoveryFilter(const QVariantMap &properties)
	{
		QList<QVariant> argumentList;
		argumentList << QVariant::fromValue(properties);
		return asyncCallWithArgumentList(QStringLiteral("SetDiscoveryFilter"), argumentList);
	}

	inline QDBusPendingReply<> StartDiscovery()
	{
		QList<QVariant> argumentList;
		return asyncCallWithArgumentList(QStringLiteral("StartDiscovery"), argumentList);
	}

	inline QDBusPendingReply<> StopDiscovery()
	{
		QList<QVariant> argumentList;
		return asyncCallWithArgumentList(QStringLiteral("StopDiscovery"), argumentList);
	}

signals:
	void aliasChanged(const QString& newAlias);
	void deviceClassChanged(quint32 deviceClass);
	void discoverableChanged(bool discoverable);
	void discoverableTimeoutChanged(uint newDiscoverableTimeout);
	void discoveringChanged(bool discovering);
	void modAliasChanged(const QString& newModAlias);
	void nameChanged(const QString& newName);
	void pairableChanged(bool pairable);
	void pairableTimeoutChanged(uint newPairableTimeout);
	void poweredChanged(bool powered);
	void uuidsChanged(const QStringList &newUuids);

};

#endif // !defined(BLUEZADAPTERINTERFACE_H)
