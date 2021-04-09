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
//  servicemanager.h
//  SkyBluetoothRcu
//

#ifndef SERVICEMANAGER_H
#define SERVICEMANAGER_H


#include <QSharedPointer>
#include <QDBusConnection>


class IrDatabase;
class BleRcuController;

class BleRcuControllerProxy;
#if QT_VERSION > QT_VERSION_CHECK(5, 4, 0)
class BleRcuASService;
#endif

class ServiceManager
{
public:
#if defined(Q_OS_ANDROID)
	ServiceManager();
#elif defined(Q_OS_LINUX)
	ServiceManager(const QDBusConnection &dbusConn);
#endif
	~ServiceManager();

	bool registerAllServices();
	void unregisterAllServices();

	void setController(const QSharedPointer<BleRcuController> &controller);
	void setIrDatabase(const QSharedPointer<IrDatabase> &irDatabase);

private:
	bool m_registeredServices;
	QSharedPointer<BleRcuController> m_controller;
	QSharedPointer<IrDatabase> m_irDatabase;

#if defined(Q_OS_LINUX)
	const QDBusConnection m_dbusConn;
	QSharedPointer<BleRcuControllerProxy> m_dbusProxy;
#if QT_VERSION > QT_VERSION_CHECK(5, 4, 0)
	QSharedPointer<BleRcuASService> m_asService;
#endif
#endif

};

#endif // SERVICEMANAGER_H
