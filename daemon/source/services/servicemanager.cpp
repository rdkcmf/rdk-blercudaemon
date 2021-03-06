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

#include "servicemanager.h"
#include "dbus/blercucontroller_proxy.h"


ServiceManager::ServiceManager(const QDBusConnection &dbusConn)
	: m_registeredServices(false)
	, m_dbusConn(dbusConn)
{

}

ServiceManager::~ServiceManager()
{

}

bool ServiceManager::registerAllServices()
{
	m_registeredServices = true;

#if defined(Q_OS_LINUX)
	if (m_controller) {

		if (!m_dbusProxy)
			m_dbusProxy = QSharedPointer<BleRcuControllerProxy>::create(m_dbusConn, m_controller);

		if (!m_dbusProxy->isRegisteredOnBus())
			m_dbusProxy->registerOnBus();

	}
#endif // defined(Q_OS_LINUX)

	return true;
}

void ServiceManager::unregisterAllServices()
{

#if defined(Q_OS_LINUX)
	if (m_dbusProxy)
		m_dbusProxy->unregisterFromBus();

#endif

}

void ServiceManager::setController(const QSharedPointer<BleRcuController> &controller)
{
	m_controller = controller;

#if defined(Q_OS_LINUX)
	if (m_registeredServices && m_controller) {

		if (!m_dbusProxy)
			m_dbusProxy = QSharedPointer<BleRcuControllerProxy>::create(m_dbusConn, m_controller);

		if (!m_dbusProxy->isRegisteredOnBus())
			m_dbusProxy->registerOnBus();

	}
#endif // defined(Q_OS_LINUX)

}

void ServiceManager::setIrDatabase(const QSharedPointer<IrDatabase> &irDatabase)
{
	m_irDatabase = irDatabase;

}

