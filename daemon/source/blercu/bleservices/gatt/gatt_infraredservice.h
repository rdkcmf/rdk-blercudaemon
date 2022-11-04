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
//  gatt_infraredservice.h
//  SkyBluetoothRcu
//

#ifndef GATT_INFRAREDSERVICE_H
#define GATT_INFRAREDSERVICE_H

#include "blercu/bleservices/blercuinfraredservice.h"
#include "blercu/blercuerror.h"
#include "utils/bleuuid.h"
#include "utils/statemachine.h"
#include "utils/futureaggregator.h"
#include "configsettings/configsettings.h"
#include "gatt_deviceinfoservice.h"

#include <QMap>
#include <QEvent>
#include <QByteArray>



class IrDatabase;

class BleGattService;
class BleGattCharacteristic;

class GattInfraredSignal;


class GattInfraredService : public BleRcuInfraredService
{
	Q_OBJECT

public:
	explicit GattInfraredService(const QSharedPointer<const IrDatabase> &irDatabase,
							  const ConfigModelSettings &settings,
							  const QSharedPointer<const GattDeviceInfoService> &deviceInfo);
	~GattInfraredService() final;

public:
	static BleUuid uuid();

public:
	bool isReady() const;

public:
	bool start(const QSharedPointer<BleGattService> &gattService);
	void stop();

signals:
	void ready();

public:
	qint32 codeId() const override;

	Future<> eraseIrSignals() override;
	Future<> programIrSignals(qint32 codeId,
	                          const QSet<Qt::Key> &keyCodes) override;
	Future<> programIrSignalWaveforms(const QMap<Qt::Key, QByteArray> &irWaveforms) override;

	Future<> emitIrSignal(Qt::Key keyCode) override;

	Future<SearchResults> brands(const QString &search,
	                             SearchOptions options,
	                             qint64 offset,
	                             qint64 limit) const override;
	Future<SearchResults> models(const QString &brand,
	                             const QString &search,
	                             SearchOptions options,
	                             qint64 offset,
	                             qint64 limit) const override;

	Future<IrCodeList> codeIds(const QString &brand,
	                           const QString &model,
	                           SearchOptions options) const override;
	Future<IrCodeList> codeIds(const QByteArray &edid) const override;

private:
	enum State {
		IdleState,
		StartingSuperState,
			SetStandbyModeState,
			GetCodeIdState,
			GetIrSignalsState,
		RunningState,
	};

	void init();

private slots:
	void onEnteredState(int state);
	void onIrSignalReady();

	void onEnteredIdleState();
	void onEnteredSetStandbyModeState();
	void onEnteredGetCodeIdState();
	void onEnteredGetIrSignalsState();

private:
	void getSignalCharacteristics(const QSharedPointer<BleGattService> &gattService);

	template<typename T = void>
	Future<T> createErrorResult(BleRcuError::ErrorType type,
	                            const QString &message) const;

	quint8 keyCodeToGattValue(Qt::Key keyCode) const;

	QMap<Qt::Key, QByteArray> getIrSignalData(qint32 codeId,
	                                          const QSet<Qt::Key> &keyCodes) const;

	Future<> writeCodeIdValue(qint32 codeId);

private:
	const QSharedPointer<const IrDatabase> m_irDatabase;
	const QSharedPointer<const GattDeviceInfoService> m_deviceInfo;

	enum StandbyMode {
		StandbyModeB,
		StandbyModeC
	};

	StandbyMode m_irStandbyMode;

	QSharedPointer<BleGattCharacteristic> m_standbyModeCharacteristic;
	QSharedPointer<BleGattCharacteristic> m_codeIdCharacteristic;
	QSharedPointer<BleGattCharacteristic> m_emitIrCharacteristic;

	QList< QSharedPointer<GattInfraredSignal> > m_irSignals;

	StateMachine m_stateMachine;
	
	qint32 m_codeId;

	QSharedPointer<FutureAggregator> m_outstandingOperation;


private:
	static const BleUuid m_serviceUuid;

private:
	static const QEvent::Type StartServiceRequestEvent = QEvent::Type(QEvent::User + 1);
	static const QEvent::Type StopServiceRequestEvent = QEvent::Type(QEvent::User + 2);

	static const QEvent::Type SetIrStandbyModeEvent = QEvent::Type(QEvent::User + 3);
	static const QEvent::Type ReceivedCodeIdEvent = QEvent::Type(QEvent::User + 4);
	static const QEvent::Type IrSignalsReadyEvent = QEvent::Type(QEvent::User + 5);

	
};

#endif // !defined(GATT_INFRAREDSERVICE_H)
