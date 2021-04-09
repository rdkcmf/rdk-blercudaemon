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
//  blercuupgrade1_adaptor.h
//  SkyBluetoothRcu
//

#ifndef BLERCUUPGRADE1_ADAPTOR_H
#define BLERCUUPGRADE1_ADAPTOR_H

#include "dbus/dbusabstractadaptor.h"

#include <QObject>
#include <QtDBus>


class BleRcuDevice;

class BleRcuUpgrade1Adaptor : public DBusAbstractAdaptor
{
	Q_OBJECT
	Q_CLASSINFO("D-Bus Interface", "com.sky.blercu.Upgrade1")
	Q_CLASSINFO("D-Bus Introspection", ""
	            "  <interface name=\"com.sky.blercu.Upgrade1\">\n"
	            "    <method name=\"StartUpgrade\">\n"
	            "      <arg direction=\"in\" type=\"h\" name=\"file\"/>\n"
	            "    </method>\n"
	            "    <method name=\"CancelUpgrade\">\n"
	            "    </method>\n"
	            "    <signal name=\"UpgradeError\">\n"
	            "      <arg type=\"s\" name=\"reason\"/>\n"
	            "    </signal>\n"
	            "    <property name=\"Upgrading\" type=\"b\" access=\"read\">\n"
	            "    </property>\n"
	            "    <property name=\"Progress\" type=\"i\" access=\"read\">\n"
	            "    </property>\n"
	            "  </interface>\n"
	            "")

public:
	Q_PROPERTY(bool Upgrading READ upgrading)
	Q_PROPERTY(bool Progress READ progress)

public:
	BleRcuUpgrade1Adaptor(const QSharedPointer<BleRcuDevice> &device,
	                      const QDBusObjectPath &objPath,
	                      QObject *parent);
	virtual ~BleRcuUpgrade1Adaptor();

public slots:
	void StartUpgrade(const QDBusUnixFileDescriptor &file,
	                  const QDBusMessage &message);

	void CancelUpgrade(const QDBusMessage &message);

signals:
	void UpgradeError(const QString &reason);

public:
	bool upgrading() const;
	qint32 progress() const;

private slots:
	void onUpgradingChanged(bool upgrading);
	void onProgressChanged(int progress);

private:
	const QSharedPointer<BleRcuDevice> m_device;
	const QDBusObjectPath m_dbusObjPath;
};

#endif // !defined(BLERCUUPGRADE1_ADAPTOR_H)

