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

#ifndef BLERCUDEBUG1_INTERFACE_H
#define BLERCUDEBUG1_INTERFACE_H

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
 * Proxy class for interface com.sky.BleRcuDebug1
 */
class ComSkyBleRcuDebug1Interface: public DBusAbstractInterface
{
	Q_OBJECT
public:
	static inline const char *staticInterfaceName()
	{ return "com.sky.blercu.Debug1"; }

public:
	ComSkyBleRcuDebug1Interface(const QString &service, const QString &path, const QDBusConnection &connection, QObject *parent = 0);

	~ComSkyBleRcuDebug1Interface() final;

	Q_PROPERTY(bool LogToConsole READ logToConsole WRITE setLogToConsole)
	inline bool logToConsole() const
	{ return qvariant_cast< bool >(property("LogToConsole")); }
	inline void setLogToConsole(bool value)
	{ setProperty("LogToConsole", QVariant::fromValue(value)); }

	Q_PROPERTY(bool LogToEthanLog READ logToEthanLog WRITE setLogToEthanLog)
	inline bool logToEthanLog() const
	{ return qvariant_cast< bool >(property("LogToEthanLog")); }
	inline void setLogToEthanLog(bool value)
	{ setProperty("LogToEthanLog", QVariant::fromValue(value)); }

	Q_PROPERTY(bool LogToSysLog READ logToSysLog WRITE setLogToSysLog)
	inline bool logToSysLog() const
	{ return qvariant_cast< bool >(property("LogToSysLog")); }
	inline void setLogToSysLog(bool value)
	{ setProperty("LogToSysLog", QVariant::fromValue(value)); }

	Q_PROPERTY(quint32 LogLevels READ logLevels WRITE setLogLevels)
	inline quint32 logLevels() const
	{ return qvariant_cast< quint32 >(property("LogLevels")); }
	inline void setLogLevels(quint32 value)
	{ setProperty("LogLevels", QVariant::fromValue(value)); }


public Q_SLOTS: // METHODS

Q_SIGNALS: // SIGNALS
};

namespace com {
	namespace sky {
		typedef ::ComSkyBleRcuDebug1Interface BleRcuDebug1;
	}
}
#endif
