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
//  blegattdescriptor_p.h
//  SkyBluetoothRcu
//

#ifndef BLUEZ_BLEGATTDESCRIPTOR_P_H
#define BLUEZ_BLEGATTDESCRIPTOR_P_H

#include "../blegattdescriptor.h"

#include <QVariantMap>
#include <QDBusConnection>
#include <QDBusObjectPath>


class BluezGattDescriptorInterface;


class BleGattDescriptorBluez : public BleGattDescriptor {
Q_OBJECT

public:
	BleGattDescriptorBluez(const QDBusConnection &conn,
	                       const QString &path,
	                       const QVariantMap &properties,
	                       QObject *parent = nullptr);

	~BleGattDescriptorBluez();

public:
	bool isValid() const override;

	BleUuid uuid() const override;

	Flags flags() const override;

	void setCacheable(bool cacheable) override;

	bool cacheable() const override;

	QSharedPointer<BleGattCharacteristic> characteristic() const override;

	Future<QByteArray> readValue() override;

	Future<void> writeValue(const QByteArray &value) override;

	int timeout() const override;

	void setTimeout(int timeout) override;


private:
	friend class BleGattProfileBluez;

	const QDBusObjectPath m_path;

	QWeakPointer<BleGattCharacteristic> m_characteristic;
	QDBusObjectPath m_characteristicPath;

	QSharedPointer<BluezGattDescriptorInterface> m_proxy;

	bool m_valid;
	Flags m_flags;
	BleUuid m_uuid;

	bool m_cacheable;
	QByteArray m_lastValue;

};


#endif // !defined(BLUEZ_BLEGATTDESCRIPTOR_P_H)
