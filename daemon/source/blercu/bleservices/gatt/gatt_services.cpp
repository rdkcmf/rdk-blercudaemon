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
//  gatt_services.cpp
//  SkyBluetoothRcu
//

#include "gatt_services.h"

#include "gatt_audioservice.h"
#include "gatt_batteryservice.h"
#include "gatt_deviceinfoservice.h"
#include "gatt_findmeservice.h"
#include "gatt_infraredservice.h"
#include "gatt_touchservice.h"
#include "gatt_upgradeservice.h"
#include "gatt_remotecontrolservice.h"

#include "blercu/blegattprofile.h"
#include "blercu/blegattservice.h"
#include "utils/logging.h"

#include <QTimer>


// -----------------------------------------------------------------------------
/*!
	Constructs an \l{GattServices} object which will connect to the vendor
	daemon and attempt to register the device with the supplied
	\a bluezDeviceObjPath.


 */
GattServices::GattServices(const BleAddress &address,
                           const QSharedPointer<BleGattProfile> &gattProfile,
                           const QSharedPointer<const IrDatabase> &irDatabase,
                           const ConfigModelSettings &settings,
                           QObject *parent)
	: BleRcuServices(parent)
	, m_address(address)
	, m_gattProfile(gattProfile)
	, m_irDatabase(irDatabase)
	, m_audioService(QSharedPointer<GattAudioService>::create())
	, m_deviceInfoService(QSharedPointer<GattDeviceInfoService>::create())
	, m_batteryService(QSharedPointer<GattBatteryService>::create())
	, m_findMeService(QSharedPointer<GattFindMeService>::create())
	, m_infraredService(QSharedPointer<GattInfraredService>::create(irDatabase, settings))
	, m_touchService(QSharedPointer<GattTouchService>::create())
	, m_upgradeService(QSharedPointer<GattUpgradeService>::create())
	, m_remoteControlService(QSharedPointer<GattRemoteControlService>::create())
{

	// connect to the gatt profile update completed event
	QObject::connect(gattProfile.data(), &BleGattProfile::updateCompleted,
	                 this, &GattServices::onGattProfileUpdated,
	                 Qt::QueuedConnection);

	// connect to the upgradeFinished signal of the upgrade service to the
	// device info service so that the device info service re-queries it's
	// cached values after an upgrade
	QObject::connect(m_upgradeService.data(), &GattUpgradeService::upgradeComplete,
	                 m_deviceInfoService.data(), &GattDeviceInfoService::forceRefresh,
	                 Qt::QueuedConnection);

	// setup and start the state machine
	init();
}

// -----------------------------------------------------------------------------
/*!
	Disposes of this object.

 */
GattServices::~GattServices()
{
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Configures and starts the state machine
 */
void GattServices::init()
{
	m_stateMachine.setObjectName(QStringLiteral("GattServices"));

	// add all the states and the super state groupings
	m_stateMachine.addState(IdleState, QStringLiteral("Idle"));
	m_stateMachine.addState(GettingGattServicesState, QStringLiteral("GettingGattServicesState"));

	m_stateMachine.addState(ResolvedServicesSuperState, QStringLiteral("ResolvedServicesSuperState"));
	m_stateMachine.addState(ResolvedServicesSuperState, StartingDeviceInfoServiceState, QStringLiteral("StartingDeviceInfoService"));
	m_stateMachine.addState(ResolvedServicesSuperState, StartingBatteryServiceState, QStringLiteral("StartingBatteryService"));
	m_stateMachine.addState(ResolvedServicesSuperState, StartingFindMeServiceState, QStringLiteral("StartingFindMeService"));
	m_stateMachine.addState(ResolvedServicesSuperState, StartingAudioServiceState, QStringLiteral("StartingAudioService"));
	m_stateMachine.addState(ResolvedServicesSuperState, StartingInfraredServiceState, QStringLiteral("StartingInfraredService"));
	m_stateMachine.addState(ResolvedServicesSuperState, StartingTouchServiceState, QStringLiteral("StartingTouchService"));
	m_stateMachine.addState(ResolvedServicesSuperState, StartingUpgradeServiceState, QStringLiteral("StartingUpgradeServiceState"));
	m_stateMachine.addState(ResolvedServicesSuperState, StartingRemoteControlServiceState, QStringLiteral("StartingRemoteControlServiceState"));
	m_stateMachine.addState(ResolvedServicesSuperState, ReadyState, QStringLiteral("Ready"));

	m_stateMachine.addState(StoppingState, QStringLiteral("Stopping"));


	// set the initial state of the state machine
	m_stateMachine.setInitialState(IdleState);


	// add the transitions:      From State                    ->   Event                       ->  To State
	m_stateMachine.addTransition(IdleState,                         StartServicesRequestEvent,      GettingGattServicesState);

	m_stateMachine.addTransition(GettingGattServicesState,          StopServicesRequestEvent,       IdleState);
	m_stateMachine.addTransition(GettingGattServicesState,          GotGattServicesEvent,           StartingDeviceInfoServiceState);

	m_stateMachine.addTransition(StartingDeviceInfoServiceState,    DeviceInfoServiceReadyEvent,    StartingBatteryServiceState);
	m_stateMachine.addTransition(StartingBatteryServiceState,       BatteryServiceReadyEvent,       StartingFindMeServiceState);
	m_stateMachine.addTransition(StartingFindMeServiceState,        FindMeServiceReadyEvent,        StartingAudioServiceState);
	m_stateMachine.addTransition(StartingAudioServiceState,         AudioServiceReadyEvent,         StartingInfraredServiceState);
	m_stateMachine.addTransition(StartingInfraredServiceState,      InfraredServiceReadyEvent,      StartingUpgradeServiceState);
//	m_stateMachine.addTransition(StartingInfraredServiceState,      InfraredServiceReadyEvent,      StartingTouchServiceState);
//	m_stateMachine.addTransition(StartingTouchServiceState,         TouchServiceReadyEvent,         StartingUpgradeServiceState);
	m_stateMachine.addTransition(StartingUpgradeServiceState,       UpgradeServiceReadyEvent,       StartingRemoteControlServiceState);
	m_stateMachine.addTransition(StartingRemoteControlServiceState, RemoteControlServiceReadyEvent, ReadyState);

	m_stateMachine.addTransition(ResolvedServicesSuperState,        StopServicesRequestEvent,       StoppingState);
	m_stateMachine.addTransition(StoppingState,                     ServicesStoppedEvent,           IdleState);


	// connect to the state entry and exit signals
	QObject::connect(&m_stateMachine, &StateMachine::entered,
	                 this, &GattServices::onEnteredState);
	QObject::connect(&m_stateMachine, &StateMachine::transition,
	                 this, &GattServices::onStateTransition);


	// start the state machine
	m_stateMachine.start();
}

// -----------------------------------------------------------------------------
/*!
	\overload

	Starts all the services for the remote device.

 */
bool GattServices::start()
{
	qInfo("starting services");

	// start the state machine
	m_stateMachine.postEvent(StartServicesRequestEvent);

	return true;
}

// -----------------------------------------------------------------------------
/*!
	\overload


 */
void GattServices::stop()
{
	qInfo("stopping services");

	m_stateMachine.postEvent(StopServicesRequestEvent);
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Private slot called on entry to a new state.
 */
void GattServices::onEnteredState(int state)
{
	switch (state) {
		case IdleState:
			onEnteredIdleState();
			break;
		case GettingGattServicesState:
			onEnteredGetGattServicesState();
			break;

		case StartingDeviceInfoServiceState:
			startService(m_deviceInfoService, DeviceInfoServiceReadyEvent);
			break;

		case StartingBatteryServiceState:
			startService(m_batteryService, BatteryServiceReadyEvent);
			break;

		case StartingFindMeServiceState:
			startService(m_findMeService, FindMeServiceReadyEvent);
			break;

		case StartingAudioServiceState:
			startService(m_audioService, AudioServiceReadyEvent);
			break;

		case StartingInfraredServiceState:
			startService(m_infraredService, InfraredServiceReadyEvent);
			break;

		case StartingUpgradeServiceState:
			startService(m_upgradeService, UpgradeServiceReadyEvent);
			break;

		case StartingRemoteControlServiceState:
			startService(m_remoteControlService, RemoteControlServiceReadyEvent);
			break;

		case ReadyState:
			emit ready();
			break;

		default:
			break;
	}
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Private slot called on all statemachine transitions.
 */
void GattServices::onStateTransition(int fromState, int toState)
{
	if (toState == StoppingState) {

		// if we're moving to the stopping state then we stop all the services
		// that have been started - this code assumes the start-up order of
		// the services matches the following switch statement

		switch (fromState) {
			case ReadyState:
			case StartingRemoteControlServiceState:
				m_remoteControlService->stop();
			case StartingUpgradeServiceState:
				m_upgradeService->stop();
			case StartingInfraredServiceState:
				m_infraredService->stop();
			case StartingAudioServiceState:
				m_audioService->stop();
			case StartingFindMeServiceState:
				m_findMeService->stop();
			case StartingBatteryServiceState:
				m_batteryService->stop();
			case StartingDeviceInfoServiceState:
				m_deviceInfoService->stop();
			default:
				break;
		}

		m_stateMachine.postEvent(ServicesStoppedEvent);
	}
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Called upon entry to the idle state ...
 */
void GattServices::onEnteredIdleState()
{
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Called on entry to the 'GetGattServices' state, this means that bluez has
	told us that it's resolved the gatt services and we just need to get
	all the dbus object paths to the gatt services / characteristics /
	descriptors.

 */
void GattServices::onEnteredGetGattServicesState()
{
	// request an update of all the gatt details from bluez / android, this will
	// emit the update signal when done which will trigger onGattProfileUpdated()
	m_gattProfile->updateProfile();
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Slot called when the GattBluezProfile object has gathered all the gatt data
	from bluez and we're ready to proceed to try and setup the services.

 */
void GattServices::onGattProfileUpdated()
{
	m_stateMachine.postEvent(GotGattServicesEvent);
}

// -----------------------------------------------------------------------------
/*!
	\internal

 */
template <typename T>
void GattServices::startService(const QSharedPointer<T> &service, QEvent::Type readyEvent)
{
	// get the service uuid and use it to try and find a matching bluez service
	// in the profile
	if (!service->isReady()) {

		QSharedPointer<BleGattService> gattService = m_gattProfile->service(T::uuid());
		if (!gattService || !gattService->isValid()) {
			if (T::uuid() == BleUuid(BleUuid::ComcastRemoteControl)) {
				qWarning() << "failed to find optional gatt service" << T::uuid() << ", ignoring...";
				m_stateMachine.postEvent(readyEvent);
			} else {
				qError() << "failed to find gatt service with uuid" << T::uuid();
			}
#if !defined(EC101_WORKAROUND_MISSING_IR_SERVICE)
			return;
#endif
		}

		// try and start the service
		if (!service->start(gattService)) {
			qError("failed to start service");
			return;
		}
	}

	// check if it's ready now
	if (service->isReady()) {
		m_stateMachine.postEvent(readyEvent);
		return;
	}

	// otherwise install a functor to deliver the 'ready' event to the
	// state machine when it becomes ready
	std::function<void()> functor = [this,readyEvent]() {
		m_stateMachine.postEvent(readyEvent);
	};
	QObject::connect(service.data(), &T::ready, this, functor);
}

// -----------------------------------------------------------------------------
/*!
	\reimp

 */
bool GattServices::isValid() const
{
	return true;
}

// -----------------------------------------------------------------------------
/*!
	\overload

 */
bool GattServices::isReady() const
{
	return m_stateMachine.inState(ReadyState);
}

// -----------------------------------------------------------------------------
/*!
	\overload

 */
QSharedPointer<BleRcuAudioService> GattServices::audioService() const
{
	return m_audioService;
}

// -----------------------------------------------------------------------------
/*!
	\overload

 */
QSharedPointer<BleRcuDeviceInfoService> GattServices::deviceInfoService() const
{
	return m_deviceInfoService;
}

// -----------------------------------------------------------------------------
/*!
	\overload

 */
QSharedPointer<BleRcuBatteryService> GattServices::batteryService() const
{
	return m_batteryService;
}

// -----------------------------------------------------------------------------
/*!
	\overload

 */
QSharedPointer<BleRcuFindMeService> GattServices::findMeService() const
{
	return m_findMeService;
}

// -----------------------------------------------------------------------------
/*!
	\overload

 */
QSharedPointer<BleRcuInfraredService> GattServices::infraredService() const
{
	return m_infraredService;
}

// -----------------------------------------------------------------------------
/*!
	\overload

	Currently the EC101 and EC102 are the only RCUs that use the GATT interface
	and neither have a touch interface so just return an empty service for
	them.

 */
QSharedPointer<BleRcuTouchService> GattServices::touchService() const
{
	return m_touchService;
}

// -----------------------------------------------------------------------------
/*!
	\overload

 	Returns a shared pointer to the upgrade service for this device.

 */
QSharedPointer<BleRcuUpgradeService> GattServices::upgradeService() const
{
	return m_upgradeService;
}

// -----------------------------------------------------------------------------
/*!
	\overload

 	Returns a shared pointer to the remote control service for this device.

 */
QSharedPointer<BleRcuRemoteControlService> GattServices::remoteControlService() const
{
	return m_remoteControlService;
}

// -----------------------------------------------------------------------------
/*!
	\overload

 	Debugging function that dumps out the state and details of all the services.

 */
void GattServices::dump(Dumper out) const
{
	out.printLine("state: %s",
	              qPrintable(m_stateMachine.stateName(m_stateMachine.state())));

	// TODO: dump out individual service states
}

