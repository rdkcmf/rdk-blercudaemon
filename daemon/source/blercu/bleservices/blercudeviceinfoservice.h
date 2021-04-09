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
//  blercudeviceinfoservice.h
//  SkyBluetoothRcu
//

#ifndef BLERCUDEVICEINFOSERVICE_H
#define BLERCUDEVICEINFOSERVICE_H

#include "utils/future.h"

#include <QObject>
#include <QString>
#include <QSharedPointer>


class BleRcuDeviceInfoService : public QObject
{
	Q_OBJECT

protected:
	explicit BleRcuDeviceInfoService(QObject *parent)
		: QObject(parent)
	{ }

public:
	~BleRcuDeviceInfoService() override = default;

public:
	enum PnPVendorSource {
		Invalid = 0,
		Bluetooth = 1,
		USB = 2
	};
#if QT_VERSION > QT_VERSION_CHECK(5, 4, 0)
	Q_ENUM(PnPVendorSource)
#else
	Q_ENUMS(PnPVendorSource)
#endif

	virtual Future<qint16> rssi() const = 0;

	virtual QString manufacturerName() const = 0;
	virtual QString modelNumber() const = 0;
	virtual QString serialNumber() const = 0;
	virtual QString hardwareRevision() const = 0;
	virtual QString firmwareVersion() const = 0;
	virtual QString softwareVersion() const = 0;
	virtual quint64 systemId() const = 0;

	virtual PnPVendorSource pnpVendorIdSource() const = 0;
	virtual quint16 pnpVendorId() const = 0;
	virtual quint16 pnpProductId() const = 0;
	virtual quint16 pnpProductVersion() const = 0;

signals:
	void manufacturerNameChanged(const QString &name);
	void modelNumberChanged(const QString &name);
	void serialNumberChanged(const QString &name);
	void hardwareRevisionChanged(const QString &name);
	void firmwareVersionChanged(const QString &name);
	void softwareVersionChanged(const QString &name);

};

#endif // !defined(BLERCUDEVICEINFOSERVICE_H)
