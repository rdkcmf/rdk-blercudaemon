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
//  blercuhcicapture1_adaptor.h
//  SkyBluetoothRcu
//

#ifndef BLERCUHCICAPTURE1_ADAPTOR_H
#define BLERCUHCICAPTURE1_ADAPTOR_H

#include "dbus/dbusabstractadaptor.h"

#include "utils/filedescriptor.h"

#include <QObject>
#include <QString>

#include <QtDBus>

class HciMonitor;

class BleRcuHciCapture1Adaptor : public DBusAbstractAdaptor
{
	Q_OBJECT
	Q_CLASSINFO("D-Bus Interface", "com.sky.blercu.HciCapture1")
	Q_CLASSINFO("D-Bus Introspection", ""
	            "  <interface name=\"com.sky.blercu.HciCapture1\">\n"
	            "    <method name=\"Enable\">\n"
	            "    </method>\n"
	            "    <method name=\"Disable\">\n"
	            "    </method>\n"
	            "    <method name=\"Clear\">\n"
	            "    </method>\n"
	            "    <method name=\"Dump\">\n"
	            "      <arg direction=\"in\" type=\"h\" name=\"file\"/>\n"
	            "    </method>\n"
	            "    <property name=\"Capturing\" type=\"b\" access=\"read\">\n"
	            "    </property>\n"
	            "  </interface>\n"
	            "")

public:
	Q_PROPERTY(bool Capturing READ isCapturing)

public:
	BleRcuHciCapture1Adaptor(QObject *parent,
	                         const QDBusObjectPath &objPath,
	                         int networkNamespaceFd);
	~BleRcuHciCapture1Adaptor() final;

public:
	bool isCapturing() const;

public slots:
	void Enable(const QDBusMessage &message);
	void Disable(const QDBusMessage &message);
	void Clear(const QDBusMessage &message);
	void Dump(QDBusUnixFileDescriptor file, const QDBusMessage &message);

private:
	const QDBusObjectPath m_dbusObjPath;
	const FileDescriptor m_networkNamespace;
	HciMonitor* m_hciMonitor;

};

#endif // !defined(BLERCUHCICAPTURE1_ADAPTOR_H)

