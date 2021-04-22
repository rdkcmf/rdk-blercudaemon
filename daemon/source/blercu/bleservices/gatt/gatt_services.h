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
//  gatt_services.h
//  SkyBluetoothRcu
//

#ifndef GATT_SERVICES_H
#define GATT_SERVICES_H

#include "blercu/bleservices/blercuservices.h"
#include "utils/bleaddress.h"
#include "utils/statemachine.h"


class BleGattProfile;

class IrDatabase;

class GattAudioService;
class GattDeviceInfoService;
class GattBatteryService;
class GattFindMeService;
class GattInfraredService;
class GattTouchService;
class GattUpgradeService;
class GattRemoteControlService;



class GattServices : public BleRcuServices
{
	Q_OBJECT

public:
	GattServices(const BleAddress &address,
	             const QSharedPointer<BleGattProfile> &gattProfile,
	             const QSharedPointer<const IrDatabase> &irDatabase,
	             QObject *parent = nullptr);
	~GattServices() final;

public:
	void dump(Dumper out) const override;

	bool isValid() const override;
	bool isReady() const override;

	bool start() override;
	void stop() override;

	QSharedPointer<BleRcuAudioService> audioService() const override;
	QSharedPointer<BleRcuDeviceInfoService> deviceInfoService() const override;
	QSharedPointer<BleRcuBatteryService> batteryService() const override;
	QSharedPointer<BleRcuFindMeService> findMeService() const override;
	QSharedPointer<BleRcuInfraredService> infraredService() const override;
	QSharedPointer<BleRcuTouchService> touchService() const override;
	QSharedPointer<BleRcuUpgradeService> upgradeService() const override;
	QSharedPointer<BleRcuRemoteControlService> remoteControlService() const override;

private:
	template <typename T>
	void startService(const QSharedPointer<T> &service, QEvent::Type readyEvent);

private slots:
	void onEnteredState(int state);
	void onStateTransition(int fromState, int toState);

	void onGattProfileUpdated();

private:
	enum State {
		IdleState,
		GettingGattServicesState,
		ResolvedServicesSuperState,
			StartingDeviceInfoServiceState,
			StartingBatteryServiceState,
			StartingFindMeServiceState,
			StartingAudioServiceState,
			StartingInfraredServiceState,
			StartingTouchServiceState,
			StartingUpgradeServiceState,
			StartingRemoteControlServiceState,
			ReadyState,
		StoppingState
	};

	void init();

	void onEnteredIdleState();
	void onEnteredResolvingGattServicesState();
	void onEnteredGetGattServicesState();

private:
	const BleAddress m_address;
	const QSharedPointer<BleGattProfile> m_gattProfile;
	const QSharedPointer<const IrDatabase> m_irDatabase;

	StateMachine m_stateMachine;

	QSharedPointer<GattAudioService> m_audioService;
	QSharedPointer<GattDeviceInfoService> m_deviceInfoService;
	QSharedPointer<GattBatteryService> m_batteryService;
	QSharedPointer<GattFindMeService> m_findMeService;
	QSharedPointer<GattInfraredService> m_infraredService;
	QSharedPointer<GattTouchService> m_touchService;
	mutable QSharedPointer<GattUpgradeService> m_upgradeService;
	QSharedPointer<GattRemoteControlService> m_remoteControlService;

private:
	static const QEvent::Type StartServicesRequestEvent = QEvent::Type(QEvent::User + 1);
	static const QEvent::Type StopServicesRequestEvent = QEvent::Type(QEvent::User + 2);
	static const QEvent::Type GotGattServicesEvent = QEvent::Type(QEvent::User + 3);

	static const QEvent::Type DeviceInfoServiceReadyEvent = QEvent::Type(QEvent::User + 4);
	static const QEvent::Type BatteryServiceReadyEvent = QEvent::Type(QEvent::User + 5);
	static const QEvent::Type FindMeServiceReadyEvent = QEvent::Type(QEvent::User + 6);
	static const QEvent::Type AudioServiceReadyEvent = QEvent::Type(QEvent::User + 7);
	static const QEvent::Type InfraredServiceReadyEvent = QEvent::Type(QEvent::User + 8);
	static const QEvent::Type TouchServiceReadyEvent = QEvent::Type(QEvent::User + 9);
	static const QEvent::Type UpgradeServiceReadyEvent = QEvent::Type(QEvent::User + 10);
	static const QEvent::Type RemoteControlServiceReadyEvent = QEvent::Type(QEvent::User + 11);
	static const QEvent::Type ServicesStoppedEvent = QEvent::Type(QEvent::User + 12);

};


#endif // IPC_SERVICES_H
