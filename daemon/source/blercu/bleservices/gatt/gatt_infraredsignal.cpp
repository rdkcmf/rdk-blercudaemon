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
//  gatt_infraredsignal.cpp
//  SkyBluetoothRcu
//

#include "gatt_infraredsignal.h"
#include "gatt_infraredservice.h"

#include "blercu/blegattcharacteristic.h"
#include "blercu/blegattdescriptor.h"

#include "utils/logging.h"



// -----------------------------------------------------------------------------
/*!
	\class GattInfraredSignal
	\brief This runs the state machine for the individual GATT Infrared Signal
	object.

	One of these objects (and hence the underlying GATT characteristic)
	corresponds to a physical button on the RCU that can be programmed with
	different IR signals.

 */



GattInfraredSignal::GattInfraredSignal(const QSharedPointer<BleGattCharacteristic> &gattCharacteristic,
                                       QObject *parent)
	: QObject(parent)
	, m_signalCharacteristic(gattCharacteristic)
	, m_keyCode(Qt::Key_unknown)
{

	// create a proxy to the dbus gatt interface
	if (!m_signalCharacteristic || !m_signalCharacteristic->isValid()) {
		qError("failed to create proxy to infrared signal");
		return;
	}


	// we also want proxies to the characteristic's two descriptors
	m_signalReferenceDescriptor = gattCharacteristic->descriptor(BleUuid::InfraredSignalReference);
	if (!m_signalReferenceDescriptor || !m_signalReferenceDescriptor->isValid()) {
		qError("failed to create proxy to infrared signal reference");
		m_signalCharacteristic.reset();
		return;
	}

	m_signalConfigurationDescriptor = gattCharacteristic->descriptor(BleUuid::InfraredSignalConfiguration);
	if (!m_signalConfigurationDescriptor || !m_signalConfigurationDescriptor->isValid()) {
		qError("failed to create proxy to infrared signal configuration");
		m_signalReferenceDescriptor.reset();
		m_signalCharacteristic.reset();
		return;
	}


	// finally initialise the state machine
	initStateMachine();
}

GattInfraredSignal::~GattInfraredSignal()
{
	m_signalConfigurationDescriptor.reset();
	m_signalReferenceDescriptor.reset();
	m_signalCharacteristic.reset();
}


// -----------------------------------------------------------------------------
/*!
	\internal

	Sets up the state machine and starts it in the Idle state.

 */
void GattInfraredSignal::initStateMachine()
{
	m_stateMachine.setObjectName(QStringLiteral("GattInfraredSignal"));


	// add all the states and the super state groupings
	m_stateMachine.addState(IdleState, QStringLiteral("Idle"));
	m_stateMachine.addState(InitialisingState, QStringLiteral("Initialising"));
	m_stateMachine.addState(ReadyState, QStringLiteral("Ready"));
	m_stateMachine.addState(ProgrammingSuperState, QStringLiteral("ProgrammingSuperState"));
	m_stateMachine.addState(ProgrammingSuperState, DisablingState, QStringLiteral("Disabling"));
	m_stateMachine.addState(ProgrammingSuperState, WritingState, QStringLiteral("Writing"));
	m_stateMachine.addState(ProgrammingSuperState, EnablingState, QStringLiteral("Enabling"));


	// add the transitions:      From State         ->  Event               ->  To State
	m_stateMachine.addTransition(IdleState,             StartRequestEvent,      InitialisingState);

	m_stateMachine.addTransition(InitialisingState,     AckEvent,               ReadyState);
	m_stateMachine.addTransition(InitialisingState,     ErrorEvent,             IdleState);
	m_stateMachine.addTransition(InitialisingState,     StopRequestEvent,       IdleState);

	m_stateMachine.addTransition(ReadyState,            ProgramRequestEvent,    DisablingState);

	m_stateMachine.addTransition(ProgrammingSuperState, ErrorEvent,             ReadyState);
	m_stateMachine.addTransition(ProgrammingSuperState, StopRequestEvent,       IdleState);

	m_stateMachine.addTransition(DisablingState,        AckEvent,               WritingState);
	m_stateMachine.addTransition(WritingState,          AckEvent,               EnablingState);
	m_stateMachine.addTransition(EnablingState,         AckEvent,               ReadyState);


	// add a slot for state machine notifications
	QObject::connect(&m_stateMachine, &StateMachine::entered,
	                 this, &GattInfraredSignal::onEnteredState);
	QObject::connect(&m_stateMachine, &StateMachine::exited,
	                 this, &GattInfraredSignal::onExitedState);


	// set the initial state of the state machine and start it
	m_stateMachine.setInitialState(IdleState);
	m_stateMachine.start();
}


// -----------------------------------------------------------------------------
/*!
	Returns \c true if the characteristic is valid.

 */
bool GattInfraredSignal::isValid() const
{
	return !m_signalCharacteristic.isNull();
}

// -----------------------------------------------------------------------------
/*!
	Returns the instance id of the characteristic that this object is wrapping.

 */
int GattInfraredSignal::instanceId() const
{
	if (!m_signalCharacteristic)
		return -1;

	return m_signalCharacteristic->instanceId();
}

// -----------------------------------------------------------------------------
/*!
	Returns \c true if the characteristic is valid.

 */
bool GattInfraredSignal::isReady() const
{
	return !m_stateMachine.inState(IdleState);
}

// -----------------------------------------------------------------------------
/*!
	Returns the key code of the physical button that this characteristic
	represents.  This is not initially known, you need to wait till we
	transition to the ready state before we know what the key will be.

	Will return BleRcuInfraredService::InvalidKey if called if the object is not
	valid or is not ready.

 */
Qt::Key GattInfraredSignal::keyCode() const
{
	return m_keyCode;
}

// -----------------------------------------------------------------------------
/*!
	Sends a request to start the service, this will only take affect if the
	the signal is in the Idle state.

 */
void GattInfraredSignal::start()
{
	m_stateMachine.postEvent(StartRequestEvent);
}

// -----------------------------------------------------------------------------
/*!
	Stops the signal state machine, this will cancel any outstanding program
	operations and cause their futures to return an error.

 */
void GattInfraredSignal::stop()
{
	m_stateMachine.postEvent(StopRequestEvent);
}

// -----------------------------------------------------------------------------
/*!
	Sends a request to program the given data into the signal object.  If the
	object is not valid or ready then an immediately errored future is returned.

	If \a data is empty then the signal data is erased from the RCU.

	If a programming operation is already in progress, the returned future will
	be immediately errored.  This method does NOT queue up writes.

 */
Future<> GattInfraredSignal::program(const QByteArray &data)
{
	// check if we're ready
	if (!m_stateMachine.inState(ReadyState)) {
		qWarning("ir signal not ready for programming");
		return Future<>::createErrored(BleRcuError::errorString(BleRcuError::Busy),
		                               QStringLiteral("IR signal is not ready"));
	}

	// check we're not already in the middle or programming
	if (m_programmingPromise) {
		qWarning("ir signal is already being programmed");
		return Future<>::createErrored(BleRcuError::errorString(BleRcuError::Busy),
		                               QStringLiteral("Programming in progress"));
	}

	// sanity check the data, it should be less than 256 bytes
	if (data.length() > 256) {
		qWarning("ir signal data is to large, expected to be less than 256 bytes,"
		         " actual %d bytes", data.length());
		return Future<>::createErrored(BleRcuError::errorString(BleRcuError::General),
		                               QStringLiteral("IR data to large"));
	}

	// store the data
	m_infraredData = data;

	// create a new promise to signal the result
	m_programmingPromise = QSharedPointer< Promise<> >::create();

	// trigger the statemachine and return the future
	m_stateMachine.postEvent(ProgramRequestEvent);
	return m_programmingPromise->future();
}

// -----------------------------------------------------------------------------
/*!
	\internal

 */
void GattInfraredSignal::onEnteredState(int state)
{
	switch (State(state)) {
		case InitialisingState:
			onEnteredInitialisingState();
			break;

		case DisablingState:
			onEnteredDisablingState();
			break;
		case WritingState:
			onEnteredWritingState();
			break;
		case EnablingState:
			onEnteredEnablingState();
			break;

		default:
			break;
	}

}

// -----------------------------------------------------------------------------
/*!
	\internal

 */
void GattInfraredSignal::onExitedState(int state)
{
	switch (State(state)) {
		case ProgrammingSuperState:
			onExitedProgrammingState();
			break;

		default:
			break;
	}

}

// -----------------------------------------------------------------------------
/*!
	\internal

	Called on entry to the 'read reference descriptor' state, we cache the
	reference descriptor value, so we only need to do work the first time.

	The 'Infrared Signal Reference Descriptor' is used to tell us what physical
	button the parent characteristic corresponds to.  Without this the sate
	machine can't advance and this object never moves into the ready state.

 */
void GattInfraredSignal::onEnteredInitialisingState()
{
	// sanity check
	if (Q_UNLIKELY(!m_signalReferenceDescriptor)) {
		qError("missing ISRD proxy");
		return;
	}

	// if we've already read this value, no need to do it again
	if (m_keyCode != Qt::Key_unknown) {
		m_stateMachine.postEvent(AckEvent);
		return;
	}



	// lambda called if an error occurs reading the descriptor
	const std::function<void(const QString&, const QString&)> errorCallback =
		[this](const QString &errorName, const QString &errorMessage)
		{
			qError() << "failed to read ISRD due to" << errorName << errorMessage;

			// TODO: should we retry after a certain amount of time ?
			m_stateMachine.postEvent(ErrorEvent);
		};

	// lamda called after successifully reading the descriptor
	const std::function<void(const QByteArray&)> successCallback =
		[this](const QByteArray &value)
		{
			// the value should be a single byte containing a known value ...
			if (value.length() != 1) {
				qError("invalid ISRD value, length wrong (%d bytes)", value.length());

				// TODO: should we retry after a certain amount of time ?
				m_stateMachine.postEvent(ErrorEvent);
				return;
			}

			switch (value.at(0)) {
				case 0x0B:   m_keyCode = Qt::Key_WakeUp;          break; // secondary power
				case 0x0C:   m_keyCode = Qt::Key_Standby;         break; // primary power
				case 0x29:   m_keyCode = Qt::Key_Settings;        break; // input select
				case 0x10:   m_keyCode = Qt::Key_VolumeUp;        break;
				case 0x11:   m_keyCode = Qt::Key_VolumeDown;      break;
				case 0x0D:   m_keyCode = Qt::Key_VolumeMute;      break;

				case 0x5C:   m_keyCode = Qt::Key_Select;          break;
				case 0x58:   m_keyCode = Qt::Key_Up;              break;
				case 0x59:   m_keyCode = Qt::Key_Down;            break;
				case 0x5A:   m_keyCode = Qt::Key_Left;            break;
				case 0x5B:   m_keyCode = Qt::Key_Right;           break;

				default:
					qError("unknown ISRD value - 0x%02hhx", quint8(value.at(0)));

					// TODO: should we retry after a certain amount of time ?
					m_stateMachine.postEvent(ErrorEvent);
					return;
			}

			qInfo() << "found characteristic for" << m_keyCode;

			// success
			m_stateMachine.postEvent(AckEvent);
		};


	// send a request to the RCU to read the value
	Future<QByteArray> result = m_signalReferenceDescriptor->readValue();
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

	Called on exit from the 'programming' super state, this can happen for
	if an error or cancel occurs, or simply when the programming completes. For
	the later case we don't care, for the error/cancel case we want to complete
	the future with an error.

 */
void GattInfraredSignal::onExitedProgrammingState()
{
	// don't care if the promise has already completed
	if (!m_programmingPromise)
		return;

	// complete the promise with an error and then clear
	m_programmingPromise->setError(BleRcuError::errorString(BleRcuError::General),
	                               QStringLiteral("Programming cancelled"));
	m_programmingPromise.reset();
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Called on entry to the 'disabling' state, simply queues up the write request
	to the configuration descriptor.

 */
void GattInfraredSignal::onEnteredDisablingState()
{
	// sanity check
	if (Q_UNLIKELY(!m_signalConfigurationDescriptor)) {
		qError("missing ISCD proxy");
		return;
	}


	// lambda called if an error occurs reading the descriptor
	const std::function<void(const QString&, const QString&)> errorCallback =
		[this](const QString &errorName, const QString &errorMessage)
		{
			qError() << "failed to write 0x00 to ISCD due to"
			         << errorName << errorMessage;

			m_stateMachine.postEvent(ErrorEvent);
		};

	// lamda called after successifully writing the descriptor
	const std::function<void()> successCallback =
		[this]()
		{
			qDebug() << "disabled" << m_keyCode << "ir signal";

			m_stateMachine.postEvent(AckEvent);
		};


	// send a request to write the value to disable the IR signal
	const QByteArray value(1, 0x00);
	Future<> result = m_signalConfigurationDescriptor->writeValue(value);
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

	Called on entry to the 'writing' state, if the data we've been asked to
	write is empty then we do nothing in this state.

 */
void GattInfraredSignal::onEnteredWritingState()
{
	// sanity check
	if (Q_UNLIKELY(!m_signalCharacteristic)) {
		qError("missing ir signal characteristic proxy");
		return;
	}

	// it's not an error if the data to write it empty, it just means that the
	// signal should be disabled, so skip the writing phase
	if (m_infraredData.isEmpty()) {
		m_stateMachine.postEvent(AckEvent);
		return;
	}


	// lambda called if an error occurs reading the descriptor
	const std::function<void(const QString&, const QString&)> errorCallback =
		[this](const QString &errorName, const QString &errorMessage)
		{
			qError() << "failed to write ir signal data due to"
			         << errorName << errorMessage;

			m_stateMachine.postEvent(ErrorEvent);
		};

	// lamda called after successifully writing the descriptor
	const std::function<void()> successCallback =
		[this]()
		{
			qDebug() << "written" << m_keyCode << "ir signal data";

			m_stateMachine.postEvent(AckEvent);
		};


	// send a request to write the value to disable the IR signal
	Future<> result = m_signalCharacteristic->writeValue(m_infraredData);
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

	Called on entry to the 'writing' state, if the data we've been asked to
	write is empty then we do nothing in this state.

 */
void GattInfraredSignal::onEnteredEnablingState()
{
	// sanity check
	if (Q_UNLIKELY(!m_signalConfigurationDescriptor)) {
		qError("missing ISCD proxy");
		return;
	}

	// it's not an error if the data to write it empty, it just means that the
	// signal should be disabled, so skip the enabling phase
	if (m_infraredData.isEmpty()) {

		// complete the future with a positive result
		if (m_programmingPromise) {
			m_programmingPromise->setFinished();
			m_programmingPromise.reset();
		}

		// send a positive ack to complete the state machine
		m_stateMachine.postEvent(AckEvent);
		return;
	}


	// lambda called if an error occurs reading the descriptor
	const std::function<void(const QString&, const QString&)> errorCallback =
		[this](const QString &errorName, const QString &errorMessage)
		{
			qError() << "failed to write 0x01 to ISCD due to"
			         << errorName << errorMessage;

			m_stateMachine.postEvent(ErrorEvent);
		};

	// lamda called after successifully writing the descriptor
	const std::function<void()> successCallback =
		[this]()
		{
			qDebug() << "enabled" << m_keyCode << "ir signal";

			// complete the future with a positive result
			if (m_programmingPromise) {
				m_programmingPromise->setFinished();
				m_programmingPromise.reset();
			}

			// send a positive ack to complete the state machine
			m_stateMachine.postEvent(AckEvent);
		};


	// send a request to write the value to enable the IR signal
	const QByteArray value(1, 0x01);
	Future<> result = m_signalConfigurationDescriptor->writeValue(value);
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

