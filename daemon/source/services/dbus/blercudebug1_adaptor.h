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
//  blercudebug1_adaptor.h
//  SkyBluetoothRcu
//

#ifndef BLERCUDEBUG1_ADAPTOR_H
#define BLERCUDEBUG1_ADAPTOR_H

#include "dbus/dbusabstractadaptor.h"

#include <QObject>
#include <QString>
#include <QList>
#include <QVariant>
#include <QByteArray>
#include <QSharedPointer>

#include <QtDBus>

class BleRcuController;

class BleRcuDebug1Adaptor final : public DBusAbstractAdaptor
{
	Q_OBJECT
	Q_CLASSINFO("D-Bus Interface", "com.sky.blercu.Debug1")
	Q_CLASSINFO("D-Bus Introspection", ""
	            "  <interface name=\"com.sky.blercu.Debug1\">\n"
	            "    <property name=\"LogToConsole\" type=\"b\" access=\"readwrite\">\n"
	            "    </property>\n"
	            "    <property name=\"LogToEthanLog\" type=\"b\" access=\"readwrite\">\n"
	            "    </property>\n"
	            "    <property name=\"LogToSysLog\" type=\"b\" access=\"readwrite\">\n"
	            "    </property>\n"
	            "    <property name=\"LogLevels\" type=\"u\" access=\"readwrite\">\n"
	            "    </property>\n"
	            "  </interface>\n"
	            "")

public:
	Q_PROPERTY(bool LogToConsole READ isConsoleEnabled WRITE enableConsole)
	Q_PROPERTY(bool LogToEthanLog READ isEthanlogEnabled WRITE enableEthanlog)
	Q_PROPERTY(bool LogToSysLog READ isSyslogEnabled WRITE enableSyslog)
	Q_PROPERTY(quint32 LogLevels READ logLevels WRITE setLogLevels)

public:
	BleRcuDebug1Adaptor(QObject *parent,
	                    const QSharedPointer<BleRcuController> &controller);
	~BleRcuDebug1Adaptor() final = default;

public:
	bool isConsoleEnabled() const;
	void enableConsole(bool enable);

	bool isEthanlogEnabled() const;
	void enableEthanlog(bool enable);

	bool isSyslogEnabled() const;
	void enableSyslog(bool enable);

	quint32 logLevels() const;
	void setLogLevels(quint32 levels);

private:
	const QSharedPointer<BleRcuController> m_controller;

};

#endif // !defined(BLERCUDEBUG1_ADAPTOR_H)
