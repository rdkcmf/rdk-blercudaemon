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

#ifndef BLERCUHCICAPTURE1_INTERFACE_H
#define BLERCUHCICAPTURE1_INTERFACE_H

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
 * Proxy class for interface com.sky.blercu.HciCapture1
 */
class ComSkyBleRcuHciCapture1Interface : public DBusAbstractInterface
{
	Q_OBJECT
public:
	static inline const char *staticInterfaceName()
	{ return "com.sky.blercu.HciCapture1"; }

public:
	ComSkyBleRcuHciCapture1Interface(const QString &service, const QString &path, const QDBusConnection &connection, QObject *parent = 0);

	~ComSkyBleRcuHciCapture1Interface() final;

	Q_PROPERTY(bool Capturing READ isCapturing NOTIFY capturingChanged)
	inline bool isCapturing() const
	{ return qvariant_cast< bool >(property("Capturing")); }

public Q_SLOTS: // METHODS
	inline QDBusPendingReply<> Enable()
	{
		QList<QVariant> argumentList;
		return asyncCallWithArgumentList(QStringLiteral("Enable"), argumentList);
	}

	inline QDBusPendingReply<> Disable()
	{
		QList<QVariant> argumentList;
		return asyncCallWithArgumentList(QStringLiteral("Disable"), argumentList);
	}

	inline QDBusPendingReply<> Clear()
	{
		QList<QVariant> argumentList;
		return asyncCallWithArgumentList(QStringLiteral("Clear"), argumentList);
	}

	inline QDBusPendingReply<> Dump(const QDBusUnixFileDescriptor &file)
	{
		QList<QVariant> argumentList;
		argumentList << QVariant::fromValue(file);
		return asyncCallWithArgumentList(QStringLiteral("Dump"), argumentList);
	}


Q_SIGNALS: // SIGNALS
	void capturingChanged(bool capturing);

};

namespace com {
	namespace sky {
		namespace blercu {
			typedef ::ComSkyBleRcuHciCapture1Interface HciCapture1;
		}
	}
}

#endif // !defined(BLERCUHCICAPTURE1_INTERFACE_H)
