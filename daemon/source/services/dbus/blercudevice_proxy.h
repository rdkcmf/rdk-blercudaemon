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
//  blercudevice_proxy.h
//  SkyBluetoothRcu
//

#ifndef BLERCUDEVICE_PROXY_H
#define BLERCUDEVICE_PROXY_H

#include "utils/bleaddress.h"

#include <QObject>
#include <QList>
#include <QSharedPointer>
#include <QDBusConnection>
#include <QDBusObjectPath>
#include <QDBusContext>


class BleRcuDevice;
class DBusAbstractAdaptor;

class BleRcuDeviceProxy : public QObject
                        , protected QDBusContext
{
	Q_OBJECT

public:
	explicit BleRcuDeviceProxy(const QSharedPointer<BleRcuDevice> &device,
	                           QObject *parent = nullptr);
	~BleRcuDeviceProxy() final;

	bool isRegisteredOnBus(const QDBusConnection &dbusConn) const;
	bool registerOnBus(const QDBusConnection &dbusConn);
	bool unregisterFromBus(const QDBusConnection &dbusConn);

public:
	static QDBusObjectPath createDeviceObjectPath(const BleAddress &address);

private:
	const QSharedPointer<BleRcuDevice> m_device;
	const QDBusObjectPath m_dbusObjectPath;

	QList<DBusAbstractAdaptor*> m_dbusAdaptors;

};

#endif // BLERCUDEVICE_PROXY_H
