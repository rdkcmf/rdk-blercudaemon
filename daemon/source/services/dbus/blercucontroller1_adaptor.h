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
//  blercucontroller1_adaptor.h
//  SkyBluetoothRcu
//

#ifndef BLERCUCONTROLLER1_ADAPTOR_H
#define BLERCUCONTROLLER1_ADAPTOR_H

#include "dbus/dbusabstractadaptor.h"
#include "blercu/blercucontroller.h"

#include <QObject>
#include <QtDBus>
#include <QSharedPointer>

class BleAddress;

class BleRcuController1Adaptor : public DBusAbstractAdaptor
{
	Q_OBJECT
	Q_CLASSINFO("D-Bus Interface", "com.sky.blercu.Controller1")
	Q_CLASSINFO("D-Bus Introspection", ""
	            "  <interface name=\"com.sky.blercu.Controller1\">\n"
	            "    <method name=\"Shutdown\"/>\n"
	            "    <method name=\"StartPairing\">\n"
	            "      <arg direction=\"in\" type=\"y\" name=\"pairing_code\"/>\n"
	            "    </method>\n"
	            "    <method name=\"StartPairingMacHash\">\n"
	            "      <arg direction=\"in\" type=\"y\" name=\"mac_hash\"/>\n"
	            "    </method>\n"
	            "    <method name=\"CancelPairing\"/>\n"
	            "    <method name=\"StartScanning\">\n"
	            "      <arg direction=\"in\" type=\"i\" name=\"timeout\"/>\n"
	            "    </method>\n"
	            "    <method name=\"GetDevices\">\n"
	            "      <arg direction=\"out\" type=\"ao\" name=\"devices\"/>\n"
	            "    </method>\n"
	            "    <method name=\"IsReady\">\n"
	            "      <annotation value=\"true\" name=\"org.freedesktop.DBus.Method.NoReply\"/>\n"
	            "    </method>\n"
	            "    <method name=\"Unpair\">\n"
	            "      <arg direction=\"in\" type=\"s\" name=\"address\"/>\n"
	            "    </method>\n"
	            "    <signal name=\"DeviceAdded\">\n"
	            "      <arg type=\"o\" name=\"path\"/>\n"
	            "      <arg type=\"s\" name=\"address\"/>\n"
	            "    </signal>\n"
	            "    <signal name=\"DeviceRemoved\">\n"
	            "      <arg type=\"o\" name=\"path\"/>\n"
	            "      <arg type=\"s\" name=\"address\"/>\n"
	            "    </signal>\n"
	            "    <signal name=\"Ready\"/>\n"
	            "    <property access=\"read\" type=\"b\" name=\"Pairing\"/>\n"
	            "    <property access=\"read\" type=\"y\" name=\"PairingCode\"/>\n"
	            "    <property access=\"read\" type=\"u\" name=\"State\"/>\n"
	            "  </interface>\n"
	            "")
public:
	Q_PROPERTY(bool Pairing READ pairing)
	Q_PROPERTY(quint8 PairingCode READ pairingCode)
	Q_PROPERTY(quint32 State READ state)

public:
	BleRcuController1Adaptor(const QSharedPointer<BleRcuController> &controller,
	                         const QDBusObjectPath &objPath,
	                         QObject *parent);
	virtual ~BleRcuController1Adaptor();

public:
	bool pairing() const;
	quint8 pairingCode() const;
	quint32 state() const;

public slots:
	void StartPairing(quint8 pairingCode, const QDBusMessage &message);
	void StartPairingMacHash(quint8 macHash, const QDBusMessage &message);
	void CancelPairing(const QDBusMessage &message);

	void StartScanning(quint32 timeout, const QDBusMessage &message);

	QList<QDBusObjectPath> GetDevices(const QDBusMessage &message);

	void Unpair(const QString &address, const QDBusMessage &message);

	Q_NOREPLY void IsReady();
	void Shutdown();

signals:
	void DeviceAdded(const QDBusObjectPath &path, const QString &address);
	void DeviceRemoved(const QDBusObjectPath &path, const QString &address);
	void Ready();

private:
	template <typename T>
	void emitPropertyChanged(const QString &propName, const T &propValue) const;

private:
	void onDeviceAdded(const BleAddress &address);
	void onDeviceRemoved(const BleAddress &address);
	void onPairingStateChanged(bool pairing);
	void onStateChanged(BleRcuController::State state);

private:
	const QSharedPointer<BleRcuController> m_controller;
	const QDBusObjectPath m_dbusObjPath;

};

#endif // BLERCUCONTROLLER1_ADAPTOR_H
