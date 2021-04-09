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
//  gatt_audioservice.h
//  SkyBluetoothRcu
//

#ifndef GATT_AUDIOSERVICE_H
#define GATT_AUDIOSERVICE_H

#include "blercu/bleservices/blercuaudioservice.h"
#include "blercu/blercuerror.h"
#include "utils/bleuuid.h"

#include "utils/statemachine.h"

#include <QString>
#include <QByteArray>


class BleGattService;
class BleGattCharacteristic;

class GattAudioPipe;


class GattAudioService : public BleRcuAudioService
{
	Q_OBJECT

public:
	GattAudioService();
	~GattAudioService() final;

public:
	static BleUuid uuid();

public:
	bool isReady() const;

public:
	bool start(const QSharedPointer<const BleGattService> &gattService);
	void stop();

signals:
	void ready();

public:
	bool isStreaming() const override;

	qint32 gainLevel() const override;
	void setGainLevel(qint32 level) override;

	Future<FileDescriptor> startStreaming(Encoding encoding) override;
	Future<> startStreamingTo(Encoding encoding, int pipeWriteFd) override;
	Future<> stopStreaming() override;

	Future<StatusInfo> status() override;

private:
	enum State {
		IdleState,
		ReadyState,
		StreamingSuperState,
			EnableNotificationsState,
			StartStreamingState,
			StreamingState,
			StopStreamingState,
			CancelStreamingState
	};

	void init();

	bool getAudioCodecsCharacteristic(const QSharedPointer<const BleGattService> &gattService);
	bool getAudioGainCharacteristic(const QSharedPointer<const BleGattService> &gattService);
	bool getAudioControlCharacteristic(const QSharedPointer<const BleGattService> &gattService);
	bool getAudioDataCharacteristic(const QSharedPointer<const BleGattService> &gattService);

	template<typename T = void>
	Future<T> createErrorResult(BleRcuError::ErrorType type,
	                            const QString &message) const;

private slots:
	void onEnteredState(int state);
	void onExitedState(int state);

	void onEnteredEnableNotificationsState();
	void onEnteredStartStreamingState();
	void onEnteredStreamingState();
	void onExitedStreamingState();
	void onEnteredStopStreamingState();

	void onExitedStreamingSuperState();

	void onAudioDataNotification(const QByteArray &value);

	void onOutputPipeClosed();

private:
	const int m_packetsPerFrame;
	
private:
	QSharedPointer<Promise<FileDescriptor>> m_startStreamingPromise;
	QSharedPointer<Promise<>> m_startStreamingToPromise;
	QSharedPointer<Promise<>> m_stopStreamingPromise;


private:
	StatusInfo m_lastStats;

	QSharedPointer<BleGattCharacteristic> m_audioGainCharacteristic;
	QSharedPointer<BleGattCharacteristic> m_audioCtrlCharacteristic;
	QSharedPointer<BleGattCharacteristic> m_audioDataCharacteristic;

	StateMachine m_stateMachine;
	qint64 m_timeoutEventId;

	qint32 m_gainLevel;

	QSharedPointer<GattAudioPipe> m_audioPipe;

private:
	static const BleUuid m_serviceUuid;

private:
	static const QEvent::Type StartServiceRequestEvent = QEvent::Type(QEvent::User + 1);
	static const QEvent::Type StopServiceRequestEvent = QEvent::Type(QEvent::User + 2);

	static const QEvent::Type StartStreamingRequestEvent = QEvent::Type(QEvent::User + 3);
	static const QEvent::Type StopStreamingRequestEvent = QEvent::Type(QEvent::User + 4);

	static const QEvent::Type NotificationsEnabledEvent = QEvent::Type(QEvent::User + 5);

	static const QEvent::Type StreamingStartedEvent = QEvent::Type(QEvent::User + 6);
	static const QEvent::Type StreamingStoppedEvent = QEvent::Type(QEvent::User + 7);

	static const QEvent::Type GattErrorEvent = QEvent::Type(QEvent::User + 8);
	static const QEvent::Type OutputPipeCloseEvent = QEvent::Type(QEvent::User + 9);

};

#endif // !defined(GATT_AUDIOSERVICE_H)
