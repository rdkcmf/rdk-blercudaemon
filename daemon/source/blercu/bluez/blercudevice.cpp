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
//  blercudevice.cpp
//  SkyBluetoothRcu
//

#include "blercudevice_p.h"
#include "blegattprofile_p.h"
#include "blegattservice_p.h"
#include "blegattcharacteristic_p.h"
#include "blegattdescriptor_p.h"

#include "blercu/bleservices/blercuservices.h"
#include "blercu/bleservices/blercuservicesfactory.h"

#include "interfaces/bluezdeviceinterface.h"

#include "utils/logging.h"
#include "utils/inputdevicemanager.h"

#include <QCoreApplication>
#include <QDBusPendingReply>
#include <QAtomicInteger>
#include <QMetaObject>
#include <QAtomicInt>
#include <QThread>
#include <QTimer>

#include <functional>





BleRcuDeviceBluez::BleRcuDeviceBluez(const BleAddress &bdaddr,
                                     const QString &name,
                                     const QDBusConnection &bluezDBusConn,
                                     const QDBusObjectPath &bluezDBusPath,
                                     const QSharedPointer<BleRcuServicesFactory> &servicesFactory,
                                     QObject *parent)
	: BleRcuDevice(parent)
	, m_bluezObjectPath(bluezDBusPath)
	, m_address(bdaddr)
	, m_name(name)
	, m_lastConnectedState(false)
	, m_lastPairedState(false)
	, m_lastServicesResolvedState(false)
	, m_isPairing(false)
	, m_recoveryAttempts(0)
	, m_maxRecoveryAttempts(100)
{

	// initialise and start the state machine
	setupStateMachine();

	// initialise the dbus interface to bluez
	if (!init(bluezDBusConn, bluezDBusPath))
		return;

	// create an empty GATT profile, this will be populated when the services
	// are resolved
	m_gattProfile =
		QSharedPointer<BleGattProfileBluez>::create(bluezDBusConn,
		                                            bluezDBusPath);

	// create the services object for the device (this may fail if there is
	// no daemon on the box to support the given device, uei vs ruwido)
	m_services = servicesFactory->createServices(m_address, m_gattProfile, m_name);
	if (!m_services) {
		qWarning() << "failed to create services for" << bdaddr << ", name " << m_name;
		return;
	}

}

BleRcuDeviceBluez::~BleRcuDeviceBluez()
{
	// we don't have to do this here, the tear down will automatically free
	// the services, however may help with debugging
	if (m_services) {
		m_services->stop();
		m_services.clear();
	}

	//
	m_deviceProxy.clear();
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Initialise the proxy to the bluez device over dbus.


 */
bool BleRcuDeviceBluez::init(const QDBusConnection &bluezDBusConn,
                             const QDBusObjectPath &bluezDBusPath)
{
	// create a proxy to the 'org.bluez.Device1' interface on the device object
	m_deviceProxy =
		QSharedPointer<BluezDeviceInterface>::create(QStringLiteral("org.bluez"),
		                                             bluezDBusPath.path(),
		                                             bluezDBusConn);
	if (!m_deviceProxy || !m_deviceProxy->isValid()) {
		qError() << m_address  << "failed to create device proxy";
		m_deviceProxy.clear();
		return false;
	}


	// connect to the property change notifications from the daemon
	QObject::connect(m_deviceProxy.data(), &BluezDeviceInterface::connectedChanged,
	                 this, &BleRcuDeviceBluez::onDeviceConnectedChanged);
	QObject::connect(m_deviceProxy.data(), &BluezDeviceInterface::pairedChanged,
	                 this, &BleRcuDeviceBluez::onDevicePairedChanged);
	QObject::connect(m_deviceProxy.data(), &BluezDeviceInterface::servicesResolvedChanged,
	                 this, &BleRcuDeviceBluez::onDeviceServicesResolvedChanged);
	QObject::connect(m_deviceProxy.data(), &BluezDeviceInterface::nameChanged,
	                 this, &BleRcuDeviceBluez::onDeviceNameChanged);


	// schedule an event next time through the event loop to go and fetch the
	// initial state of the paired / connected and serviceResolved properties
#if QT_VERSION >= QT_VERSION_CHECK(5, 4, 0)
	QTimer::singleShot(0, this, &BleRcuDeviceBluez::getInitialDeviceProperties);
#else
	{
		QTimer* timer = new QTimer(this);
		QObject::connect(timer, &QTimer::timeout, this, &BleRcuDeviceBluez::getInitialDeviceProperties);
		QObject::connect(timer, &QTimer::timeout, timer, &QObject::deleteLater);
		timer->setSingleShot(true);
		timer->start(0);
	}
#endif

	return true;
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Called from the main event loop after we've been initialised, used to get
 	the initial states for the state machine.

 */
void BleRcuDeviceBluez::getInitialDeviceProperties()
{
	// get the initial connected and paired status and the name
	bool paired = m_deviceProxy->paired();
	bool connected = m_deviceProxy->connected();
	bool servicesResolved = m_deviceProxy->servicesResolved();


	// simulate the parameters change as if they were notifications, this will
	// move the state machine into the correct initial place
	// (nb: the order of the following calls is important)
	onDeviceConnectedChanged(connected);
	onDevicePairedChanged(paired);
	onDeviceServicesResolvedChanged(servicesResolved);
}

// -----------------------------------------------------------------------------
/*!
	Sends a pairing request to the bluez daemon.  Note this doesn't directly
	affect the state machine, this can be called from any state.  The state
	machine responses to events for bluez.

	The \a timeout value is the number of milliseconds to wait before issuing
	a 'cancel pairing' request to the bluez daemon.  The timer is automatically
	cancelled when we receive an event that shifts unpaired to paired.

	Lastly this is an asynchronous call, if an error happens with the pair
	request then the \a pairingError signal will be emitted.

 */
void BleRcuDeviceBluez::pair(int timeout)
{
	Q_UNUSED(timeout);

	QDBusPendingReply<> reply = m_deviceProxy->Pair();
	QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(reply, this);

	QObject::connect(watcher, &QDBusPendingCallWatcher::finished,
	                 this, &BleRcuDeviceBluez::onPairRequestReply);

	// set the flag, it may be cleared if the call fails
	m_isPairing = true;
}

// -----------------------------------------------------------------------------
/*!
	Called when we get a reply to the dbus message to pair this device. We
	use this to detect any errors that we just log and emit a signal to
	anyone who's interested.

 */
void BleRcuDeviceBluez::onPairRequestReply(QDBusPendingCallWatcher *call)
{
	QDBusPendingReply<> reply = *call;

	if (Q_UNLIKELY(reply.isError())) {

		m_isPairing = false;

		// an error occurred so log it, however from bluez 5.47 the bluez daemon
		// doesn't seem to send a reply on success, so ignore that error
		QDBusError error = reply.error();
		if (error.type() != QDBusError::NoReply)
			qError() << m_address << "pairing request failed with error" << error;

		// emit pairingError(m_address, error.message());

	} else {

		qDebug() << m_address << "pairing request successful";

		// TODO: start the timer to cancel the pairing after a certain amount of time

	}

	// free the pending reply object
	call->deleteLater();
}

// -----------------------------------------------------------------------------
/*!
	Sends a request to the bluez daemon to cancel paring.

 */
void BleRcuDeviceBluez::cancelPairing()
{
	qInfo() << "canceling pairing for device" << m_address;

	QDBusPendingReply<> reply = m_deviceProxy->CancelPairing();
	QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(reply, this);

	QObject::connect(watcher, &QDBusPendingCallWatcher::finished,
	                 this, &BleRcuDeviceBluez::onCancelPairingRequestReply);

	// regardless of whether the cancel succeeds we clear the isPairing flag
	m_isPairing = false;
}

// -----------------------------------------------------------------------------
/*!
	Called when we get a reply to the dbus message to cancel the pairing of this
	device. We use this to detect any errors that we just log and emit a signal
	to anyone who's interested.

 */
void BleRcuDeviceBluez::onCancelPairingRequestReply(QDBusPendingCallWatcher *call)
{
	QDBusPendingReply<> reply = *call;

	if (Q_UNLIKELY(reply.isError())) {

		// an error occurred so log it and emit an error signal
		QDBusError error = reply.error();
		qError() << m_address << "cancel pairing request failed with error" << error;

		// emit pairingError(m_address, error.message());

	} else {

		qDebug() << m_address << "cancel pairing request successful";

		// TODO: start the timer to cancel the pairing after a certain amount of time

	}

	// free the pending reply object
	call->deleteLater();
}

// -----------------------------------------------------------------------------
/*!



 */
void BleRcuDeviceBluez::setupStateMachine()
{
	// set the name of the state machine for logging
	m_stateMachine.setObjectName(QStringLiteral("DeviceStateMachine"));

	// on debug builds to milestone logging of this state machine
#if (AI_BUILD_TYPE == AI_DEBUG)
	m_stateMachine.setTransistionLogLevel(QtInfoMsg, &milestone());
#endif

	// add all the states and the super state groupings
	m_stateMachine.addState(IdleState, QStringLiteral("Idle"));
	m_stateMachine.addState(PairedState, QStringLiteral("Paired"));
	m_stateMachine.addState(ConnectedState, QStringLiteral("Connected"));
	m_stateMachine.addState(ResolvingServicesState, QStringLiteral("ResolvingServices"));

	m_stateMachine.addState(RecoverySuperState, QStringLiteral("RecoverySuperState"));
	m_stateMachine.addState(RecoverySuperState, RecoveryDisconnectingState, QStringLiteral("RecoveryDisconnecting"));
	m_stateMachine.addState(RecoverySuperState, RecoveryReconnectingState, QStringLiteral("RecoveryReconnecting"));

	m_stateMachine.addState(SetupSuperState, QStringLiteral("SetupSuperState"));
	m_stateMachine.addState(SetupSuperState, StartingServicesState, QStringLiteral("StartingServices"));
	m_stateMachine.addState(SetupSuperState, ReadyState, QStringLiteral("ReadyState"));


	// set the initial state of the state machine
	m_stateMachine.setInitialState(IdleState);

	// add the transitions:      From State            ->   Event                    ->  To State
	m_stateMachine.addTransition(IdleState,                 DevicePairedEvent,           PairedState);
	m_stateMachine.addTransition(IdleState,                 DeviceConnectedEvent,        ConnectedState);

	m_stateMachine.addTransition(PairedState,               DeviceUnpairedEvent,         IdleState);
	m_stateMachine.addTransition(PairedState,               DeviceConnectedEvent,        ResolvingServicesState);

	m_stateMachine.addTransition(ConnectedState,            DeviceDisconnectedEvent,     IdleState);
	m_stateMachine.addTransition(ConnectedState,            DevicePairedEvent,           ResolvingServicesState);

	m_stateMachine.addTransition(ResolvingServicesState,    DeviceDisconnectedEvent,     PairedState);
	m_stateMachine.addTransition(ResolvingServicesState,    DeviceUnpairedEvent,         ConnectedState);
	m_stateMachine.addTransition(ResolvingServicesState,    ServicesResolvedEvent,       StartingServicesState);
	m_stateMachine.addTransition(ResolvingServicesState,    ServicesResolveTimeoutEvent, RecoveryDisconnectingState);

	m_stateMachine.addTransition(RecoverySuperState,        DeviceUnpairedEvent,         ConnectedState);
	m_stateMachine.addTransition(RecoverySuperState,        DeviceConnectedEvent,        ResolvingServicesState);
	m_stateMachine.addTransition(RecoverySuperState,        ServicesResolvedEvent,       StartingServicesState);
	m_stateMachine.addTransition(RecoveryDisconnectingState,DeviceDisconnectedEvent,     RecoveryReconnectingState);

	m_stateMachine.addTransition(SetupSuperState,           ServicesNotResolvedEvent,    ResolvingServicesState);
	m_stateMachine.addTransition(SetupSuperState,           DeviceDisconnectedEvent,     PairedState);
	m_stateMachine.addTransition(SetupSuperState,           DeviceUnpairedEvent,         ConnectedState);

	m_stateMachine.addTransition(StartingServicesState,     ServicesStartedEvent,        ReadyState);


	// connect to the state entry and exit signals
	QObject::connect(&m_stateMachine, &StateMachine::entered,
	                 this, &BleRcuDeviceBluez::onEnteredState);
	QObject::connect(&m_stateMachine, &StateMachine::exited,
	                 this, &BleRcuDeviceBluez::onExitedState);


	// start the state machine
	m_stateMachine.start();
}

// -----------------------------------------------------------------------------
/*!
	Called when bluetoothd notifies us of a change in the device name.

	We simply pass this to the upper layers as a signal.

 */
void BleRcuDeviceBluez::onDeviceNameChanged(const QString &name)
{
	qInfo() << m_address << "device name changed from" << m_name << "to" << name;

	if (Q_LIKELY(m_name != name)) {
		m_name = name;
		emit nameChanged(m_name, BleRcuDevice::privateSignal());
	}
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Called when the connection status of the device changes.

	This is trigger for the state machine and therefore obvious the device may
	move into a new state as a result of this signal.
 */
void BleRcuDeviceBluez::onDeviceConnectedChanged(bool connected)
{
	// log a milestone on any change
	if (Q_LIKELY(connected != m_lastConnectedState)) {
#if (AI_BUILD_TYPE == AI_DEBUG)
		qMilestone() << m_address << (connected ? "connected" : "disconnected");
#else
		qLimitedProdLog("RCU %sconnected", connected ? "" : "dis");
#endif
		emit connectedChanged(connected, BleRcuDevice::privateSignal());

		m_lastConnectedState = connected;
	}

	// post an event to update the state machine
	if (connected)
		m_stateMachine.postEvent(DeviceConnectedEvent);
	else
		m_stateMachine.postEvent(DeviceDisconnectedEvent);
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Called when the paired status of the device changes.

	This is trigger for the state machine and therefore obvious the device may
	move into a new state as a result of this signal.
 */
void BleRcuDeviceBluez::onDevicePairedChanged(bool paired)
{
	// log a milestone on any change
	if (Q_LIKELY(paired != m_lastPairedState)) {
#if (AI_BUILD_TYPE == AI_DEBUG)
		qMilestone() << m_address << (paired ? "paired" : "unpaired");
#else
		qLimitedProdLog("RCU %spaired", paired ? "" : "un");
#endif
		emit pairedChanged(paired, BleRcuDevice::privateSignal());

		m_lastPairedState = paired;
	}

	// either way the pairing procedure has finished
	m_isPairing = false;

	// post an event to update the state machine
	if (paired)
		m_stateMachine.postEvent(DevicePairedEvent);
	else
		m_stateMachine.postEvent(DeviceUnpairedEvent);
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Called when the 'ServicesResolved' status of the device changes.

	This is trigger for the state machine and therefore obvious the device may
	move into a new state as a result of this signal.
 */
void BleRcuDeviceBluez::onDeviceServicesResolvedChanged(bool resolved)
{
	// log a milestone on any change
	if (Q_LIKELY(resolved != m_lastServicesResolvedState)) {
#if (AI_BUILD_TYPE == AI_DEBUG)
		qMilestone() << m_address << "services" << (resolved ? "resolved" : "unresolved");
#else
		qLimitedProdLog("RCU services %sresolved", resolved ? "" : "un");
#endif
		m_lastServicesResolvedState = resolved;
	}

	// post an event to update the state machine
	if (resolved)
		m_stateMachine.postEvent(ServicesResolvedEvent);
	else
		m_stateMachine.postEvent(ServicesNotResolvedEvent);
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Slot called from the state machine object when we've entered a new state.
	We hook this point to farm out the event to specific event handlers.

 */
void BleRcuDeviceBluez::onEnteredState(int state)
{
	switch (state) {
		case IdleState:
			break;

		case ResolvingServicesState:
			onEnteredResolvingServicesState();
			break;
		case StartingServicesState:
			onEnteredStartingServicesState();
			break;

		case RecoveryDisconnectingState:
			onEnteredRecoveryDisconnectingState();
			break;
		case RecoveryReconnectingState:
			onEnteredRecoveryReconnectingState();
			break;

		case ReadyState:
			onEnteredReadyState();
			break;

		default:
			break;
	}
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Slot called from the state machine object when we've exited a state.
	We hook this point to farm out the event to specific event handlers.

 */
void BleRcuDeviceBluez::onExitedState(int state)
{
	switch (State(state)) {
		case SetupSuperState:
			onExitedSetupSuperState();
			break;
		case ReadyState:
			onExitedReadyState();
			break;

		default:
			break;
	}
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Slot called on entry to the ready state.

 */
void BleRcuDeviceBluez::onEnteredReadyState()
{
	//
	m_timeSinceReady.start();

	// notify everyone that we are now ready
	emit readyChanged(true, BleRcuDevice::privateSignal());
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Slot called on exit from the ready state.

 */
void BleRcuDeviceBluez::onExitedReadyState()
{
	// notify everyone that we're no longer ready
	emit readyChanged(false, BleRcuDevice::privateSignal());
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Slot called on exit from the setup super state, basically when either we've
	disconnected, unpaired or the services have become unresolved.

 */
void BleRcuDeviceBluez::onExitedSetupSuperState()
{
	// stop the services as we've either not connected or lost pairing, either
	// way we won't have much luck talking to the RCU
	if (m_services)
		m_services->stop();
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Called upon entry to the 'resolving services' state, we check the services
	are not already resolved, if they are we just post the notification message.

 */
void BleRcuDeviceBluez::onEnteredResolvingServicesState()
{
	qInfo() << m_address << "entered RESOLVING_SERVICES state";

	// cancel any delayed events that may have previously been posted
	m_stateMachine.cancelDelayedEvents(ServicesResolveTimeoutEvent);

	// check if the services are already resolved
	if (m_lastServicesResolvedState == true) {

		// emit a fake signal to move the state machine on
		m_stateMachine.postEvent(ServicesResolvedEvent);

	} else {

		// services haven't been resolved so start a timer to check that the
		// services are resolved within 30 seconds, if they aren't then
		// something has gone wrong and we should try a manual re-connect
		if (m_recoveryAttempts < m_maxRecoveryAttempts)
			m_stateMachine.postDelayedEvent(ServicesResolveTimeoutEvent, 30000);

	}
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Called upon entry to the 'recovery' state, here we try and disconnect from
	device if connected.  Once disconnected we move on to the 'recovery
	reconnect' state.


 */
void BleRcuDeviceBluez::onEnteredRecoveryDisconnectingState()
{
	// increment the number of recovery attempts
	m_recoveryAttempts++;

	// log the attempt
#if (AI_BUILD_TYPE == AI_DEBUG)
	qError("entered recovery state after device %s failed to resolve services"
	       " (attempt #%d)", qPrintable(m_address.toString()), m_recoveryAttempts);
#else
	qLimitedProdLog("RCU failed to resolve services, triggering recovery (attempt #%d)",
	                m_recoveryAttempts);
#endif

	// always send a disconnect request, even if bluez is telling us we're
	// disconnected (sometimes it lies)
	QDBusPendingReply<> reply = m_deviceProxy->Disconnect();
	QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(reply, this);

	// lambda invoked when the request returns
	std::function<void(QDBusPendingCallWatcher*)> replyHandler =
		[this](QDBusPendingCallWatcher *call)
		{
			// check for errors (only for logging)
			QDBusPendingReply<> reply = *call;

			if (Q_UNLIKELY(reply.isError())) {
				QDBusError error = reply.error();
				qError() << m_address << "disconnect request failed with error" << error;
			} else {
				qDebug() << m_address << "disconnect request successful";
			}

			// free the pending reply object
			call->deleteLater();

			// if the device is now disconnected then update the state machine
			if (!m_lastConnectedState)
				m_stateMachine.postEvent(DeviceDisconnectedEvent);
		};

	// connect the lambda to the reply event
	QObject::connect(watcher, &QDBusPendingCallWatcher::finished,
	                 this, replyHandler);
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Called upon entry to the 'recovery re-connect' state, here we ask bluez to
	try and (re)connect to the device.

 */
void BleRcuDeviceBluez::onEnteredRecoveryReconnectingState()
{
	// always send a connect request, even if bluez is telling us we're
	// connected (sometimes it lies)
	QDBusPendingReply<> reply = m_deviceProxy->Connect();
	QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(reply, this);

	// lambda invoked when the request returns
	std::function<void(QDBusPendingCallWatcher*)> replyHandler =
		[this](QDBusPendingCallWatcher *call)
		{
			// check for errors (only for logging)
			QDBusPendingReply<> reply = *call;

			if (Q_UNLIKELY(reply.isError())) {
				QDBusError error = reply.error();
				qError() << m_address << "connect request failed with error" << error;
			} else {
				qDebug() << m_address << "connect request successful";
			}

			// free the pending reply object
			call->deleteLater();

			// if the device is now disconnected then update the state machine
			if (m_lastConnectedState)
				m_stateMachine.postEvent(DeviceConnectedEvent);
		};

	// connect the lambda to the reply event
	QObject::connect(watcher, &QDBusPendingCallWatcher::finished,
	                 this, replyHandler);
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Called upon entry to the 'start services' state...

 */
void BleRcuDeviceBluez::onEnteredStartingServicesState()
{
	qInfo() << m_address << "entered STARTING_SERVICES state";

	// sanity check
	if (!m_services) {
		qError("no services available for device");
		return;
	}

	// start the services
	m_services->start();

	// check if the services are already ready
	if (m_services->isReady()) {
		onServicesReady();

	} else {

		// chain the ready signal from the service to a local callback
		QObject::connect(m_services.data(), &BleRcuServices::ready,
		                 this, &BleRcuDeviceBluez::onServicesReady,
		                 Qt::UniqueConnection);
	}
}

// -----------------------------------------------------------------------------
/*!
	Called upon entry to the 'setup device info service' state, here we create
	the \a DeviceInfoService object attached to the message tunnel created
	in the previous state.
 */
void BleRcuDeviceBluez::onServicesReady()
{
	// post the event to the state machine saying the services are ready
	m_stateMachine.postEvent(ServicesStartedEvent);
}



QSharedPointer<BleRcuAudioService> BleRcuDeviceBluez::audioService() const
{
	Q_ASSERT(m_services && m_services->isValid());
	return m_services->audioService();
}

QSharedPointer<BleRcuBatteryService> BleRcuDeviceBluez::batteryService() const
{
	Q_ASSERT(m_services && m_services->isValid());
	return m_services->batteryService();
}

QSharedPointer<BleRcuDeviceInfoService> BleRcuDeviceBluez::deviceInfoService() const
{
	Q_ASSERT(m_services && m_services->isValid());
	return m_services->deviceInfoService();
}

QSharedPointer<BleRcuFindMeService> BleRcuDeviceBluez::findMeService() const
{
	Q_ASSERT(m_services && m_services->isValid());
	return m_services->findMeService();
}

QSharedPointer<BleRcuInfraredService> BleRcuDeviceBluez::infraredService() const
{
	Q_ASSERT(m_services && m_services->isValid());
	return m_services->infraredService();
}

QSharedPointer<BleRcuTouchService> BleRcuDeviceBluez::touchService() const
{
	Q_ASSERT(m_services && m_services->isValid());
	return m_services->touchService();
}

QSharedPointer<BleRcuUpgradeService> BleRcuDeviceBluez::upgradeService() const
{
	Q_ASSERT(m_services && m_services->isValid());
	return m_services->upgradeService();
}


// -----------------------------------------------------------------------------
/*!
	\fn bool BleRcuDevice::isValid() const

	Returns \c true if the device was successifully created and has all the
	services added.

 */
bool BleRcuDeviceBluez::isValid() const
{
	return !m_deviceProxy.isNull() && m_deviceProxy->isValid() &&
	       !m_services.isNull() && m_services->isValid();
}

// -----------------------------------------------------------------------------
/*!
	\fn BleAddress BleRcuDevice::address() const

	Returns the BDADDR/MAC address of the device.

 */
BleAddress BleRcuDeviceBluez::address() const
{
	return m_address;
}

// -----------------------------------------------------------------------------
/*!
	\fn QString BleRcuDevice::name() const

	Returns the current name of the device, this may be an empty string if the
	device doesn't have a name.

 */
QString BleRcuDeviceBluez::name() const
{
	return m_name;
}

// -----------------------------------------------------------------------------
/*!
	Returns \c true if the device is currently connected.

	More specifically it returns true if in any one of the following states;
	ConnectedState, ResolvingServicesState or SetupSuperState.

 */
bool BleRcuDeviceBluez::isConnected() const
{
	return m_stateMachine.inState({ ConnectedState, ResolvingServicesState,
	                                SetupSuperState });
}

// -----------------------------------------------------------------------------
/*!
	Returns \c true if the device is currently paired.

	More specifically it returns true if in any one of the following states;
	PairedState, ResolvingServicesState, RecoverySuperState or SetupSuperState.

 */
bool BleRcuDeviceBluez::isPaired() const
{
	return m_stateMachine.inState({ PairedState, ResolvingServicesState,
	                                RecoverySuperState, SetupSuperState });
}

// -----------------------------------------------------------------------------
/*!
	Returns \c true if the device is currently in the processing of pairing.


 */
bool BleRcuDeviceBluez::isPairing() const
{
	return m_isPairing;
}

// -----------------------------------------------------------------------------
/*!
	Returns \c true if the device is connected, paired and all the services have
	been initialised.

	More specifically it returns \c true in in the ReadyState.

 */
bool BleRcuDeviceBluez::isReady() const
{
	return m_stateMachine.inState(ReadyState);
}

// -----------------------------------------------------------------------------
/*!
	Returns the number of milliseconds since the device last transitioned into
	the ready state.

	If the device has never transitioned to the ready state then \c INT64_MAX
	is returned.

	\sa isReady(), readyChanged()
 */
qint64 BleRcuDeviceBluez::msecsSinceReady() const
{
	if (!m_timeSinceReady.isValid())
		return INT64_MAX;

	return m_timeSinceReady.elapsed();
}

// -----------------------------------------------------------------------------
/*!
	Returns the bluez dbus object path of the 'org.bluez.Device1' interface we
	are using to control this device.

	This value is constant and set at the construction time of this object.

 */
QDBusObjectPath BleRcuDeviceBluez::bluezObjectPath() const
{
	return m_bluezObjectPath;
}

// -----------------------------------------------------------------------------
/*!
	Called when dumpsys is invoked on this service.  We just simply dump the
	state of the device.

 */
void BleRcuDeviceBluez::dump(Dumper out) const
{
	out.printString("name:      ", m_name);
	out.printBoolean("connected: ", isConnected());
	out.printBoolean("paired:    ", isPaired());
	out.printLine("services:  %sresolved", m_lastServicesResolvedState ? "" : "not ");
	// out.printBoolean("input device attached:", m_inputDeviceConnected);
	out.printBoolean("ready:     ", isReady());

	out.printLine("Services:");
	if (m_services) {
		out.pushIndent(2);
		m_services->dump(out);
		out.popIndent();
	}
}

// -----------------------------------------------------------------------------
/*!
	\debugging

 */
QDebug operator<<(QDebug dbg, const BleRcuDeviceBluez &device)
{
	if (!device.isValid()) {
		dbg << "BleRcuDevice(invalid)";
	} else {
		dbg.nospace() << "BleRcuDevice(" << device.address()
		              << "," << device.name()
		              << "connected:" << device.isConnected()
		              << "paired:" << device.isPaired()
		              << "ready:" << device.isReady()
		              << ")";
	}

	return dbg.space();
}

