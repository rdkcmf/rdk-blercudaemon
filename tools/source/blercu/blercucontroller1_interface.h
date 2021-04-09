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
#ifndef BLERCUCONTROLLER1_INTERFACE_H
#define BLERCUCONTROLLER1_INTERFACE_H

#include "dbus/dbusabstractinterface.h"

#include <QtCore/QObject>
#include <QtCore/QByteArray>
#include <QtCore/QList>
#include <QtCore/QMap>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QVariant>
#include <QtDBus/QtDBus>

/*
 * Proxy class for interface com.sky.BleRcuController1
 */
class ComSkyBleRcuController1Interface: public DBusAbstractInterface
{
	Q_OBJECT
public:
	static inline const char *staticInterfaceName()
	{ return "com.sky.blercu.Controller1"; }

public:
	ComSkyBleRcuController1Interface(const QString &service, const QString &path, const QDBusConnection &connection, QObject *parent = 0);

	~ComSkyBleRcuController1Interface() final;

	Q_PROPERTY(bool Pairing READ pairing NOTIFY pairingChanged)
	inline bool pairing() const
	{ return qvariant_cast< bool >(property("Pairing")); }

	Q_PROPERTY(quint8 PairingCode READ pairingCode)
	inline quint8 pairingCode() const
	{ return qvariant_cast< quint8 >(property("PairingCode")); }

	Q_PROPERTY(quint32 State READ state NOTIFY stateChanged)
	inline quint32 state() const
	{ return qvariant_cast< quint32 >(property("State")); }

public Q_SLOTS: // METHODS
	inline QDBusPendingReply<QList<QDBusObjectPath>> GetDevices()
	{
		QList<QVariant> argumentList;
		return asyncCallWithArgumentList(QStringLiteral("GetDevices"), argumentList);
	}

	inline QDBusPendingReply<> Shutdown()
	{
		QList<QVariant> argumentList;
		return asyncCallWithArgumentList(QStringLiteral("Shutdown"), argumentList);
	}

	inline QDBusPendingReply<> IsReady()
	{
		QList<QVariant> argumentList;
		return asyncCallWithArgumentList(QStringLiteral("IsReady"), argumentList);
	}

	inline QDBusPendingReply<> StartPairing(quint8 pairing_code)
	{
		QList<QVariant> argumentList;
		argumentList << QVariant::fromValue(pairing_code);
		return asyncCallWithArgumentList(QStringLiteral("StartPairing"), argumentList);
	}

	inline QDBusPendingReply<> CancelPairing()
	{
		QList<QVariant> argumentList;
		return asyncCallWithArgumentList(QStringLiteral("CancelPairing"), argumentList);
	}

	inline QDBusPendingReply<> StartScanning(quint32 timeout)
	{
		QList<QVariant> argumentList;
		argumentList << QVariant::fromValue(timeout);
		return asyncCallWithArgumentList(QStringLiteral("StartScanning"), argumentList);
	}

Q_SIGNALS: // SIGNALS
	void DeviceAdded(const QDBusObjectPath &path, const QString &address);
	void DeviceRemoved(const QDBusObjectPath &path, const QString &address);
	void Ready();

	void pairingChanged(bool pairing);
	void stateChanged(quint32 status);
};

namespace com {
  namespace sky {
	typedef ::ComSkyBleRcuController1Interface BleRcuController1;
  }
}
#endif
