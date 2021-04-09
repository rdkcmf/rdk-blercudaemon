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
//  blegattservice_p.h
//  SkyBluetoothRcu
//

#ifndef BLUEZ_BLEGATTSERVICE_P_H
#define BLUEZ_BLEGATTSERVICE_P_H

#include "../blegattservice.h"

#include <QMultiMap>
#include <QDBusConnection>
#include <QDBusObjectPath>
#include <QVariantMap>


class BleGattCharacteristicBluez;


class BleGattServiceBluez : public BleGattService
{
	Q_OBJECT

public:
	BleGattServiceBluez(const QDBusConnection &conn,
	                    const QString &path,
	                    const QVariantMap &properties,
	                    QObject *parent = nullptr);
	~BleGattServiceBluez();

public:
	bool isValid() const override;
	BleUuid uuid() const override;
	int instanceId() const override;
	bool primary() const override;

	QList< QSharedPointer<BleGattCharacteristic> > characteristics() const override;
	QList< QSharedPointer<BleGattCharacteristic> > characteristics(BleUuid charUuid) const override;
	QSharedPointer<BleGattCharacteristic> characteristic(BleUuid charUuid) const override;

private:
	friend class BleGattProfileBluez;

	void addCharacteristic(const QSharedPointer<BleGattCharacteristicBluez> &characteristic);

private:
	const QDBusObjectPath m_path;

	bool m_valid;
	bool m_primary;
	BleUuid m_uuid;
	int m_instanceId;

	QDBusObjectPath m_devicePath;

	QMultiMap<BleUuid, QSharedPointer<BleGattCharacteristicBluez>> m_characteristics;
};


#endif // !defined(BLUEZ_BLEGATTSERVICE_P_H)
