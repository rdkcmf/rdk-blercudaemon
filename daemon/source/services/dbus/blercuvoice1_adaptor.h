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
//  blercuvoice1_adaptor.h
//  SkyBluetoothRcu
//

#ifndef BLERCUVOICE1_ADAPTOR_H
#define BLERCUVOICE1_ADAPTOR_H

#include "dbus/dbusabstractadaptor.h"

#include "blercu/bleservices/blercuaudioservice.h"
#include "blercu/blercuerror.h"

#include <QObject>
#include <QString>
#include <QSharedPointer>

#include <QtDBus>


class BleRcuController;
class BleRcuDevice;

class BleRcuVoice1Adaptor : public DBusAbstractAdaptor
{
Q_OBJECT
	Q_CLASSINFO("D-Bus Interface", "com.sky.blercu.Voice1")
	Q_CLASSINFO("D-Bus Introspection", ""
	                                   "  <interface name=\"com.sky.blercu.Voice1\">\n"
	                                   "    <method name=\"StartAudioStreaming\">\n"
	                                   "      <arg direction=\"in\" type=\"s\" name=\"bdaddr\"/>\n"
	                                   "      <arg direction=\"in\" type=\"u\" name=\"encoding\"/>\n"
	                                   "      <arg direction=\"out\" type=\"h\" name=\"stream\"/>\n"
	                                   "    <method name=\"GetAudioStatus\">\n"
	                                   "      <arg direction=\"in\" type=\"s\" name=\"bdaddr\"/>\n"
	                                   "      <arg direction=\"out\" type=\"u\" name=\"error_status\"/>\n"
	                                   "      <arg direction=\"out\" type=\"u\" name=\"packets_received\"/>\n"
	                                   "      <arg direction=\"out\" type=\"u\" name=\"packets_expected\"/>\n"
	                                   "    </method>\n"
	                                   "  </interface>\n"
	                                   "")

public:
	BleRcuVoice1Adaptor(const QSharedPointer<BleRcuController> &controller,
	                    QObject *parent);
	~BleRcuVoice1Adaptor() final;

public slots:
	void StartAudioStreaming(const QString &bdaddr, uint encoding, const QDBusMessage &message);
	void GetAudioStatus(const QString &bdaddr, const QDBusMessage &message);

private:
	QSharedPointer<BleRcuDevice> getDevice(const QString &bdaddr) const;

	static QList<QVariant> convertStatusInfo(const BleRcuAudioService::StatusInfo &info);
	static QList<QVariant> convertFileDescriptor(const FileDescriptor &fd);

private:
	const QSharedPointer<BleRcuController> m_controller;

};

#endif // !defined(BLERCUVOICE1_ADAPTOR_H)

