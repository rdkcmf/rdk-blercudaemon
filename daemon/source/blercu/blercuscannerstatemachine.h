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
//  blercucscannerstatemachine.h
//  SkyBluetoothRcu
//

#ifndef BLERCUSCANNERSTATEMACHINE_H
#define BLERCUSCANNERSTATEMACHINE_H

#include "utils/bleaddress.h"
#include "utils/statemachine.h"
#include "utils/dumper.h"

#include <QObject>
#include <QElapsedTimer>
#include <QSharedPointer>
#include <QMap>
#include <QRegExp>
#include <QVector>


class BleRcuAdapter;
class ConfigSettings;


class BleRcuScannerStateMachine : public QObject
{
Q_OBJECT

public:
	enum State {
		RunningSuperState,
			StartingDiscoveryState,
			DiscoveringState,
			StoppingDiscoveryState,
		FinishedState
	};
#if QT_VERSION > QT_VERSION_CHECK(5, 4, 0)
	Q_ENUM(State)
#else
	Q_ENUMS(State)
#endif

public:
	BleRcuScannerStateMachine(const QSharedPointer<const ConfigSettings> &config,
	                          const QSharedPointer<BleRcuAdapter> &adapter,
	                          QObject *parent = nullptr);
	~BleRcuScannerStateMachine() final;

public:
	void dump(Dumper out) const;
	bool isRunning() const;

public slots:
	void start(int timeoutMs);
	void stop();

signals:
	void started();
	void finished();
	void failed();

	void foundPairableDevice(const BleAddress &address, const QString &name);

private slots:
	void onDiscoveryChanged(bool discovering);

	void onDeviceFound(const BleAddress &address, const QString &name);
	void onDeviceNameChanged(const BleAddress &address, const QString &name);

	void onAdapterPoweredChanged(bool powered);

	void onDiscoveryStartTimeout();
	void onDiscoveryTimeout();

private slots:
	void onStateEntry(int state);
	void onStateExit(int state);

	void onEnteredStartDiscoveryState();
	void onEnteredDiscoveringState();
	void onEnteredStopDiscoveryState();
	void onEnteredFinishedState();


private:
	void setupStateMachine();
	void processDevice(const BleAddress &address, const QString &name);

	QString formatTimeDuration(qint64 millis) const;

private:
	const QSharedPointer<BleRcuAdapter> m_adapter;

	QMap<quint32, QRegExp> m_deviceNameMatchers;
	QVector<QRegExp> m_supportedPairingNames;

	StateMachine m_stateMachine;

	int m_scanTimeoutMs;

	QElapsedTimer m_scanElapsedTime;

	struct {
		BleAddress address;
		QString name;

		bool isNull() const {
			return address.isNull();
		}

		void clear() {
			address.clear();
			name.clear();
		}

	} m_foundDevice;

private:
	static const QEvent::Type DiscoveryStartedEvent      = QEvent::Type(QEvent::User + 1);
	static const QEvent::Type DiscoveryStoppedEvent      = QEvent::Type(QEvent::User + 2);
	static const QEvent::Type DiscoveryTimeoutEvent      = QEvent::Type(QEvent::User + 3);

	static const QEvent::Type DeviceFoundEvent           = QEvent::Type(QEvent::User + 4);

	static const QEvent::Type DiscoveryStartTimeoutEvent = QEvent::Type(QEvent::User + 5);
	static const QEvent::Type DiscoveryStopTimeoutEvent  = QEvent::Type(QEvent::User + 6);

	static const QEvent::Type CancelRequestEvent         = QEvent::Type(QEvent::User + 7);

	static const QEvent::Type AdapterPoweredOffEvent     = QEvent::Type(QEvent::User + 8);
};



#endif // !defined(BLERCUSCANNERSTATEMACHINE_H)
