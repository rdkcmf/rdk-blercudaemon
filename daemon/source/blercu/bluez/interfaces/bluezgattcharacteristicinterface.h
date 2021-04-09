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
//  bluezgattcharacteristicinterface.h
//  SkyBluetoothRcu
//

#ifndef BLUEZGATTCHARACTERISTICINTERFACE_H
#define BLUEZGATTCHARACTERISTICINTERFACE_H

#include "dbus/dbusabstractinterface.h"

#include <QObject>
#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <QVariantMap>

#include <QtDBus>


class BluezGattCharacteristicInterface : public DBusAbstractInterface
{
	Q_OBJECT

public:
	static inline const char *staticInterfaceName()
	{ return "org.bluez.GattCharacteristic1"; }

public:
	BluezGattCharacteristicInterface(const QString &service, const QString &path,
	                                 const QDBusConnection &connection,
	                                 QObject *parent = nullptr)
		: DBusAbstractInterface(service, path, staticInterfaceName(), connection, parent)
		, m_useNewApi(false)
	{ }

	~BluezGattCharacteristicInterface() final = default;

	Q_PROPERTY(QDBusObjectPath Service READ service)
	inline QDBusObjectPath service() const
	{ return qvariant_cast< QDBusObjectPath >(property("Service")); }

	Q_PROPERTY(QString UUID READ uuid)
	inline QString uuid() const
	{ return qvariant_cast< QString >(property("UUID")); }

	Q_PROPERTY(QByteArray Value READ value NOTIFY valueChanged)
	inline QByteArray value() const
	{ return qvariant_cast< QByteArray >(property("Value")); }

	Q_PROPERTY(bool WriteAcquired READ writeAcquired NOTIFY writeAcquiredChanged)
	inline bool writeAcquired() const
	{ return qvariant_cast< bool >(property("WriteAcquired")); }

	Q_PROPERTY(bool NotifyAcquired READ notifyAcquired NOTIFY notifyAcquiredChanged)
	inline bool notifyAcquired() const
	{ return qvariant_cast< bool >(property("NotifyAcquired")); }

	Q_PROPERTY(bool Notifying READ notifying NOTIFY notifyingChanged)
	inline bool notifying() const
	{ return qvariant_cast< bool >(property("Notifying")); }

	Q_PROPERTY(QStringList Flags READ flags)
	inline QStringList flags() const
	{ return qvariant_cast< QStringList >(property("Flags")); }

public:
	inline void useNewDBusApi(bool newApi)
	{
		m_useNewApi = newApi;
	}

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

	inline QDBusPendingReply<QDBusUnixFileDescriptor, quint16> AcquireWrite()
	{
		QList<QVariant> argumentList;
		if (m_useNewApi) {
			const QVariantMap options;
			argumentList << QVariant::fromValue(options);
		}
		return asyncCallWithArgumentList(QStringLiteral("AcquireWrite"), argumentList);
	}

	inline QDBusPendingReply<QDBusUnixFileDescriptor, quint16> AcquireNotify()
	{
		QList<QVariant> argumentList;
		if (m_useNewApi) {
			const QVariantMap options;
			argumentList << QVariant::fromValue(options);
		}
		return asyncCallWithArgumentList(QStringLiteral("AcquireNotify"), argumentList);
	}

	inline QDBusPendingReply<> StartNotify()
	{
		QList<QVariant> argumentList;
		return asyncCallWithArgumentList(QStringLiteral("StartNotify"), argumentList);
	}

	inline QDBusPendingReply<> StopNotify()
	{
		QList<QVariant> argumentList;
		return asyncCallWithArgumentList(QStringLiteral("StopNotify"), argumentList);
	}

signals:
	void valueChanged(const QByteArray &newValue);
	void writeAcquiredChanged(bool acquired);
	void notifyingChanged(bool notifying);
	void notifyAcquiredChanged(bool acquired);

private:
	bool m_useNewApi;
};

#endif // !defined(BLUEZGATTCHARACTERISTICINTERFACE_H)
