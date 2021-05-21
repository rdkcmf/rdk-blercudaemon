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
//  blercucontroller_proxy.cpp
//  SkyBluetoothRcu
//

#include "blercucontroller_proxy.h"
#include "blercucontroller1_adaptor.h"
#include "blercuvoice1_adaptor.h"
#include "blercudebug1_adaptor.h"
#include "blercuhcicapture1_adaptor.h"
#include "blercu/blercucontroller.h"
#include "blercu/blercudevice.h"
#include "blercudevice_proxy.h"
#include "dbus/dbusabstractadaptor.h"
#include "utils/logging.h"



BleRcuControllerProxy::BleRcuControllerProxy(const QDBusConnection &dbusConn,
                                             const QSharedPointer<BleRcuController> &controller,
                                             QObject *parent)
	: QObject(parent)
	, m_dbusConn(dbusConn)
	, m_controller(controller)
	, m_dbusObjectPath("/com/sky/blercu/controller")
{

	// connect to the device added and removed signals, we use these to create
	// BleRcuDeviceProxy objects
	QObject::connect(controller.data(), &BleRcuController::managedDeviceAdded,
	                 this, &BleRcuControllerProxy::onDeviceAdded);
	QObject::connect(controller.data(), &BleRcuController::managedDeviceRemoved,
	                 this, &BleRcuControllerProxy::onDeviceRemoved);


	// create and attach the dbus adaptor for the controller interface to
	// ourselves
	m_dbusAdaptors.append( new BleRcuController1Adaptor(controller, m_dbusObjectPath, this) );

	// this interface was added for the RDK build, it is a simplified dbus API
	// that just supports 'start recording' and 'get recording stats' APIs
	m_dbusAdaptors.append( new BleRcuVoice1Adaptor(controller, this) );


#if (AI_BUILD_TYPE == AI_DEBUG)
	// create and attach the dbus adaptor for the debug interface(s) to ourselves
	m_dbusAdaptors.append( new BleRcuDebug1Adaptor(this, controller) );
	m_dbusAdaptors.append( new BleRcuHciCapture1Adaptor(this, m_dbusObjectPath, -1) );
#endif


}

BleRcuControllerProxy::~BleRcuControllerProxy()
{
	// clean-up all attached proxy devices
	for (BleRcuDeviceProxy *proxy : m_proxyDevices) {
		if (proxy) {
			proxy->unregisterFromBus(m_dbusConn);
			delete proxy;
		}
	}

	m_proxyDevices.clear();
}

// -----------------------------------------------------------------------------
/*!
	Returns \c true if this object has been registered on dbus.

	\see registerOnDBus()
 */
bool BleRcuControllerProxy::isRegisteredOnBus() const
{
	return (m_dbusConn.objectRegisteredAt(m_dbusObjectPath.path()) == this);
}

// -----------------------------------------------------------------------------
/*!
	Registers the object on the supplied \a dbusConn, this adds the following
	dbus interfaces; \c com.sky.blercu.Device1 and \c com.sky.blercu.Infrared1
	In addition the \c com.sky.blercu.Upgrade1 may be added on debug builds and
	for certain RCU types.

 */
bool BleRcuControllerProxy::registerOnBus()
{
	// sanity check we haven't already registered this object
	if (isRegisteredOnBus()) {
		qWarning("already registered dbus object");
		return false;
	}

	// strip the const'ness off the connection (bit of a hack)
	if (!m_dbusConn.registerObject(m_dbusObjectPath.path(), this)) {
		qError("failed to register blercudevice adaptor object(s)");
		return false;
	}

	// tell all the adaptors that they're now registered on the bus, this is
	// used for property change notifications
	for (DBusAbstractAdaptor *adaptor : m_dbusAdaptors)
		adaptor->registerConnection(m_dbusConn);


	// we are now ready so emit a 'Ready' signal over dbus letting everyone know
	BleRcuController1Adaptor *adaptor = findChild<BleRcuController1Adaptor*>();
	if (!adaptor)
		qWarning("failed to find BleRcuController1Adaptor child");
	else
		emit adaptor->Ready();

	return true;
}

// -----------------------------------------------------------------------------
/*!
	Unregisters the this object from the \a dbusConn dbus.

 */
bool BleRcuControllerProxy::unregisterFromBus()
{
	// sanity check we haven't already registered this object
	if (!isRegisteredOnBus()) {
		qWarning("device is not registered on dbus");
		return false;
	}

	// tell all the adaptors that they're now registered on the bus, this is
	// used for property change notifications
	for (DBusAbstractAdaptor *adaptor : m_dbusAdaptors)
		adaptor->unregisterConnection(m_dbusConn);

	// strip the const'ness off the connection (bit of a hack)
	m_dbusConn.unregisterObject(m_dbusObjectPath.path());

	return true;
}

// -----------------------------------------------------------------------------
/*!
	\internal


 */
void BleRcuControllerProxy::onDeviceAdded(const BleAddress &address)
{
	// for the device added signal we also want to broadcast the dbus object path
	const QSharedPointer<BleRcuDevice> device = m_controller->managedDevice(address);
	if (!device || !device->isValid()) {
		qError() << "odd, failed to find" << address << "in added slot";
		return;
	}

	// check we don't already have a proxy
	if (Q_UNLIKELY(m_proxyDevices.contains(address))) {
		qWarning("already have a proxy device for address %s",
		         qPrintable(address.toString()));
		return;
	}

	// create a new proxy object wrapping the device
	BleRcuDeviceProxy *proxy = new BleRcuDeviceProxy(device);
	proxy->registerOnBus(m_dbusConn);

	m_proxyDevices.insert(address, proxy);
}

// -----------------------------------------------------------------------------
/*!
	\internal


 */
void BleRcuControllerProxy::onDeviceRemoved(const BleAddress &address)
{
	// remove from the map
	BleRcuDeviceProxy *proxy = m_proxyDevices.take(address);
	if (!proxy) {
		qWarning("no proxy device for address %s", qPrintable(address.toString()));
		return;
	}

	// unregister the proxy from dbus and delete it
	proxy->unregisterFromBus(m_dbusConn);
	delete proxy;
}

