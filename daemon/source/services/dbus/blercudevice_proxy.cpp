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
//  blercudevice_proxy.cpp
//  SkyBluetoothRcu
//

#include "blercudevice_proxy.h"
#include "blercudevice1_adaptor.h"
#include "blercuinfrared1_adaptor.h"
#include "blercuupgrade1_adaptor.h"
#include "blercu/blercudevice.h"
#include "dbus/dbusabstractadaptor.h"
#include "utils/logging.h"


QDBusObjectPath BleRcuDeviceProxy::createDeviceObjectPath(const BleAddress &address)
{
	const QVarLengthArray<quint8, 6> bdaddrArray = address.toArray();
	const QString objPath = QString_asprintf("/com/sky/blercu/device_%02hhX_%02hhX_%02hhX_%02hhX_%02hhX_%02hhX",
	                                          bdaddrArray[0], bdaddrArray[1], bdaddrArray[2],
	                                          bdaddrArray[3], bdaddrArray[4], bdaddrArray[5]);

	return QDBusObjectPath(objPath);
}



BleRcuDeviceProxy::BleRcuDeviceProxy(const QSharedPointer<BleRcuDevice> &device,
                                     QObject *parent)
	: QObject(parent)
	, m_device(device)
	, m_dbusObjectPath(createDeviceObjectPath(device->address()))
{

	// create an dbus adaptor for this device, we are the parent of the adaptor
	// so it will be automatically destroyed when we are destructed
	m_dbusAdaptors.append( new BleRcuDevice1Adaptor(device, m_dbusObjectPath, this) );
	m_dbusAdaptors.append( new BleRcuInfrared1Adaptor(device, this) );

	// (for now?) only create the firmware upgrade service on debug builds and
	// only for EC10x rcus
#if (AI_BUILD_TYPE == AI_DEBUG)
	m_dbusAdaptors.append( new BleRcuUpgrade1Adaptor(device, m_dbusObjectPath, this) );
#endif

}

BleRcuDeviceProxy::~BleRcuDeviceProxy()
{
}

// -----------------------------------------------------------------------------
/*!
	\fn bool BleRcuDevice::isRegisteredOnBus() const

	Returns \c true if this object has been registered on dbus.

	\see registerOnDBus()
 */
bool BleRcuDeviceProxy::isRegisteredOnBus(const QDBusConnection &dbusConn) const
{
	return (dbusConn.objectRegisteredAt(m_dbusObjectPath.path()) == this);
}

// -----------------------------------------------------------------------------
/*!
	\fn bool BleRcuDevice::registerOnBus(const QDBusConnection &dbusConn)

	Registers the object on the supplied \a dbusConn, this adds the following
	dbus interfaces; \c com.sky.blercu.Device1 and \c com.sky.blercu.Infrared1
	In addition the \c com.sky.blercu.Upgrade1 may be added on debug builds and
	for certain RCU types.

 */
bool BleRcuDeviceProxy::registerOnBus(const QDBusConnection &dbusConn)
{
	// sanity check we haven't already registered this object
	if (isRegisteredOnBus(dbusConn)) {
		qWarning("already registered dbus object");
		return false;
	}

	// strip the const'ness off the connection (bit of a hack)
	QDBusConnection dbusConn_(dbusConn);

	if (!dbusConn_.registerObject(m_dbusObjectPath.path(), this)) {
		qError("failed to register blercudevice adaptor object(s)");
		return false;
	}

	// tell all the adaptors that they're now registered on the bus, this is
	// used for property change notifications
	for (DBusAbstractAdaptor *adaptor : m_dbusAdaptors)
		adaptor->registerConnection(dbusConn);

	return true;
}

// -----------------------------------------------------------------------------
/*!
	\fn bool BleRcuDevice::unregisterFromBus(const QDBusConnection &dbusConn)

	Unregisters the this object from the \a dbusConn dbus.

 */
bool BleRcuDeviceProxy::unregisterFromBus(const QDBusConnection &dbusConn)
{
	// sanity check we haven't already registered this object
	if (!isRegisteredOnBus(dbusConn)) {
		qWarning("device is not registered on dbus");
		return false;
	}

	// tell all the adaptors that they're now registered on the bus, this is
	// used for property change notifications
	for (DBusAbstractAdaptor *adaptor : m_dbusAdaptors)
		adaptor->unregisterConnection(dbusConn);

	// strip the const'ness off the connection (bit of a hack)
	QDBusConnection dbusConn_(dbusConn);
	dbusConn_.unregisterObject(m_dbusObjectPath.path());

	return true;
}

