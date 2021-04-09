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
//  bleconnparamchanger.cpp
//  BleRcuDaemon
//

#include "bleconnparamchanger.h"
#include "bleconnparamdevice.h"

#include "utils/hcisocket.h"
#include "utils/logging.h"



// -----------------------------------------------------------------------------
/*!
	\class BleConnParamChanger
	\brief Class that manages the bluetooth LE connection parameters for
	connected devices.

	Why do we need this?  It is an attempt to improve voice search on Ruwido
	RCUs by adjusting the poll interval for the bluetooth connection.  In a
	normal scenario it's the remote device (RCU) that sets the connection params,
	however this class monitors connections / disconnections and parameter
	update events from the kernel's HCI bluetooth driver and then on any
	change that doesn't match our desired parameters we request a change.
 
	In an ideal world we would have just updated the firmware on the RCUs,
	however there is an obvious reluctance to do this, so this is half way house
	of trying to manage the connection params without requiring remote device
	changes.


 */




BleConnParamChanger::BleConnParamChanger(const QSharedPointer<HciSocket> &hciSocket,
                                         int postConnectionTimeout,
                                         int postUpdateTimeout,
                                         int retryTimeout,
                                         int startupTimeout,
                                         QObject *parent)
	: QObject(parent)
	, m_hciSocket(hciSocket)
	, m_postConnectionTimeout(postConnectionTimeout)
	, m_postUpdateTimeout(postUpdateTimeout)
	, m_retryTimeout(retryTimeout)
	, m_startupTimeout(startupTimeout)
{
}

BleConnParamChanger::~BleConnParamChanger()
{
}

// -----------------------------------------------------------------------------
/*!
	Returns the connection parameters that will be set for all connected devices
	that have OUI that matches \a deviceOui.

 */
BleConnectionParameters BleConnParamChanger::connectionParamsFor(quint32 deviceOui) const
{
	QMutexLocker locker(&m_lock);

	return m_desiredParams[deviceOui];
}

// -----------------------------------------------------------------------------
/*!
	Sets the connection parameters that will be set for all connected devices
	that have OUI that matches \a deviceOui.

 */
bool BleConnParamChanger::setConnectionParamsFor(quint32 deviceOui,
                                                 const BleConnectionParameters &params)
{
	QMutexLocker locker(&m_lock);

	m_desiredParams[deviceOui] = params;

	// TODO: check

	return true;
}

// -----------------------------------------------------------------------------
/*!
	Starts the connection parameter changer by creating an \l{HciSocket} object
	and connecting to it's connection changed parameters.

	This will also get the list of currently connected devices and request
	all of them to have their params changed. This is done even if the
	connected devices already have the right params, because there is no way
	to query the current params from the driver.

 */
bool BleConnParamChanger::start()
{
	QMutexLocker locker(&m_lock);

	if (!m_hciSocket || !m_hciSocket->isValid()) {
		qWarning("hci socket is invalid");
		return false;
	}

	QObject::connect(m_hciSocket.data(), &HciSocket::connectionCompleted,
	                 this, &BleConnParamChanger::onConnectionCompleted);
	QObject::connect(m_hciSocket.data(), &HciSocket::connectionUpdated,
	                 this, &BleConnParamChanger::onConnectionUpdated);
	QObject::connect(m_hciSocket.data(), &HciSocket::disconnectionComplete,
	                 this, &BleConnParamChanger::onDisconnectionCompleted);


	// get all the currently connected devices and then issue conn param
	// updates to them (there is no api to get the existing params so we have
	// to assume they're wrong and need updating)
	const QList<HciSocket::ConnectedDeviceInfo> deviceInfos = m_hciSocket->getConnectedDevices();
	for (const HciSocket::ConnectedDeviceInfo &deviceInfo : deviceInfos) {

		qInfo() << "found connected device" << deviceInfo;

		// check if we have some desired params for this device based on the
		// oui of the bdaddr
		const quint32 oui = deviceInfo.address.oui();
		if (!m_desiredParams.contains(oui)) {
			qInfo() << "device" << deviceInfo.address
			        << "doesn't require conn param management";
			continue;
		}

		// create an object to manage the ble connection params
		QSharedPointer<BleConnParamDevice> device =
			QSharedPointer<BleConnParamDevice>::create(m_hciSocket,
			                                           deviceInfo.handle,
			                                           deviceInfo.address,
			                                           m_desiredParams[oui],
			                                           m_postConnectionTimeout,
			                                           m_postUpdateTimeout,
			                                           m_retryTimeout);

		m_devices.insert(deviceInfo.handle, device);

		// trigger an connection parameter update in 1 seconds time
		device->triggerUpdate(m_startupTimeout);
	}

	return true;
}

// -----------------------------------------------------------------------------
/*!
	Stops the connection parameter changer by destroying the current
	\l{HciSocket} object and stopping any timers.

 */
void BleConnParamChanger::stop()
{
	QMutexLocker locker(&m_lock);

	m_devices.clear();

	if (m_hciSocket)
		QObject::disconnect(m_hciSocket.data(), 0, this, 0);

	m_devices.clear();
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Called by the \l{HciSocket} object when the driver tells us that a new
	connection has been completed.  The \a device argument is the BDADDR of
	the remote device that connected and \a params contains the current 
	connection parameters used for the new connection.

	We first check the connection parameters match our current desired
	parameters, if they don't then we (re)start the timer to fire in 5 seconds
	to update the parameters.  If they do match we stop the timer.

	Typically after an initial connection the remote device will issue a
	request to update it's parameters, this should happen within 5 seconds so
	it's likely the timer won't expire before onConnectionUpdated() is called.
	So this function is mainly used in case the remote device doesn't
	request an update.

 */
void BleConnParamChanger::onConnectionCompleted(quint16 handle,
                                                const BleAddress &address,
                                                const BleConnectionParameters &params)
{
	qInfo() << address << "(" << handle << ") connected with params" << params;

	QMutexLocker locker(&m_lock);

	// check if the oui of the new device indicates that we need to tweak
	// it's connection parameters
	const quint32 oui = address.oui();
	if (!m_desiredParams.contains(oui)) {
		qInfo() << "connected device doesn't require conn param management";
		return;
	}

	// if we don't already have this device add it
	QSharedPointer<BleConnParamDevice> device;
	if (m_devices.contains(handle)) {
		device = m_devices[handle];
	} else {
		device = QSharedPointer<BleConnParamDevice>::create(m_hciSocket,
		                                                    handle,
		                                                    address,
		                                                    m_desiredParams[oui],
		                                                    m_postConnectionTimeout,
		                                                    m_postUpdateTimeout,
		                                                    m_retryTimeout);
		m_devices[handle] = device;
	}

	// pass the event onto the device to handle
	device->onConnectionCompleted(params);
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Called by the \l{HciSocket} object when the driver tells us that the
	connection parameters have been updated.  The \a params contains the new
	connection parameters used for the new connection.

	We first check the connection parameters match our current desired
	parameters, if they don't then we (re)start the timer to fire in 2 seconds
	to update the parameters.  If they do match we stop the timer.

 */
void BleConnParamChanger::onConnectionUpdated(quint16 handle,
                                              const BleConnectionParameters &params)
{
	qInfo() << "connection parameters changed to" << params;

	QMutexLocker locker(&m_lock);

	// sanity check we know about the connected device with this handle, if
	// the device in question is not a ruwido remote then we probably won't
	if (!m_devices.contains(handle)) {
		qInfo("received a connection update event from unknown device with "
		      "handle %hu", handle);
		return;
	}

	// pass the event onto the device to process
	m_devices[handle]->onConnectionUpdated(params);
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Called by the \l{HciSocket} object when the driver tells us that the
	connected device has disconnected.

	This is just used to tidy up our internal map of handles to device ids.

 */
void BleConnParamChanger::onDisconnectionCompleted(quint16 handle,
                                                   HciSocket::HciStatus reason)
{
	qInfo() << "connection with handle" << handle
	        << "has disconnected due to" << reason;

	QMutexLocker locker(&m_lock);

	// sanity check we know about the connected device with this handle, if
	// the device in question is not a ruwido remote then we probably won't
	if (!m_devices.contains(handle)) {
		qInfo("received a disconnection event from unknown device with "
		      "handle %hu", handle);
		return;
	}

	// take the device from the map
	QSharedPointer<BleConnParamDevice> device = m_devices.take(handle);

	// pass the event onto the device to process
	device->onDisconnectionCompleted(reason);
}
