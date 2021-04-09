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
//  blercudevice_p.h
//  SkyBluetoothRcu
//

#ifndef BLUEZ_BLERCUDEVICE_P_H
#define BLUEZ_BLERCUDEVICE_P_H

#include "../blercudevice.h"
#include "utils/bleuuid.h"
#include "utils/statemachine.h"

#include <QObject>
#include <QString>
#include <QList>
#include <QTimer>
#include <QElapsedTimer>
#include <QSharedPointer>
#include <QReadWriteLock>

#include <QDBusConnection>
#include <QDBusObjectPath>
#include <QDBusPendingCallWatcher>


class BleRcuServicesFactory;

class DeviceInfoService;
class BatteryService;
class InfraredService;
class FindMeService;
class AudioService;
class TouchService;

class BleRcuServices;
class BleGattProfileBluez;

class BluezDeviceInterface;
class DBusAbstractAdaptor;


class BleRcuDeviceBluez : public BleRcuDevice
{
	Q_OBJECT

public:
	enum State {
		IdleState,
		PairedState,
		ConnectedState,
		ResolvingServicesState,
		RecoverySuperState,
			RecoveryDisconnectingState,
			RecoveryReconnectingState,
		SetupSuperState,
			StartingServicesState,
			ReadyState
	};
#if QT_VERSION > QT_VERSION_CHECK(5, 4, 0)
	Q_ENUM(State)
#else
	Q_ENUMS(State)
#endif

public:
	BleRcuDeviceBluez(const BleAddress &bdaddr,
	                  const QString &name,
	                  const QDBusConnection &bluezDBusConn,
	                  const QDBusObjectPath &bluezDBusPath,
	                  const QSharedPointer<BleRcuServicesFactory> &servicesFactory,
	                  QObject *parent = nullptr);
	~BleRcuDeviceBluez() final;

public:
	void dump(Dumper out) const override;

	bool isValid() const override;

	BleAddress address() const override;
	QString name() const override;

	bool isConnected() const override;
	bool isPairing() const override;
	bool isPaired() const override;
	bool isReady() const override;

	qint64 msecsSinceReady() const override;

//	Future<qint16> rssi() const override;

public:
	QDBusObjectPath bluezObjectPath() const;

	void pair(int timeout);
	void cancelPairing();

	void block();
	void unblock();

public:
	QSharedPointer<BleRcuAudioService> audioService() const override;
	QSharedPointer<BleRcuBatteryService> batteryService() const override;
	QSharedPointer<BleRcuDeviceInfoService> deviceInfoService() const override;
	QSharedPointer<BleRcuFindMeService> findMeService() const override;
	QSharedPointer<BleRcuInfraredService> infraredService() const override;
	QSharedPointer<BleRcuTouchService> touchService() const override;
	QSharedPointer<BleRcuUpgradeService> upgradeService() const override;

private:
	bool init(const QDBusConnection &bluezDBusConn,
	          const QDBusObjectPath &bluezDBusPath);

	void setupStateMachine();

	void getInitialDeviceProperties();

private slots:
	void onDeviceConnectedChanged(bool connected);
	void onDevicePairedChanged(bool paired);
	void onDeviceNameChanged(const QString &name);
	void onDeviceServicesResolvedChanged(bool resolved);

	void onPairRequestReply(QDBusPendingCallWatcher *call);
	void onCancelPairingRequestReply(QDBusPendingCallWatcher *call);


private:
	void onEnteredState(int state);
	void onExitedState(int state);

	void onEnteredReadyState();
	void onExitedReadyState();

	void onExitedSetupSuperState();

	void onEnteredRecoveryDisconnectingState();
	void onEnteredRecoveryReconnectingState();

	void onEnteredResolvingServicesState();
	void onEnteredStartingServicesState();

	void onServicesReady();


private:
	static const QEvent::Type DeviceConnectedEvent        = QEvent::Type(QEvent::User + 1);
	static const QEvent::Type DeviceDisconnectedEvent     = QEvent::Type(QEvent::User + 2);
	static const QEvent::Type DevicePairedEvent           = QEvent::Type(QEvent::User + 3);
	static const QEvent::Type DeviceUnpairedEvent         = QEvent::Type(QEvent::User + 4);

	static const QEvent::Type ServicesResolvedEvent       = QEvent::Type(QEvent::User + 5);
	static const QEvent::Type ServicesNotResolvedEvent    = QEvent::Type(QEvent::User + 6);
	static const QEvent::Type ServicesStartedEvent        = QEvent::Type(QEvent::User + 7);

	static const QEvent::Type ServicesResolveTimeoutEvent = QEvent::Type(QEvent::User + 8);


private:
	QSharedPointer<BluezDeviceInterface> m_deviceProxy;

	QSharedPointer<BleGattProfileBluez> m_gattProfile;
	QSharedPointer<BleRcuServices> m_services;


private:
	const QDBusObjectPath m_bluezObjectPath;
	const BleAddress m_address;

	QString m_name;

	bool m_lastConnectedState;
	bool m_lastPairedState;
	bool m_lastServicesResolvedState;
	bool m_isPairing;

	QElapsedTimer m_timeSinceReady;

	int m_recoveryAttempts;
	const int m_maxRecoveryAttempts;

private:
	StateMachine m_stateMachine;
};


#endif // !defined(BLUEZ_BLERCUDEVICE_P_H)
