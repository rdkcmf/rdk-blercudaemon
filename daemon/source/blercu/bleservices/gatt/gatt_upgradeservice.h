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
//  gatt_upgradeservice.h
//  SkyBluetoothRcu
//

#ifndef GATT_UPGRADESERVICE_H
#define GATT_UPGRADESERVICE_H

#include "blercu/bleservices/blercuupgradeservice.h"
#include "blercu/blercuerror.h"
#include "utils/bleuuid.h"

#include "utils/statemachine.h"

#include <QFile>
#include <QTimer>
#include <QString>
#include <QSharedPointer>


class BleGattService;
class BleGattCharacteristic;
class BleGattDescriptor;


class GattUpgradeService : public BleRcuUpgradeService
{
	Q_OBJECT

public:
	GattUpgradeService();
	~GattUpgradeService() final;

public:
	static BleUuid uuid();

public:
	bool isReady() const;

public:
	bool start(const QSharedPointer<const BleGattService> &gattService);
	void stop();

signals:
	void ready();
	void upgradeComplete();

public:
	Future<> startUpgrade(const QSharedPointer<FwImageFile> &fwFile) override;
	Future<> cancelUpgrade() override;

	bool upgrading() const override;
	int progress() const override;

private:
	enum State {
		InitialState,
		SendingSuperState,
			SendingWriteRequestState,
			SendingDataState,
		ErroredState,
		FinishedState
	};

	void init();

	enum SetupFlag {
		EnabledNotifications = 0x01,
		ReadWindowSize = 0x02,
		VerifiedDeviceModel = 0x04,
	};
	Q_DECLARE_FLAGS(SetupFlags, SetupFlag)

private slots:
	void onStateEntry(int state);
	void onStateExit(int state);

	void onEnteredInitialState();
	void onEnteredSendWriteRequestState();
	void onEnteredSendingDataState();
	void onEnteredErroredState();
	void onEnteredFinishedState();

private slots:
	void onTimeout();
	void onPacketNotification(const QByteArray &value);

private:
	Future<> createFutureError(BleRcuError::ErrorType type,
	                           const QString &message) const;

	void enablePacketNotifications();
	void readControlPoint();
	void readWindowSize();

	void setSetupFlag(SetupFlag flag);

	void doPacketWrite(const QByteArray &value);

	void sendWRQ();
	void sendDATA();

	void onACKPacket(const quint8 data[2]);
	void onERRORPacket(const quint8 data[2]);

private:
	bool m_ready;

	QSharedPointer<BleGattCharacteristic> m_controlCharacteristic;
	QSharedPointer<BleGattCharacteristic> m_packetCharacteristic;
	QSharedPointer<BleGattDescriptor> m_windowSizeDescriptor;

	SetupFlags m_setupFlags;

	int m_progress;

	uint m_windowSize;

	QSharedPointer<FwImageFile> m_fwFile;

	QSharedPointer< Promise<> > m_startPromise;

	QTimer m_timeoutTimer;

	StateMachine m_stateMachine;

private:
	int m_lastAckBlockId;

	int m_timeoutCounter;

	QString m_lastError;

private:
	static const QEvent::Type CancelledEvent     = QEvent::Type(QEvent::User + 1);
	static const QEvent::Type TimeoutErrorEvent  = QEvent::Type(QEvent::User + 2);
	static const QEvent::Type EnableNotifyErrorEvent = QEvent::Type(QEvent::User + 3);
	static const QEvent::Type WriteErrorEvent    = QEvent::Type(QEvent::User + 4);
	static const QEvent::Type ReadErrorEvent     = QEvent::Type(QEvent::User + 5);
	static const QEvent::Type PacketErrorEvent   = QEvent::Type(QEvent::User + 6);
	static const QEvent::Type StopServiceEvent   = QEvent::Type(QEvent::User + 7);

	static const QEvent::Type FinishedSetupEvent = QEvent::Type(QEvent::User + 8);
	static const QEvent::Type PacketAckEvent     = QEvent::Type(QEvent::User + 9);
	static const QEvent::Type CompleteEvent      = QEvent::Type(QEvent::User + 10);

};


#endif // !defined(GATT_UPGRADESERVICE_H)


