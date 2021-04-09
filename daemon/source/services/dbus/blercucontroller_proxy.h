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
//  blercucontroller_proxy.h
//  SkyBluetoothRcu
//

#ifndef BLERCUCONTROLLER_PROXY_H
#define BLERCUCONTROLLER_PROXY_H

#include "utils/bleaddress.h"

#include <QObject>
#include <QMap>
#include <QList>
#include <QSharedPointer>
#include <QDBusConnection>
#include <QDBusObjectPath>
#include <QDBusContext>


class BleRcuController;
class BleRcuDeviceProxy;
class DBusAbstractAdaptor;


class BleRcuControllerProxy : public QObject
                            , protected QDBusContext
{
	Q_OBJECT

public:
	explicit BleRcuControllerProxy(const QDBusConnection &dbusConn,
	                               const QSharedPointer<BleRcuController> &controller,
	                               QObject *parent = nullptr);
	~BleRcuControllerProxy() final;

	bool isRegisteredOnBus() const;
	bool registerOnBus();
	bool unregisterFromBus();

private:
	void onDeviceAdded(const BleAddress &address);
	void onDeviceRemoved(const BleAddress &address);

private:
	QDBusConnection m_dbusConn;

	const QSharedPointer<BleRcuController> m_controller;
	const QDBusObjectPath m_dbusObjectPath;

	QList<DBusAbstractAdaptor*> m_dbusAdaptors;
	QMap<BleAddress, BleRcuDeviceProxy*> m_proxyDevices;

};

#endif // BLERCUCONTROLLER_PROXY_H
