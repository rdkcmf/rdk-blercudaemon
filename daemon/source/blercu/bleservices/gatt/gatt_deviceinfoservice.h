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
//  gatt_deviceinfoservice.h
//  SkyBluetoothRcu
//

#ifndef GATT_DEVICEINFOSERVICE_H
#define GATT_DEVICEINFOSERVICE_H

#include "blercu/bleservices/blercudeviceinfoservice.h"
#include "utils/statemachine.h"
#include "utils/bleaddress.h"
#include "utils/bleuuid.h"

#include <QMap>
#include <QLatin1String>


class BleGattProfile;
class BleGattService;

class GattDeviceInfoService : public BleRcuDeviceInfoService
{
	Q_OBJECT

public:
	GattDeviceInfoService();
	~GattDeviceInfoService() final;

public:
	static BleUuid uuid();

public:
	bool isReady() const;

public:
	bool start(const QSharedPointer<const BleGattService> &gattService);
	void stop();

	void forceRefresh();
	
signals:
	void ready();

public:
	Future<qint16> rssi() const override;

public:
	QString manufacturerName() const override;
	QString modelNumber() const override;
	QString serialNumber() const override;
	QString hardwareRevision() const override;
	QString firmwareVersion() const override;
	QString softwareVersion() const override;
	quint64 systemId() const override;

public:
	PnPVendorSource pnpVendorIdSource() const override;
	quint16 pnpVendorId() const override;
	quint16 pnpProductId() const override;
	quint16 pnpProductVersion() const override;

private:
	enum State {
		IdleState,
		InitialisingState,
		RunningState,
		StoppedState
	};

	void init();

public:
	enum InfoField {
		ManufacturerName  = (0x1 << 0),
		ModelNumber       = (0x1 << 1),
		SerialNumber      = (0x1 << 2),
		HardwareRevision  = (0x1 << 3),
		FirmwareVersion   = (0x1 << 4),
		SoftwareVersion   = (0x1 << 5),
		SystemId          = (0x1 << 6),
		PnPId             = (0x1 << 7),
	};
	Q_DECLARE_FLAGS(InfoFieldFlags, InfoField)

private:
	void onEnteredState(int state);
	void onExitedState(int state);

	void sendCharacteristicReadRequest(InfoField field);
	void onCharacteristicReadSuccess(const QByteArray &data, InfoField field);
	void onCharacteristicReadError(const QString &error, const QString &message,
	                               InfoField field);

private:
	void setManufacturerName(const QByteArray &value);
	void setModelNumber(const QByteArray &value);
	void setSerialNumber(const QByteArray &value);
	void setHardwareRevision(const QByteArray &value);
	void setFirmwareVersion(const QByteArray &value);
	void setSoftwareVersion(const QByteArray &value);
	void setSystemId(const QByteArray &value);
	void setPnPId(const QByteArray &value);

private:
	bool m_forceRefresh;

	QSharedPointer<const BleGattService> m_gattService;

	StateMachine m_stateMachine;

	struct StateHandler {
		BleUuid uuid;
		void (GattDeviceInfoService::*handler)(const QByteArray &value);
	};

	static const QMap<InfoField, StateHandler> m_stateHandler;

	InfoFieldFlags m_infoFlags;

private:
	QString m_manufacturerName;
	QString m_modelNumber;
	QString m_serialNumber;
	QString m_hardwareRevision;
	QString m_firmwareVersion;
	QString m_softwareVersion;
	quint64 m_systemId;

	quint8 m_vendorIdSource;
	quint16 m_vendorId;
	quint16 m_productId;
	quint16 m_productVersion;

private:
	static const BleUuid m_serviceUuid;

private:
	static const QEvent::Type StartServiceRequestEvent = QEvent::Type(QEvent::User + 1);
	static const QEvent::Type StartServiceForceRefreshRequestEvent = QEvent::Type(QEvent::User + 2);
	static const QEvent::Type StopServiceRequestEvent = QEvent::Type(QEvent::User + 3);
	static const QEvent::Type InitialisedEvent = QEvent::Type(QEvent::User + 4);

};

Q_DECLARE_OPERATORS_FOR_FLAGS(GattDeviceInfoService::InfoFieldFlags)


#endif // !defined(GATT_DEVICEINFOSERVICE_H)
