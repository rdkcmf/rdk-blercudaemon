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
//  gatt_batteryservice.h
//  SkyBluetoothRcu
//

#ifndef GATT_BATTERYSERVICE_H
#define GATT_BATTERYSERVICE_H

#include "blercu/bleservices/blercubatteryservice.h"
#include "utils/bleuuid.h"
#include "utils/statemachine.h"

#include <QString>
#include <QTimer>
#include <QSharedPointer>


class BleGattService;
class BleGattCharacteristic;


class GattBatteryService : public BleRcuBatteryService
{
	Q_OBJECT

public:
	GattBatteryService();
	~GattBatteryService() final;

public:
	static BleUuid uuid();

public:
	bool isReady() const;

	int level() const override;

public:
	bool start(const QSharedPointer<const BleGattService> &gattService);
	void stop();

signals:
	void ready();

private:
	enum State {
		IdleState,
		StartNotifyState,
		StartingState,
		RunningState,
	};

	void init();

	int sanitiseBatteryLevel(char level) const;
	
private slots:
	void onEnteredState(int state);
	void onBatteryLevelChanged(const QByteArray &newValue);

	void onLogTimerTimeout();

private:
	void requestStartNotify();
	void requestBatteryLevel();

private:
	QSharedPointer<BleGattCharacteristic> m_battLevelCharacteristic;

	StateMachine m_stateMachine;

	int m_batteryLevel;
	int m_lastLoggedLevel;

	QTimer m_logTimer;

private:
	static const BleUuid m_serviceUuid;
	static const BleUuid m_batteryLevelCharUuid;

private:
	static const QEvent::Type StartServiceRequestEvent = QEvent::Type(QEvent::User + 1);
	static const QEvent::Type StopServiceRequestEvent  = QEvent::Type(QEvent::User + 2);
	static const QEvent::Type StartedNotifingEvent     = QEvent::Type(QEvent::User + 3);
	static const QEvent::Type ServiceReadyEvent        = QEvent::Type(QEvent::User + 4);
	static const QEvent::Type RetryStartNotifyEvent    = QEvent::Type(QEvent::User + 5);
};

#endif // !defined(GATT_BATTERYSERVICE_H)
