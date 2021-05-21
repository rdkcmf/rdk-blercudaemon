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
//  blercucontroller_p.h
//  SkyBluetoothRcu
//

#ifndef BLERCUCONTROLLER_P_H
#define BLERCUCONTROLLER_P_H

#include "blercucontroller.h"
#include "blercupairingstatemachine.h"
#include "blercuscannerstatemachine.h"

#include <QObject>
#include <QList>
#include <QSet>


class ConfigSettings;

class BleRcuAnalytics;
class BleRcuAdapter;
class BleRcuDevice;



class BleRcuControllerImpl final : public BleRcuController
{
	Q_OBJECT

public:
	BleRcuControllerImpl(const QSharedPointer<const ConfigSettings> &config,
	                     const QSharedPointer<BleRcuAdapter> &manager,
	                     QObject *parent = nullptr);
	~BleRcuControllerImpl() final;

public:
	void dump(Dumper out) const override;

	bool isValid() const override;
	State state() const override;

	BleRcuError lastError() const override;

	bool isPairing() const override;
	int pairingCode() const override;

	bool startPairing(quint8 filterByte, quint8 pairingCode) override;
	bool startPairingMacHash(quint8 filterByte, quint8 macHash) override;

	bool cancelPairing() override;

	bool isScanning() const override;
	bool startScanning(int timeoutMs) override;
	bool cancelScanning() override;

	QSet<BleAddress> managedDevices() const override;
	QSharedPointer<BleRcuDevice> managedDevice(const BleAddress &address) const override;

	bool unpairDevice(const BleAddress &address) const override;

private:
	void syncManagedDevices();
	void removeLastConnectedDevice();

private slots:
	void onStartedPairing();
	void onFinishedPairing();
	void onFailedPairing();
	void onInitialised();
	void onDevicePairingChanged(const BleAddress &address, bool paired);
	void onDeviceReadyChanged(const BleAddress &address, bool ready);

	void onStartedScanning();
	void onFinishedScanning();
	void onFailedScanning();
	void onFoundPairableDevice(const BleAddress &address, const QString &name);

private:
	const QSharedPointer<const ConfigSettings> m_config;
	const QSharedPointer<BleRcuAdapter> m_adapter;
	const QSharedPointer<BleRcuAnalytics> m_analytics;

	QSet<quint8> m_supportedFilterBytes;

	BleRcuPairingStateMachine m_pairingStateMachine;
	BleRcuScannerStateMachine m_scannerStateMachine;

	QSet<BleAddress> m_managedDevices;

	BleRcuError m_lastError;

	int m_maxManagedDevices;

	State m_state;

	bool m_ignoreScannerSignal;
};

#endif // !defined(BLERCUCONTROLLER_P_H)
