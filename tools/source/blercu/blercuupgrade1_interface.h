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

#ifndef BLERCUUPGRADE1_INTERFACE_H
#define BLERCUUPGRADE1_INTERFACE_H

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
 * Proxy class for interface com.sky.BleRcuUpgrade1
 */
class ComSkyBleRcuUpgrade1Interface : public DBusAbstractInterface
{
	Q_OBJECT
public:
	static inline const char *staticInterfaceName()
	{ return "com.sky.blercu.Upgrade1"; }

public:
	ComSkyBleRcuUpgrade1Interface(const QString &service, const QString &path, const QDBusConnection &connection, QObject *parent = 0);

	~ComSkyBleRcuUpgrade1Interface() final;

	Q_PROPERTY(bool Upgrading READ upgrading NOTIFY upgradingChanged)
	inline bool upgrading() const
	{ return qvariant_cast< bool >(property("Upgrading")); }

	Q_PROPERTY(qint32 Progress READ progress NOTIFY progressChanged)
	inline qint32 progress() const
	{ return qvariant_cast< qint32 >(property("Progress")); }

public Q_SLOTS: // METHODS
	inline QDBusPendingReply<> StartUpgrade(const QDBusUnixFileDescriptor &file)
	{
		QList<QVariant> argumentList;
		argumentList << QVariant::fromValue(file);
		return asyncCallWithArgumentList(QStringLiteral("StartUpgrade"), argumentList);
	}

	inline QDBusPendingReply<> CancelUpgrade()
	{
		QList<QVariant> argumentList;
		return asyncCallWithArgumentList(QStringLiteral("CancelUpgrade"), argumentList);
	}

Q_SIGNALS: // SIGNALS
	void upgradingChanged(bool upgrading);
	void progressChanged(qint32 progress);

};

namespace com {
	namespace sky {
		typedef ::ComSkyBleRcuUpgrade1Interface BleRcuUpgrade1;
	}
}

#endif // !defined(BLERCUUPGRADE1_INTERFACE_H)
