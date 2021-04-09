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
//  blercucontroller1_adaptor.cpp
//  SkyBluetoothRcu
//

#include "blercucontroller1_adaptor.h"
#include "blercudevice_proxy.h"
#include "blercu/blercucontroller.h"
#include "blercu/blercudevice.h"

#include "utils/logging.h"

#include <QCoreApplication>


BleRcuController1Adaptor::BleRcuController1Adaptor(const QSharedPointer<BleRcuController> &controller,
                                                   const QDBusObjectPath &objPath,
                                                   QObject *parent)
	: DBusAbstractAdaptor(parent)
	, m_controller(controller)
	, m_dbusObjPath(objPath)
{
	// don't auto relay signals, we do this manually
	setAutoRelaySignals(false);

	// connect to the device added and removed signals
	QObject::connect(m_controller.data(), &BleRcuController::managedDeviceAdded,
	                 this, &BleRcuController1Adaptor::onDeviceAdded);
	QObject::connect(m_controller.data(), &BleRcuController::managedDeviceRemoved,
	                 this, &BleRcuController1Adaptor::onDeviceRemoved);

	// connect to the pairing state change signals
	QObject::connect(m_controller.data(), &BleRcuController::pairingStateChanged,
	                 this, &BleRcuController1Adaptor::onPairingStateChanged);

	// connect to the controller state change signal
	QObject::connect(m_controller.data(), &BleRcuController::stateChanged,
	                 this, &BleRcuController1Adaptor::onStateChanged);

}

BleRcuController1Adaptor::~BleRcuController1Adaptor()
{
	// destructor
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Slot called by the \l{BleRcuController} when a new managed device is added.
	We hook this signal to emit another deviceAdded signal over dbus.

 */
void BleRcuController1Adaptor::onDeviceAdded(const BleAddress &address)
{
	// for the device added signal we also want to broadcast the dbus object path
	const QSharedPointer<const BleRcuDevice> device = m_controller->managedDevice(address);
	if (!device || !device->isValid()) {
		qError() << "odd, failed to find" << address << "in added slot";
		return;
	}

	// send out the com.sky.BleRcuController1.DeviceAdded dbus signal
	emit DeviceAdded(BleRcuDeviceProxy::createDeviceObjectPath(address),
	                 address.toString());
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Slot called by the \l{BleRcuController} when a new managed device is added.
	We hook this signal to emit another deviceRemoved signal over dbus.

 */
void BleRcuController1Adaptor::onDeviceRemoved(const BleAddress &address)
{
	// send out the com.sky.BleRcuController1.DeviceRemoved dbus signal
	emit DeviceRemoved(BleRcuDeviceProxy::createDeviceObjectPath(address),
	                   address.toString());
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Utility function to emit a org.freedesktop.DBus.Properties.PropertiesChanged
	signal over the dbus interface.

 */
template <typename T>
void BleRcuController1Adaptor::emitPropertyChanged(const QString &propName,
                                                   const T &propValue) const
{
	sendPropertyChangeNotification<T>(m_dbusObjPath.path(), propName, propValue);
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Slot called by the \l{BleRcuController} when the pairing state changes. We
	hook this point so we can send out a property changed signal for the
	'Pairing' property.

 */
void BleRcuController1Adaptor::onPairingStateChanged(bool pairing)
{
	emitPropertyChanged(QStringLiteral("Pairing"), pairing);
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Slot called by the \l{BleRcuController} when the state changes. We
	hook this point so we can send out a property changed signal for the
	'State' property.

 */
void BleRcuController1Adaptor::onStateChanged(BleRcuController::State state)
{
	qMilestone() << "onStateChanged(" << state << ")";
	emitPropertyChanged(QStringLiteral("State"),
	                    static_cast<quint32>(state));
}

// -----------------------------------------------------------------------------
/*!
	DBus get property call for com.sky.BleRcuController1.Pairing

 */
bool BleRcuController1Adaptor::pairing() const
{
	return m_controller->isPairing();
}

// -----------------------------------------------------------------------------
/*!
	DBus get property call for com.sky.BleRcuController1.PairingCode

 */
quint8 BleRcuController1Adaptor::pairingCode() const
{
	return m_controller->pairingCode();
}

// -----------------------------------------------------------------------------
/*!
	DBus get property call for com.sky.BleRcuController1.State

 */
quint32 BleRcuController1Adaptor::state() const
{
	return static_cast<quint32>(m_controller->state());
}

// -----------------------------------------------------------------------------
/*!
	DBus method call handler for com.sky.BleRcuController1.StartPairing

 */
void BleRcuController1Adaptor::StartPairing(quint8 pairingCode,
                                            const QDBusMessage &message)
{
	const quint8 filterByte = 0;

	// sanity check we're not already in the pairing state
	if (m_controller->isPairing()) {
		sendErrorReply(message, BleRcuError::errorString(BleRcuError::Busy),
		               QStringLiteral("Already in pairing state"));
		return;
	}

	// attempt to start pairing using the supplied code
	if (!m_controller->startPairing(filterByte, pairingCode)) {

		const BleRcuError error = m_controller->lastError();
		sendErrorReply(message, error.name(), error.message());

	}
}

// -----------------------------------------------------------------------------
/*!
	DBus method call handler for com.sky.BleRcuController1.StartPairingMacHash

 */
void BleRcuController1Adaptor::StartPairingMacHash(quint8 macHash,
                                                   const QDBusMessage &message)
{
	const quint8 filterByte = 0;

	// sanity check we're not already in the pairing state
	if (m_controller->isPairing()) {
		sendErrorReply(message, BleRcuError::errorString(BleRcuError::Busy),
		               QStringLiteral("Already in pairing state"));
		return;
	}

	// attempt to start pairing using the supplied code
	if (!m_controller->startPairingMacHash(filterByte, macHash)) {

		const BleRcuError error = m_controller->lastError();
		sendErrorReply(message, error.name(), error.message());

	}
}
// -----------------------------------------------------------------------------
/*!
	DBus method call handler for com.sky.BleRcuController1.CancelPairing

 */
void BleRcuController1Adaptor::CancelPairing(const QDBusMessage &message)
{
	// sanity check we're actually in the pairing state
	if (!m_controller->isPairing()) {
		sendErrorReply(message, BleRcuError::errorString(BleRcuError::General),
		               QStringLiteral("Not in pairing state"));
		return;
	}

	// cancel the pairing
	m_controller->cancelPairing();
}

// -----------------------------------------------------------------------------
/*!
	DBus method call handler for com.sky.BleRcuController1.StartScanning

 */
void BleRcuController1Adaptor::StartScanning(quint32 timeout,
                                             const QDBusMessage &message)
{
	// sanity check we're not already in the scanning state
	if (m_controller->isScanning()) {
		sendErrorReply(message, BleRcuError::errorString(BleRcuError::Busy),
		               QStringLiteral("Already in scanning state"));
		return;
	}

	// attempt to start scanning using the supplied timeout
	if (!m_controller->startScanning(timeout)) {

		const BleRcuError error = m_controller->lastError();
		sendErrorReply(message, error.name(), error.message());
	}
}

// -----------------------------------------------------------------------------
/*!
	DBus method call handler for com.sky.BleRcuController1.GetDevices

 */
QList<QDBusObjectPath> BleRcuController1Adaptor::GetDevices(const QDBusMessage &message)
{
	Q_UNUSED(message);

	QList<QDBusObjectPath> devicePaths;

	const QSet<BleAddress> addresses = m_controller->managedDevices();
	for (const BleAddress &address : addresses) {

		const QSharedPointer<const BleRcuDevice> device = m_controller->managedDevice(address);
		if (device && device->isValid()) {
			devicePaths.append(BleRcuDeviceProxy::createDeviceObjectPath(address));
		}
	}

	return devicePaths;
}

// -----------------------------------------------------------------------------
/*!
	DBus method call handler for com.sky.BleRcuController1.IsReady

 */
void BleRcuController1Adaptor::IsReady()
{
	// send out a com.sky.BleRcuController1.Ready signal
	emit Ready();
}

// -----------------------------------------------------------------------------
/*!
	DBus method call handler for com.sky.BleRcuController1.Shutdown

 */
void BleRcuController1Adaptor::Shutdown()
{
	// the following will post a quit message to the main event loop
	qApp->quit();
}


