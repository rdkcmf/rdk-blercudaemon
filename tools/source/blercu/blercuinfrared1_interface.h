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

#ifndef BLERCUINFRARED1_INTERFACE_H
#define BLERCUINFRARED1_INTERFACE_H

#include "dbus/dbusabstractinterface.h"

#include <QtCore/QObject>
#include <QtCore/QByteArray>
#include <QtCore/QList>
#include <QtCore/QMap>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QVariant>
#include <QtDBus/QtDBus>

typedef QList<qint32> IrCodeList;

/*
 * Proxy class for interface com.sky.BleRcuInfrared1
 */
class ComSkyBleRcuInfrared1Interface: public DBusAbstractInterface
{
	Q_OBJECT
public:
	static inline const char *staticInterfaceName()
	{ return "com.sky.blercu.Infrared1"; }

public:
	ComSkyBleRcuInfrared1Interface(const QString &service, const QString &path, const QDBusConnection &connection, QObject *parent = 0);

	~ComSkyBleRcuInfrared1Interface() final;

public Q_SLOTS: // METHODS
	inline QDBusPendingReply<IrCodeList> GetCodes(const QString &manufacturer, const QString &model, uint flags)
	{
		QList<QVariant> argumentList;
		argumentList << QVariant::fromValue(manufacturer) << QVariant::fromValue(model) << QVariant::fromValue(flags);
		return asyncCallWithArgumentList(QStringLiteral("GetCodes"), argumentList);
	}

	inline QDBusPendingReply<IrCodeList> GetCodesFromEDID(const QByteArray &edid)
	{
		QList<QVariant> argumentList;
		argumentList << QVariant::fromValue(edid);
		return asyncCallWithArgumentList(QStringLiteral("GetCodesFromEDID"), argumentList);
	}

	inline QDBusPendingReply<quint64, QStringList> GetManufacturers(const QString &search, quint32 flags, qint64 offset, qint64 limit)
	{
		QList<QVariant> argumentList;
		argumentList << QVariant::fromValue(search) << QVariant::fromValue(flags) << QVariant::fromValue(offset) << QVariant::fromValue(limit);
		return asyncCallWithArgumentList(QStringLiteral("GetManufacturers"), argumentList);
	}

	inline QDBusPendingReply<quint64, QStringList> GetModels(const QString &manufacturer, const QString &search, uint flags, qint64 offset, qint64 limit)
	{
		QList<QVariant> argumentList;
		argumentList << QVariant::fromValue(manufacturer) << QVariant::fromValue(search) << QVariant::fromValue(flags) << QVariant::fromValue(offset) << QVariant::fromValue(limit);
		return asyncCallWithArgumentList(QStringLiteral("GetModels"), argumentList);
	}

Q_SIGNALS: // SIGNALS
};

namespace com {
  namespace sky {
	typedef ::ComSkyBleRcuInfrared1Interface BleRcuInfrared1;
  }
}
#endif
