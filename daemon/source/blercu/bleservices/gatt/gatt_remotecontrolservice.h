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
//  gatt_remotecontrolservice.h
//  SkyBluetoothRcu
//

#ifndef GATT_REMOTECONTROLSERVICE_H
#define GATT_REMOTECONTROLSERVICE_H

#include "blercu/bleservices/blercuremotecontrolservice.h"
#include "utils/bleuuid.h"
#include "utils/statemachine.h"

#include <QString>
#include <QTimer>
#include <QSharedPointer>


class BleGattService;
class BleGattCharacteristic;


class GattRemoteControlService : public BleRcuRemoteControlService
{
	Q_OBJECT

public:
	GattRemoteControlService();
	~GattRemoteControlService() final;

public:
	static BleUuid uuid();

public:
	bool isReady() const;

	quint8 unpairReason() const override;
	quint8 rebootReason() const override;
	quint8 lastKeypress() const override;
	quint8 advConfig() const override;
	QByteArray advConfigCustomList() const override;
	Future<> sendRcuAction(quint8 action) override;
	Future<> writeAdvertisingConfig(quint8 config, const QByteArray &customList) override;


public:
	bool start(const QSharedPointer<const BleGattService> &gattService);
	void stop();

signals:
	void ready();

private:
	enum State {
		IdleState,
		StartReadLastKeypressState,
		StartUnpairNotifyState,
		StartRebootNotifyState,
		StartingState,
		RunningState,
	};

	void init();

private slots:
	void onEnteredState(int state);
	void onUnpairReasonChanged(const QByteArray &newValue);
	void onRebootReasonChanged(const QByteArray &newValue);

private:
	void requestStartUnpairNotify();
	void requestStartRebootNotify();
	void requestUnpairReason();
	void requestRebootReason();
	void requestLastKeypress();
	void requestAdvConfig();
	void requestAdvConfigCustomList();
	
	void onRcuActionReply();
	void onRcuActionError(const QString &errorName, const QString &errorMessage);
	void onWriteAdvConfigReply();
	void onWriteAdvConfigError(const QString &errorName, const QString &errorMessage);
	void onWriteCustomConfigReply();
	void onWriteCustomConfigError(const QString &errorName, const QString &errorMessage);

private:
	QSharedPointer<BleGattCharacteristic> m_unpairReasonCharacteristic;
	QSharedPointer<BleGattCharacteristic> m_rebootReasonCharacteristic;
	QSharedPointer<BleGattCharacteristic> m_rcuActionCharacteristic;
	QSharedPointer<BleGattCharacteristic> m_lastKeypressCharacteristic;
	QSharedPointer<BleGattCharacteristic> m_advConfigCharacteristic;
	QSharedPointer<BleGattCharacteristic> m_advConfigCustomListCharacteristic;

	StateMachine m_stateMachine;

	quint8 m_unpairReason;
	quint8 m_rebootReason;
	quint8 m_rcuAction;
	quint8 m_lastKeypress;
	quint8 m_advConfig;
	QByteArray m_advConfigCustomList;

	QSharedPointer< Promise<> > m_promiseResults;

private:
	static const BleUuid m_serviceUuid;
	static const BleUuid m_unpairReasonCharUuid;
	static const BleUuid m_rebootReasonCharUuid;
	static const BleUuid m_rcuActionCharUuid;
	static const BleUuid m_lastKeypressCharUuid;
	static const BleUuid m_advConfigCharUuid;
	static const BleUuid m_advConfigCustomListCharUuid;

private:
	static const QEvent::Type StartServiceRequestEvent = QEvent::Type(QEvent::User + 1);
	static const QEvent::Type StopServiceRequestEvent  = QEvent::Type(QEvent::User + 2);
	static const QEvent::Type StartedNotifingEvent     = QEvent::Type(QEvent::User + 3);
	static const QEvent::Type ServiceReadyEvent        = QEvent::Type(QEvent::User + 4);
	static const QEvent::Type RetryStartNotifyEvent    = QEvent::Type(QEvent::User + 5);
};

#endif // !defined(GATT_RemoteControlService_H)
