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
//  gatt_remotecontrolservice.cpp
//  SkyBluetoothRcu
//

#include "gatt_remotecontrolservice.h"
#include "blercu/blegattservice.h"
#include "blercu/blegattcharacteristic.h"
#include "blercu/blercuerror.h"

#include "utils/logging.h"

#include <QTimer>


const BleUuid GattRemoteControlService::m_serviceUuid(BleUuid::ComcastRemoteControl);
const BleUuid GattRemoteControlService::m_unpairReasonCharUuid(BleUuid::UnpairReason);
const BleUuid GattRemoteControlService::m_rebootReasonCharUuid(BleUuid::RebootReason);
const BleUuid GattRemoteControlService::m_rcuActionCharUuid(BleUuid::RcuAction);


GattRemoteControlService::GattRemoteControlService()
	: BleRcuRemoteControlService(nullptr)
	, m_unpairReason(0xFF)
	, m_rebootReason(0xFF)
	, m_rcuAction(0xFF)
{
	// create the basic statemachine
	init();
}

GattRemoteControlService::~GattRemoteControlService()
{
	//
	stop();

	//
	m_unpairReasonCharacteristic.reset();
	m_rebootReasonCharacteristic.reset();
	m_rcuActionCharacteristic.reset();
}

// -----------------------------------------------------------------------------
/*!
	Returns the constant gatt service uuid.

 */
BleUuid GattRemoteControlService::uuid()
{
	return m_serviceUuid;
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Configures and starts the state machine
 */
void GattRemoteControlService::init()
{
	m_stateMachine.setObjectName(QStringLiteral("GattRemoteControlService"));

	// add all the states and the super state groupings
	m_stateMachine.addState(IdleState, QStringLiteral("Idle"));
	m_stateMachine.addState(StartUnpairNotifyState, QStringLiteral("StartUnpairNotify"));
	m_stateMachine.addState(StartRebootNotifyState, QStringLiteral("StartRebootNotify"));
	m_stateMachine.addState(StartingState, QStringLiteral("Starting"));
	m_stateMachine.addState(RunningState, QStringLiteral("Running"));


	// add the transitions:      From State              -> Event                   ->  To State
	m_stateMachine.addTransition(IdleState,              StartServiceRequestEvent,   StartUnpairNotifyState);

	m_stateMachine.addTransition(StartUnpairNotifyState, RetryStartNotifyEvent,      StartUnpairNotifyState);
	m_stateMachine.addTransition(StartUnpairNotifyState, StopServiceRequestEvent,    IdleState);
	m_stateMachine.addTransition(StartUnpairNotifyState, StartedNotifingEvent,       StartRebootNotifyState);

	m_stateMachine.addTransition(StartRebootNotifyState, RetryStartNotifyEvent,      StartRebootNotifyState);
	m_stateMachine.addTransition(StartRebootNotifyState, StopServiceRequestEvent,    IdleState);
	m_stateMachine.addTransition(StartRebootNotifyState, StartedNotifingEvent,       StartingState);

	m_stateMachine.addTransition(StartingState,    ServiceReadyEvent,          RunningState);
	m_stateMachine.addTransition(StartingState,    StopServiceRequestEvent,    IdleState);

	m_stateMachine.addTransition(RunningState,     StopServiceRequestEvent,    IdleState);


	// connect to the state entry signal
	QObject::connect(&m_stateMachine, &StateMachine::entered,
	                 this, &GattRemoteControlService::onEnteredState);


	// set the initial state of the state machine and start it
	m_stateMachine.setInitialState(IdleState);
	m_stateMachine.start();
}

// -----------------------------------------------------------------------------
/*!
	\reimp

	Starts the service by creating a dbus proxy connection to the gatt
	service exposed by the bluez daemon.

	\a connection is the dbus connection to use to connected to dbus and
	the \a gattService contains the info on the dbus paths to the service
	and it's child characteristics and descriptors.

 */
bool GattRemoteControlService::start(const QSharedPointer<const BleGattService> &gattService)
{
	// sanity check the supplied info is valid
	if (!gattService->isValid() || (gattService->uuid() != m_serviceUuid)) {
		qWarning("invalid remote control gatt service info");
		return false;
	}

	// create the bluez dbus proxy to the characteristic
	if (!m_unpairReasonCharacteristic || !m_unpairReasonCharacteristic->isValid()) {
		// get the chararacteristic
		m_unpairReasonCharacteristic = gattService->characteristic(m_unpairReasonCharUuid);
		if (!m_unpairReasonCharacteristic || !m_unpairReasonCharacteristic->isValid()) {
			qWarning("failed to get unpair reason characteristic");
			return false;
		}

		// connect to the notification signal from the bluez daemon
		QObject::connect(m_unpairReasonCharacteristic.data(),
		                 &BleGattCharacteristic::valueChanged,
		                 this, &GattRemoteControlService::onUnpairReasonChanged);
	}
	// create the bluez dbus proxy to the characteristic
	if (!m_rebootReasonCharacteristic || !m_rebootReasonCharacteristic->isValid()) {
		// get the chararacteristic
		m_rebootReasonCharacteristic = gattService->characteristic(m_rebootReasonCharUuid);
		if (!m_rebootReasonCharacteristic || !m_rebootReasonCharacteristic->isValid()) {
			qWarning("failed to get reboot reason characteristic");
			return false;
		}

		// connect to the notification signal from the bluez daemon
		QObject::connect(m_rebootReasonCharacteristic.data(),
		                 &BleGattCharacteristic::valueChanged,
		                 this, &GattRemoteControlService::onRebootReasonChanged);
	}
	// create the bluez dbus proxy to the characteristic
	if (!m_rcuActionCharacteristic || !m_rcuActionCharacteristic->isValid()) {
		// get the chararacteristic
		m_rcuActionCharacteristic = gattService->characteristic(m_rcuActionCharUuid);
		if (!m_rcuActionCharacteristic || !m_rcuActionCharacteristic->isValid()) {
			qWarning("failed to get RCU action characteristic");
			return false;
		}
	}

	// check we're not already started
	if (m_stateMachine.state() != IdleState) {
		qWarning("remote control service already started");
		return true;
	}

	m_stateMachine.postEvent(StartServiceRequestEvent);
	return true;
}

// -----------------------------------------------------------------------------
/*!

 */
void GattRemoteControlService::stop()
{
	m_stateMachine.postEvent(StopServiceRequestEvent);
}

// -----------------------------------------------------------------------------
/*!
	\internal

 */
void GattRemoteControlService::onEnteredState(int state)
{
	if (state == IdleState) {
		m_unpairReasonCharacteristic.reset();
		m_rebootReasonCharacteristic.reset();
		m_rcuActionCharacteristic.reset();

	} else if (state == StartUnpairNotifyState) {
		requestStartUnpairNotify();

	} else if (state == StartRebootNotifyState) {
		requestStartRebootNotify();

	} else if (state == StartingState) {
		requestUnpairReason();
		requestRebootReason();
		m_stateMachine.postEvent(ServiceReadyEvent);

	} else if (state == RunningState) {

		// entered running state so emit the ready signal
		emit ready();
	}
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Sends a request to org.bluez.GattCharacteristic1.StartNotify() to enable
	notifications for changes to the characteristic value.

 */
void GattRemoteControlService::requestStartUnpairNotify()
{
	// lambda called if an error occurs enabling the notifications
	const std::function<void(const QString&, const QString&)> errorCallback =
		[this](const QString &errorName, const QString &errorMessage)
		{
			// this is bad if this happens as we won't get updates, so we install a timer to 
			// retry enabling notifications in a couple of seconds time
			qError() << "failed to enable unpair or reboot notifications due to"
			         << errorName << errorMessage;

			m_stateMachine.postDelayedEvent(RetryStartNotifyEvent, 2000);
		};

	// lamda called if notifications are successifully enabled
	const std::function<void()> successCallback =
		[this](void)
		{
			// notifications enabled so post an event to the state machine
			m_stateMachine.postEvent(StartedNotifingEvent);
		};


	// send a request to the bluez daemon to start notifing us of unpair reason changes
	Future<> result = m_unpairReasonCharacteristic->enableNotifications(true);
	if (!result.isValid() || result.isError()) {
		errorCallback(result.errorName(), result.errorMessage());

	} else if (result.isFinished()) {
		successCallback();

	} else {
		// connect functors to the future async completion
		result.connectErrored(this, errorCallback);
		result.connectFinished(this, successCallback);
	}
}

void GattRemoteControlService::requestStartRebootNotify()
{
	// lambda called if an error occurs enabling the notifications
	const std::function<void(const QString&, const QString&)> errorCallback =
		[this](const QString &errorName, const QString &errorMessage)
		{
			// this is bad if this happens as we won't get updates, so we install a timer to 
			// retry enabling notifications in a couple of seconds time
			qError() << "failed to enable unpair or reboot notifications due to"
			         << errorName << errorMessage;

			m_stateMachine.postDelayedEvent(RetryStartNotifyEvent, 2000);
		};

	// lamda called if notifications are successifully enabled
	const std::function<void()> successCallback =
		[this](void)
		{
			// notifications enabled so post an event to the state machine
			m_stateMachine.postEvent(StartedNotifingEvent);
		};


	// send a request to the bluez daemon to start notifing us of reboot reason changes
	Future<> result = m_rebootReasonCharacteristic->enableNotifications(true);
	if (!result.isValid() || result.isError()) {
		errorCallback(result.errorName(), result.errorMessage());

	} else if (result.isFinished()) {
		successCallback();

	} else {
		// connect functors to the future async completion
		result.connectErrored(this, errorCallback);
		result.connectFinished(this, successCallback);
	}
}

// -----------------------------------------------------------------------------
/*!
	Starts sending the 'find me' signal, or more precisely adds an event to the
	state machine to trigger it to move into the 'start signalling state', which
	will send the request to the RCU.

	This method will fail and return \c false if the service is not started or
	it is already signalling, i.e. should only be called it \a state() returns
	\a RemoteControlService::Ready.

 */
Future<> GattRemoteControlService::sendRcuAction(quint8 action)
{
	// check the service is ready
	if (!isReady()) {
		return Future<>::createErrored(BleRcuError::errorString(BleRcuError::Rejected),
		                               QStringLiteral("Service is not ready"));
	}

	// check we don't already have an outstanding pending call
	if (m_promiseResults) {
		return Future<>::createErrored(BleRcuError::errorString(BleRcuError::Busy),
		                               QStringLiteral("Request already in progress"));
	}


	// set the new level
	m_rcuAction = action;
	const QByteArray value(1, m_rcuAction);
	qWarning() << "sending RCU Action = " <<  m_rcuAction;

	// send a request to the vendor daemon write the characteristic, and connect
	// the reply to a listener
	Future<> result = m_rcuActionCharacteristic->writeValue(value);
	if (!result.isValid() || result.isError()) {
		return Future<>::createErrored(BleRcuError::errorString(BleRcuError::General),
		                               QStringLiteral("Failed to issue request"));
	} else if (result.isFinished()) {
		return Future<>::createFinished();
	}

	// connect the future events
	result.connectFinished(this, &GattRemoteControlService::onRcuActionReply);
	result.connectErrored(this, &GattRemoteControlService::onRcuActionError);

	// create a new pending result object to notify the caller when the request
	// is complete
	m_promiseResults = QSharedPointer<Promise<>>::create();
	return m_promiseResults->future();
}// -----------------------------------------------------------------------------
/*!
	\internal


 */
void GattRemoteControlService::onRcuActionError(const QString &errorName,
                                             const QString &errorMessage)
{
	qWarning() << "failed to send RCU action due to" << errorName;

	// signal the client that we failed
	if (m_promiseResults) {
		m_promiseResults->setError(BleRcuError::errorString(BleRcuError::General),
		                           errorMessage);
		m_promiseResults.reset();
	}
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Slot called a reply is received from the vendor daemon after a request to
	start / stop the find me signalling.
 */
void GattRemoteControlService::onRcuActionReply()
{
	// there should be a valid pending results object, if not something has
	// gone wrong
	if (!m_promiseResults) {
		qError("received a dbus reply message with no matching pending operation");
		return;
	}

	qDebug("RCU action written successfully");

	// non-error so complete the pending operation and post a message to the
	// state machine
	m_promiseResults->setFinished();
	m_promiseResults.reset();
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Sends a request to org.bluez.GattCharacteristic1.Value() to get the value
	propery of the characteristic which contains the unpair reason.

 */
void GattRemoteControlService::requestUnpairReason()
{
	// lambda called if an error occurs enabling the notifications
	const std::function<void(const QString&, const QString&)> errorCallback =
		[this](const QString &errorName, const QString &errorMessage)
		{
			qError() << "failed to get unpair reason due to"
			         << errorName << errorMessage;
		};

	// lamda called if notifications are successifully enabled
	const std::function<void(const QByteArray &value)> successCallback =
		[this](const QByteArray &value)
		{
			// onUnpairReasonChanged(value);
			m_unpairReason = static_cast<quint8>(value.at(0));
			qWarning() << "Initial unpair reason is" << m_unpairReason;
		};


	// send a request to the bluez daemon to read the characteristic
	Future<QByteArray> result = m_unpairReasonCharacteristic->readValue();
	if (!result.isValid() || result.isError()) {
		errorCallback(result.errorName(), result.errorMessage());
		return;
	} else if (result.isFinished()) {
		successCallback(result.result());
		return;
	}

	// connect functors to the future async completion
	result.connectErrored(this, errorCallback);
	result.connectFinished(this, successCallback);
}
// -----------------------------------------------------------------------------
/*!
	\internal

	Sends a request to org.bluez.GattCharacteristic1.Value() to get the value
	propery of the characteristic which contains the reboot reason.

 */
void GattRemoteControlService::requestRebootReason()
{
	// lambda called if an error occurs enabling the notifications
	const std::function<void(const QString&, const QString&)> errorCallback =
		[this](const QString &errorName, const QString &errorMessage)
		{
			qError() << "failed to get reboot reason due to"
			         << errorName << errorMessage;
		};

	// lamda called if notifications are successifully enabled
	const std::function<void(const QByteArray &value)> successCallback =
		[this](const QByteArray &value)
		{
			// onRebootReasonChanged(value);
			m_rebootReason = static_cast<quint8>(value.at(0));
			qInfo() << "Initial reboot reason is" << m_rebootReason;
		};


	// send a request to the bluez daemon to read the characteristic
	Future<QByteArray> result = m_rebootReasonCharacteristic->readValue();
	if (!result.isValid() || result.isError()) {
		errorCallback(result.errorName(), result.errorMessage());
		return;
	} else if (result.isFinished()) {
		successCallback(result.result());
		return;
	}

	// connect functors to the future async completion
	result.connectErrored(this, errorCallback);
	result.connectFinished(this, successCallback);
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Internal slot called when a notification from the remote device is sent
	due to unpair reason change.
 */
void GattRemoteControlService::onUnpairReasonChanged(const QByteArray &newValue)
{
	m_unpairReason = static_cast<quint8>(newValue.at(0));
	qWarning() << "unpair reason changed to" << m_unpairReason;
	emit unpairReasonChanged(m_unpairReason);
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Internal slot called when a notification from the remote device is sent
	due to reboot reason change.
 */
void GattRemoteControlService::onRebootReasonChanged(const QByteArray &newValue)
{
	m_rebootReason = static_cast<quint8>(newValue.at(0));
	qWarning() << "reboot reason changed to" << m_rebootReason;
	emit rebootReasonChanged(m_rebootReason);
}


// -----------------------------------------------------------------------------
/*!
	\overload

 */
bool GattRemoteControlService::isReady() const
{
	return (m_stateMachine.state() == RunningState);
}

// -----------------------------------------------------------------------------
/*!
	\overload

 */
quint8 GattRemoteControlService::unpairReason() const
{
	return m_unpairReason;
}
// -----------------------------------------------------------------------------
/*!
	\overload

 */
quint8 GattRemoteControlService::rebootReason() const
{
	return m_rebootReason;
}
