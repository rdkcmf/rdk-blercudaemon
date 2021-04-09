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
//  gatt_findmeservice.h
//  SkyBluetoothRcu
//

#ifndef GATT_FINDMESERVICE_H
#define GATT_FINDMESERVICE_H

#include "../blercufindmeservice.h"
#include "utils/bleuuid.h"
#include "utils/statemachine.h"

#include <QEvent>



class BleGattService;
class BleGattCharacteristic;


class GattFindMeService : public BleRcuFindMeService
{
	Q_OBJECT

public:
	GattFindMeService();
	~GattFindMeService() final;

public:
	static BleUuid uuid();

public:
	bool isReady() const;

	BleRcuFindMeService::State state() const override;

	Future<> startBeeping(Level level, int duration) override;
	Future<> stopBeeping() override;

public:
	bool start(const QSharedPointer<const BleGattService> &gattService);
	void stop();

signals:
	void ready();

private:
	enum State {
		IdleState,
		StartingState,
		RunningState,
	};

	void init();

	Future<> setFindMeLevel(quint8 level);

private:
	void onEnteredState(int state);
	void onExitedState(int state);

	void onEnteredStartingState();

	void onFindMeRequestReply();
	void onFindMeRequestError(const QString &errorName,
	                          const QString &errorMessage);

private:
	QSharedPointer<BleGattCharacteristic> m_alertLevelCharacteristic;

	StateMachine m_stateMachine;

	QSharedPointer< Promise<> > m_promiseResults;

	quint8 m_level;

private:
	static const BleUuid m_serviceUuid;

private:
	static const QEvent::Type StartServiceRequestEvent = QEvent::Type(QEvent::User + 1);
	static const QEvent::Type StopServiceRequestEvent = QEvent::Type(QEvent::User + 2);
	static const QEvent::Type ServiceReadyEvent = QEvent::Type(QEvent::User + 3);
};


#endif // !defined(GATT_FINDMESERVICE_H)
