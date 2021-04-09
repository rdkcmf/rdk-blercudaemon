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
//  bluezdeviceinterface.h
//  SkyBluetoothRcu
//

#ifndef BLUEZDEVICEINTERFACE_H
#define BLUEZDEVICEINTERFACE_H

#include "dbus/dbusabstractinterface.h"

#include <QObject>
#include <QByteArray>
#include <QList>
#include <QMap>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <QAtomicInteger>

#include <QtDBus>



typedef QMap<quint16, QDBusVariant> ManufacturerDataMap;
Q_DECLARE_METATYPE(ManufacturerDataMap)



// -----------------------------------------------------------------------------
/**
 *	@class BluezDeviceInterface
 *	@brief Proxy class for dbus interface org.bluez.Device1
 */
class BluezDeviceInterface : public DBusAbstractInterface
{
	Q_OBJECT

public:
	static inline const char *staticInterfaceName()
	{ return "org.bluez.Device1"; }

public:
	BluezDeviceInterface(const QString &service, const QString &path, const QDBusConnection &connection, QObject *parent = nullptr)
		: DBusAbstractInterface(service, path, staticInterfaceName(), connection, parent)
	{
		// register the type with dbus once
		static QAtomicInteger<bool> registered(false);
		if (registered.testAndSetAcquire(false, true))
			qDBusRegisterMetaType<ManufacturerDataMap>();
	}

	~BluezDeviceInterface() final = default;

	Q_PROPERTY(QDBusObjectPath Adapter READ adapter)
	inline QDBusObjectPath adapter() const
	{ return qvariant_cast< QDBusObjectPath >(property("Adapter")); }

	Q_PROPERTY(QString Address READ address)
	inline QString address() const
	{ return qvariant_cast< QString >(property("Address")); }

	Q_PROPERTY(QString Alias READ alias WRITE setAlias NOTIFY aliasChanged)
	inline QString alias() const
	{ return qvariant_cast< QString >(property("Alias")); }
	inline void setAlias(const QString &value)
	{ setProperty("Alias", QVariant::fromValue(value)); }

	Q_PROPERTY(ushort Appearance READ appearance NOTIFY appearanceChanged)
	inline ushort appearance() const
	{ return qvariant_cast< ushort >(property("Appearance")); }

	Q_PROPERTY(bool Blocked READ blocked WRITE setBlocked NOTIFY blockedChanged)
	inline bool blocked() const
	{ return qvariant_cast< bool >(property("Blocked")); }
	inline void setBlocked(bool value)
	{ setProperty("Blocked", QVariant::fromValue(value)); }

	Q_PROPERTY(bool Connected READ connected NOTIFY connectedChanged)
	inline bool connected() const
	{ return qvariant_cast< bool >(property("Connected")); }

	Q_PROPERTY(QString Icon READ icon)
	inline QString icon() const
	{ return qvariant_cast< QString >(property("Icon")); }

	Q_PROPERTY(bool LegacyPairing READ legacyPairing)
	inline bool legacyPairing() const
	{ return qvariant_cast< bool >(property("LegacyPairing")); }

	Q_PROPERTY(QString Modalias READ modAlias NOTIFY modaliasChanged)
	inline QString modAlias() const
	{ return qvariant_cast< QString >(property("Modalias")); }

	Q_PROPERTY(QString Name READ name NOTIFY nameChanged)
	inline QString name() const
	{ return qvariant_cast< QString >(property("Name")); }

	Q_PROPERTY(bool Paired READ paired NOTIFY pairedChanged)
	inline bool paired() const
	{ return qvariant_cast< bool >(property("Paired")); }

	Q_PROPERTY(short RSSI READ rssi NOTIFY rssiChanged)
	inline short rssi() const
	{ return qvariant_cast< short >(property("RSSI")); }

	Q_PROPERTY(bool Trusted READ trusted WRITE setTrusted)
	inline bool trusted() const
	{ return qvariant_cast< bool >(property("Trusted")); }
	inline void setTrusted(bool value)
	{ setProperty("Trusted", QVariant::fromValue(value)); }

	Q_PROPERTY(QStringList UUIDs READ uuids)
	inline QStringList uuids() const
	{ return qvariant_cast< QStringList >(property("UUIDs")); }

	Q_PROPERTY(quint32 Class READ deviceClass)
	inline quint32 deviceClass() const
	{ return qvariant_cast< quint32 >(property("Class")); }

	Q_PROPERTY(bool ServicesResolved READ servicesResolved NOTIFY servicesResolvedChanged)
	inline bool servicesResolved() const
	{ return qvariant_cast< bool >(property("ServicesResolved")); }

	Q_PROPERTY(qint16 TxPower READ txPower NOTIFY txPowerChanged)
	inline qint16 txPower() const
	{ return qvariant_cast< qint16 >(property("TxPower")); }

	Q_PROPERTY(ManufacturerDataMap ManufacturerData READ manufacturerData NOTIFY manufacturerDataChanged)
	inline ManufacturerDataMap manufacturerData() const
	{ return qvariant_cast< ManufacturerDataMap >(property("ManufacturerData")); }

	Q_PROPERTY(QByteArray AdvertisingFlags READ advertisingFlags NOTIFY advertisingFlagsChanged)
	inline QByteArray advertisingFlags() const
	{ return qvariant_cast< QByteArray >(property("AdvertisingFlags")); }

public slots:
	inline QDBusPendingReply<> CancelPairing()
	{
		QList<QVariant> argumentList;
		return asyncCallWithArgumentList(QStringLiteral("CancelPairing"), argumentList);
	}

	inline QDBusPendingReply<> Connect()
	{
		QList<QVariant> argumentList;
		return asyncCallWithArgumentList(QStringLiteral("Connect"), argumentList);
	}

	inline QDBusPendingReply<> ConnectProfile(const QString &UUID)
	{
		QList<QVariant> argumentList;
		argumentList << QVariant::fromValue(UUID);
		return asyncCallWithArgumentList(QStringLiteral("ConnectProfile"), argumentList);
	}

	inline QDBusPendingReply<> Disconnect()
	{
		QList<QVariant> argumentList;
		return asyncCallWithArgumentList(QStringLiteral("Disconnect"), argumentList);
	}

	inline QDBusPendingReply<> DisconnectProfile(const QString &UUID)
	{
		QList<QVariant> argumentList;
		argumentList << QVariant::fromValue(UUID);
		return asyncCallWithArgumentList(QStringLiteral("DisconnectProfile"), argumentList);
	}

	inline QDBusPendingReply<> Pair()
	{
		QList<QVariant> argumentList;
		return asyncCallWithArgumentList(QStringLiteral("Pair"), argumentList);
	}

signals:
	void aliasChanged(const QString &newAlias);
	void appearanceChanged(quint16 newAppearance);
	void blockedChanged(bool blocked);
	void connectedChanged(bool connected);
	void pairedChanged(bool paired);
	void nameChanged(const QString &newName);
	void modaliasChanged(const QString &newModAlias);
	void rssiChanged(qint16 newRssi);
	void servicesResolvedChanged(bool servicesResolved);
	void txPowerChanged(qint16 txPower);
	void manufacturerDataChanged(const ManufacturerDataMap &manufacturerData);
	void advertisingFlagsChanged(const QByteArray &advertisingFlags);

};

#endif // !defined(BLUEZDEVICEINTERFACE_H)
