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
//  gatt_batteryservice.cpp
//  SkyBluetoothRcu
//

#include "gatt_batteryservice.h"
#include "blercu/blegattservice.h"
#include "blercu/blegattcharacteristic.h"

#include "utils/logging.h"

#include <QTimer>


const BleUuid GattBatteryService::m_serviceUuid(BleUuid::BatteryService);
const BleUuid GattBatteryService::m_batteryLevelCharUuid(BleUuid::BatteryLevel);


GattBatteryService::GattBatteryService()
	: BleRcuBatteryService(nullptr)
	, m_batteryLevel(-1)
	, m_lastLoggedLevel(-1)
{
	// setup the timer that polls the battery level and reports in the log
	m_logTimer.setSingleShot(false);
	m_logTimer.setInterval(5 * 60 * 1000);
	QObject::connect(&m_logTimer, &QTimer::timeout,
	                 this, &GattBatteryService::onLogTimerTimeout);

	// create the basic statemachine
	init();
}

GattBatteryService::~GattBatteryService()
{
	stop();
}

// -----------------------------------------------------------------------------
/*!
	Returns the constant gatt service uuid.

 */
BleUuid GattBatteryService::uuid()
{
	return m_serviceUuid;
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Vendor daemons send 255 for when the battery level is unknown / invalid,
	whereas we use -1 for that case, this function handles the conversion.
 */
int GattBatteryService::sanitiseBatteryLevel(char level) const
{
	return qBound<int>(0, level, 100);
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Configures and starts the state machine
 */
void GattBatteryService::init()
{
	m_stateMachine.setObjectName(QStringLiteral("GattBatteryService"));

	// add all the states and the super state groupings
	m_stateMachine.addState(IdleState, QStringLiteral("Idle"));
	m_stateMachine.addState(StartNotifyState, QStringLiteral("StartNotify"));
	m_stateMachine.addState(StartingState, QStringLiteral("Starting"));
	m_stateMachine.addState(RunningState, QStringLiteral("Running"));


	// add the transitions:      From State     -> Event                   ->  To State
	m_stateMachine.addTransition(IdleState,        StartServiceRequestEvent,   StartNotifyState);

	m_stateMachine.addTransition(StartNotifyState, RetryStartNotifyEvent,      StartNotifyState);
	m_stateMachine.addTransition(StartNotifyState, StopServiceRequestEvent,    IdleState);
	m_stateMachine.addTransition(StartNotifyState, StartedNotifingEvent,       StartingState);

	m_stateMachine.addTransition(StartingState,    ServiceReadyEvent,          RunningState);
	m_stateMachine.addTransition(StartingState,    StopServiceRequestEvent,    IdleState);

	m_stateMachine.addTransition(RunningState,     StopServiceRequestEvent,    IdleState);


	// connect to the state entry signal
	QObject::connect(&m_stateMachine, &StateMachine::entered,
	                 this, &GattBatteryService::onEnteredState);


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
bool GattBatteryService::start(const QSharedPointer<const BleGattService> &gattService)
{

	// create the bluez dbus proxy to the characteristic
	if (!m_battLevelCharacteristic || !m_battLevelCharacteristic->isValid()) {

		// sanity check the supplied info is valid
		if (!gattService->isValid() || (gattService->uuid() != m_serviceUuid)) {
			qWarning("invalid battery gatt service info");
			return false;
		}

		// get the chararacteristic for the actual battery level
		m_battLevelCharacteristic = gattService->characteristic(m_batteryLevelCharUuid);
		if (!m_battLevelCharacteristic || !m_battLevelCharacteristic->isValid()) {
			qWarning("failed to get battery level characteristic");
			return false;
		}

		// connect to the notification signal from the bluez daemon
		QObject::connect(m_battLevelCharacteristic.data(),
		                 &BleGattCharacteristic::valueChanged,
		                 this, &GattBatteryService::onBatteryLevelChanged);

		// TODO: create a timer to poll the battery level ... just in case
	}

	// check we're not already started
	if (m_stateMachine.state() != IdleState) {
		qWarning("battery service already started");
		return true;
	}

	// start the logging timer used to record the battery level, initially we
	// report the battery level in 5mins time, subsequently it's every 2 hours
	m_logTimer.start(5 * 60 * 1000);

	m_stateMachine.postEvent(StartServiceRequestEvent);
	return true;
}

// -----------------------------------------------------------------------------
/*!

 */
void GattBatteryService::stop()
{
	m_logTimer.stop();
	m_stateMachine.postEvent(StopServiceRequestEvent);
}

// -----------------------------------------------------------------------------
/*!
	\internal

 */
void GattBatteryService::onEnteredState(int state)
{
	if (state == IdleState) {
		if (m_battLevelCharacteristic) {
			qInfo() << "Disabling notifications for m_battLevelCharacteristic";
			m_battLevelCharacteristic->enableNotifications(false);
		}
		m_battLevelCharacteristic.reset();

	} else if (state == StartNotifyState) {
		requestStartNotify();

	} else if (state == StartingState) {
		requestBatteryLevel();

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
void GattBatteryService::requestStartNotify()
{
	// lambda called if an error occurs enabling the notifications
	const std::function<void(const QString&, const QString&)> errorCallback =
		[this](const QString &errorName, const QString &errorMessage)
		{
			// this is bad if this happens as we won't get updates to the battery
			// level, so we install a timer to retry enabling notifications in a
			// couple of seconds time
			qError() << "failed to enable battery level notifications due to"
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


	// send a request to the bluez daemon to start notifing us of battery
	// level changes
	Future<> result = m_battLevelCharacteristic->enableNotifications(true);
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
	\internal

	Sends a request to org.bluez.GattCharacteristic1.Value() to get the value
	propery of the characteristic which contains the current battery level.

 */
void GattBatteryService::requestBatteryLevel()
{
	// lambda called if an error occurs enabling the notifications
	const std::function<void(const QString&, const QString&)> errorCallback =
		[this](const QString &errorName, const QString &errorMessage)
		{
			qError() << "failed to get battery level due to"
			         << errorName << errorMessage;

			// even though an error occured signal that we are now ready
			m_stateMachine.postEvent(ServiceReadyEvent);
		};

	// lamda called if notifications are successifully enabled
	const std::function<void(const QByteArray &value)> successCallback =
		[this](const QByteArray &value)
		{
			// trigger a notification that the battery level changed
			onBatteryLevelChanged(value);

			// signal that we are now ready
			m_stateMachine.postEvent(ServiceReadyEvent);
		};



	// send a request to the bluez daemon to start notifing us of battery
	// level changes
	Future<QByteArray> result = m_battLevelCharacteristic->readValue();
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
	due to a battery level change.
 */
void GattBatteryService::onBatteryLevelChanged(const QByteArray &newValue)
{
	// sanity check the data received
	if (newValue.length() != 1) {
		qError("battery value received has invalid length (%d bytes)",
		       newValue.length());
		return;
	}

	qInfo() << "battery level changed to" << static_cast<quint8>(newValue.at(0));

	// clamp the reply between 0 and 100
	int level = sanitiseBatteryLevel(newValue.at(0));

	// store the battery level and emit a signal if it's changed
	if (level != m_batteryLevel) {
		m_batteryLevel = level;
		emit levelChanged(m_batteryLevel);
	}
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Called when the long timer used to log the battery level expires.  Just
	prints the battery level.

 */
void GattBatteryService::onLogTimerTimeout()
{
	// reset the interval to 2 hours
	m_logTimer.setInterval(2 * 60 * 60 * 1000);

	// log the current battery level if changed
	if ((m_batteryLevel > 0) && (m_batteryLevel != m_lastLoggedLevel)) {
		m_lastLoggedLevel = m_batteryLevel;
		qProdLog("RCU battery level %d%%", m_batteryLevel);
	}
}

// -----------------------------------------------------------------------------
/*!
	\overload

 */
bool GattBatteryService::isReady() const
{
	return (m_stateMachine.state() == RunningState);
}

// -----------------------------------------------------------------------------
/*!
	\overload

 */
int GattBatteryService::level() const
{
	return m_batteryLevel;
}
