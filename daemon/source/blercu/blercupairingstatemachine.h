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
//  blercucpairingstatemachine.h
//  SkyBluetoothRcu
//

#ifndef BLERCUPAIRINGSTATEMACHINE_H
#define BLERCUPAIRINGSTATEMACHINE_H

#include "utils/bleaddress.h"
#include "utils/statemachine.h"
#include "utils/dumper.h"

#include "btrmgradapter.h"

#include <QObject>
#include <QTimer>
#include <QSharedPointer>
#include <QMap>
#include <QRegExp>
#include <QVector>


class BleRcuAdapter;
class ConfigSettings;


class BleRcuPairingStateMachine : public QObject
{
	Q_OBJECT

public:
	enum State {
		RunningSuperState,
			DiscoverySuperState,
				StartingDiscoveryState,
				DiscoveringState,
			PairingSuperState,
				StoppingDiscoveryState,
				EnablePairableState,
				PairingState,
				SetupState,
			UnpairingState,
			StoppingDiscoveryStartedExternally,
		FinishedState
	};
#if QT_VERSION > QT_VERSION_CHECK(5, 4, 0)
	Q_ENUM(State)
#else
	Q_ENUMS(State)
#endif

public:
	BleRcuPairingStateMachine(const QSharedPointer<const ConfigSettings> &config,
	                          const QSharedPointer<BleRcuAdapter> &adapter,
	                          QObject *parent = nullptr);
	~BleRcuPairingStateMachine();

public:
	void dump(Dumper out) const;
	bool isRunning() const;
	int pairingCode() const;

public slots:
	void start(quint8 filterByte, quint8 pairingCode);
	void start(const BleAddress &target, const QString &name);
	void startMacHash(quint8 filterByte, quint8 macHash);
	void stop();


signals:
	void started();
	void finished();
	void failed();

private slots:
	void onDiscoveryChanged(bool discovering);
	void onPairableChanged(bool pairable);

	void onDeviceFound(const BleAddress &address, const QString &name);
	void onDeviceRemoved(const BleAddress &address);
	void onDeviceNameChanged(const BleAddress &address, const QString &name);
	void onDevicePairingChanged(const BleAddress &address, bool paired);
	void onDeviceReadyChanged(const BleAddress &address, bool ready);

	void onAdapterPoweredChanged(bool powered);

	void onDiscoveryTimeout();
	void onPairingTimeout();
	void onSetupTimeout();
	void onUnpairingTimeout();

private slots:
	void onStateEntry(int state);
	void onStateExit(int state);

	void onEnteredStartDiscoveryState();
	void onEnteredDiscoveringState();
	void onExitedDiscoverySuperState();

	void onEnteredStoppingDiscoveryState();
	void onEnteredEnablePairableState();
	void onEnteredPairingState();
	void onEnteredSetupState();
	void onExitedPairingSuperState();

	void onEnteredUnpairingState();
	void onExitedUnpairingState();

	void onEnteredFinishedState();

	void onEnteredStoppingDiscoveryStartedExternally();


private:
	void setupStateMachine();
	void processDevice(const BleAddress &address, const QString &name);

private:
	const QSharedPointer<BleRcuAdapter> m_adapter;

	QVector<QByteArray> m_pairingPrefixFormats;

	int m_pairingCode;
	int m_pairingMacHash;
	QVector<QRegExp> m_supportedPairingNames;

	BleAddress m_targetAddress;

	StateMachine m_stateMachine;

	QTimer m_discoveryTimer;
	QTimer m_pairingTimer;
	QTimer m_setupTimer;
	QTimer m_unpairingTimer;

	int m_pairingAttempts;
	int m_pairingSuccesses;
	bool m_pairingSucceeded;

	BtrMgrAdapter m_btrMgrAdapter;
	bool discoveryStartedExternally = false;
	BtrMgrAdapter::OperationType lastOperationType = BtrMgrAdapter::unknownOperation;

private:
	static const QEvent::Type DiscoveryStartedEvent      = QEvent::Type(QEvent::User + 1);
	static const QEvent::Type DiscoveryStoppedEvent      = QEvent::Type(QEvent::User + 2);
	static const QEvent::Type DiscoveryStartTimeoutEvent = QEvent::Type(QEvent::User + 3);
	static const QEvent::Type DiscoveryStopTimeoutEvent  = QEvent::Type(QEvent::User + 4);

	static const QEvent::Type PairableEnabledEvent       = QEvent::Type(QEvent::User + 5);
	static const QEvent::Type PairableDisabledEvent      = QEvent::Type(QEvent::User + 6);

	static const QEvent::Type PairingTimeoutEvent        = QEvent::Type(QEvent::User + 7);
	static const QEvent::Type SetupTimeoutEvent          = QEvent::Type(QEvent::User + 8);
	static const QEvent::Type UnpairingTimeoutEvent      = QEvent::Type(QEvent::User + 9);

	static const QEvent::Type DeviceFoundEvent           = QEvent::Type(QEvent::User + 10);
	static const QEvent::Type DeviceUnpairedEvent        = QEvent::Type(QEvent::User + 11);
	static const QEvent::Type DeviceRemovedEvent         = QEvent::Type(QEvent::User + 12);
	static const QEvent::Type DevicePairedEvent          = QEvent::Type(QEvent::User + 13);
	static const QEvent::Type DeviceReadyEvent           = QEvent::Type(QEvent::User + 14);

	static const QEvent::Type AdapterPoweredOffEvent     = QEvent::Type(QEvent::User + 15);

};


#endif // !defined(BLERCUPAIRINGSTATEMACHINE_H)
