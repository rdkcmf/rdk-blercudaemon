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
//  gatt_findmeservice.cpp
//  SkyBluetoothRcu
//

#include "gatt_findmeservice.h"
#include "blercu/blegattservice.h"
#include "blercu/blegattcharacteristic.h"
#include "blercu/blercuerror.h"

#include "utils/logging.h"



const BleUuid GattFindMeService::m_serviceUuid(BleUuid::ImmediateAlert);


GattFindMeService::GattFindMeService()
	: BleRcuFindMeService(nullptr)
	, m_level(0)
{
	// initialise the state machine
	init();
}

GattFindMeService::~GattFindMeService()
{
	stop();
}

// -----------------------------------------------------------------------------
/*!
	Returns the constant gatt service uuid.

 */
BleUuid GattFindMeService::uuid()
{
	return m_serviceUuid;
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Initialises and starts the state machine.  The state machine always starts
	in the idle state.

 */
void GattFindMeService::init()
{
	m_stateMachine.setObjectName(QStringLiteral("GattFindMeService"));

	// add all the states and the super state groupings
	m_stateMachine.addState(IdleState, QStringLiteral("Idle"));
	m_stateMachine.addState(StartingState, QStringLiteral("Starting"));
	m_stateMachine.addState(RunningState, QStringLiteral("Running"));


	// add the transitions:      From State  -> Event                   ->  To State
	m_stateMachine.addTransition(IdleState,     StartServiceRequestEvent,   StartingState);
	m_stateMachine.addTransition(StartingState, ServiceReadyEvent,          RunningState);
	m_stateMachine.addTransition(StartingState, StopServiceRequestEvent,    IdleState);
	m_stateMachine.addTransition(RunningState,  StopServiceRequestEvent,    IdleState);


	// connect to the state entry signal
	QObject::connect(&m_stateMachine, &StateMachine::entered,
	                 this, &GattFindMeService::onEnteredState);
	QObject::connect(&m_stateMachine, &StateMachine::exited,
	                 this, &GattFindMeService::onExitedState);


	// set the initial state of the state machine and start it
	m_stateMachine.setInitialState(IdleState);
	m_stateMachine.start();
}

// -----------------------------------------------------------------------------
/*!
	Returns \c true if not if the service is ready
 */
bool GattFindMeService::isReady() const
{
	return m_stateMachine.inState(RunningState);
}

// -----------------------------------------------------------------------------
/*!
	Returns the current beeping state of the find me service
 */
BleRcuFindMeService::State GattFindMeService::state() const
{
	switch (m_level) {
		case 0:
			return BleRcuFindMeService::State::BeepingOff;
		case 1:
			return BleRcuFindMeService::State::BeepingMid;
		case 2:
			return BleRcuFindMeService::State::BeepingHigh;
		default:
			qWarning("unknown find me level");
			return BleRcuFindMeService::State::BeepingOff;
	}
}

// -----------------------------------------------------------------------------
/*!
	Starts the service, this should move the state machine on from the idle
	state to the ready state.

 */
bool GattFindMeService::start(const QSharedPointer<const BleGattService> &gattService)
{
	// create the proxy to the characteristic
	if (!m_alertLevelCharacteristic || !m_alertLevelCharacteristic->isValid()) {

		// sanity check the supplied info is valid
		if (!gattService->isValid() || (gattService->uuid() != m_serviceUuid)) {
			qWarning("invalid alert level gatt service info");
			return false;
		}

		// get the chararacteristic for the alert level
		m_alertLevelCharacteristic = gattService->characteristic(BleUuid::AlertLevel);
		if (!m_alertLevelCharacteristic || !m_alertLevelCharacteristic->isValid()) {
			qWarning("failed to get alert level characteristic");
			return false;
		}
	}

	// check we're not already started
	if (m_stateMachine.state() != IdleState) {
		qWarning("trying to start an already running findme service");
		return false;
	}

	// send the event to the state machine
	m_stateMachine.postEvent(StartServiceRequestEvent);
	return true;
}

// -----------------------------------------------------------------------------
/*!

 */
void GattFindMeService::stop()
{
	m_stateMachine.postEvent(StopServiceRequestEvent);
}

// -----------------------------------------------------------------------------
/*!
	\internal

 */
void GattFindMeService::onEnteredState(int state)
{
	if (state == IdleState) {

		// clear the characteristic, will be re-created on servce start
		m_alertLevelCharacteristic.reset();

	} else if (state == StartingState) {

		// will attempt to disable find me buzzer
		onEnteredStartingState();

	} else if (state == RunningState) {

		// entered running state so emit the ready signal
		emit ready();
	}
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Called on entry to the 'StartingState', here we just send a request to the
	rcu to stop findme, this is just to pipe clean the interface ... on the reply
	we signal that we are ready.

 */
void GattFindMeService::onEnteredStartingState()
{
	// set the alert level to 0
	m_level = 0;

	// lambda called if an error occurs disabling find me
	const std::function<void(const QString&, const QString&)> errorCallback =
		[this](const QString &errorName, const QString &errorMessage)
		{
			qError() << "failed to disable findme due to" << errorName << errorMessage;

			// signal we're ready, even though we failed
			m_stateMachine.postEvent(ServiceReadyEvent);
		};

	// lamda called findme was disabled
	const std::function<void()> successCallback =
		[this]()
		{
			qDebug("disabled buzzer during start-up");

			// signal we're ready
			m_stateMachine.postEvent(ServiceReadyEvent);
		};


	// write the beeper level
	const QByteArray value(1, m_level);
	Future<> result = m_alertLevelCharacteristic->writeValueWithoutResponse(value);
	if (!result.isValid() || result.isError()) {
		errorCallback(result.errorName(), result.errorMessage());
		return;
	} else if (result.isFinished()) {
		successCallback();
		return;
	}

	// connect functors to the future async completion
	result.connectErrored(this, errorCallback);
	result.connectFinished(this, successCallback);
}

// -----------------------------------------------------------------------------
/*!
	\internal

 */
void GattFindMeService::onExitedState(int state)
{
	if (state == RunningState) {
		// we're leaving the running state so cancel any pending operations with
		// an error
		if (m_promiseResults) {
			m_promiseResults->setError(BleRcuError::errorString(BleRcuError::General),
			                           QStringLiteral("Service stopped"));
			m_promiseResults.reset();
		}
	}
	
}

// -----------------------------------------------------------------------------
/*!
	Starts sending the 'find me' signal, or more precisely adds an event to the
	state machine to trigger it to move into the 'start signalling state', which
	will send the request to the RCU.

	This method will fail and return \c false if the service is not started or
	it is already signalling, i.e. should only be called it \a state() returns
	\a FindMeService::Ready.

 */
Future<> GattFindMeService::setFindMeLevel(quint8 level)
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
	m_level = level;
	const QByteArray value(1, m_level);

	// send a request to the vendor daemon start the find me beep, and connect
	// the reply to a listener
	Future<> result = m_alertLevelCharacteristic->writeValueWithoutResponse(value);
	if (!result.isValid() || result.isError()) {
		return Future<>::createErrored(BleRcuError::errorString(BleRcuError::General),
		                               QStringLiteral("Failed to issue request"));
	} else if (result.isFinished()) {
		return Future<>::createFinished();
	}

	// connect the future events
	result.connectFinished(this, &GattFindMeService::onFindMeRequestReply);
	result.connectErrored(this, &GattFindMeService::onFindMeRequestError);

	// create a new pending result object to notify the caller when the request
	// is complete
	m_promiseResults = QSharedPointer<Promise<>>::create();
	return m_promiseResults->future();
}

// -----------------------------------------------------------------------------
/*!
	Starts sending the 'find me' signal, or more precisely adds an event to the
	state machine to trigger it to move into the 'start signalling state', which
	will send the request to the RCU.

	This method will fail and return \c false if the service is not started or
	it is already signalling, i.e. should only be called it \a state() returns
	\a GattFindMeService::Ready.

 */
Future<> GattFindMeService::startBeeping(Level level, int duration)
{
	Q_UNUSED(duration);

	// convert the level to a value for the vendor daemon
	quint8 level_ = 0;
	switch (level) {
		case Level::High:   level_ = 2;   break;
		case Level::Mid:    level_ = 1;   break;
	}

	return setFindMeLevel(level_);
}

// -----------------------------------------------------------------------------
/*!
	Stops sending the 'find me' signal.

	This simply post the message to the state machine, if it's in one of the
	beeping states then this will move it to the stopping / stopped state.
	If not in a beeping state then this function doesn't nothing.

 */
Future<> GattFindMeService::stopBeeping()
{
	return setFindMeLevel(0);
}

// -----------------------------------------------------------------------------
/*!
	\internal


 */
void GattFindMeService::onFindMeRequestError(const QString &errorName,
                                             const QString &errorMessage)
{
	qWarning() << "failed to signal findme due to" << errorName;

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
void GattFindMeService::onFindMeRequestReply()
{
	// there should be a valid pending results object, if not something has
	// gone wrong
	if (!m_promiseResults) {
		qError("received a dbus reply message with no matching pending operation");
		return;
	}

	qDebug("findme signal written successfully");

	// non-error so complete the pending operation and post a message to the
	// state machine
	m_promiseResults->setFinished();
	m_promiseResults.reset();
}
