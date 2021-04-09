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
//  blegattcharacteristic_p.h
//  SkyBluetoothRcu
//

#ifndef BLUEZ_BLEGATTCHARACTERISTIC_P_H
#define BLUEZ_BLEGATTCHARACTERISTIC_P_H

#include "../blegattcharacteristic.h"

#include <QDBusConnection>
#include <QDBusObjectPath>
#include <QVersionNumber>
#include <QVariantMap>


class BleGattNotifyPipe;
class BleGattDescriptorBluez;
class BluezGattCharacteristicInterface;

class QDBusPendingCallWatcher;


class BleGattCharacteristicBluez : public BleGattCharacteristic
{
	Q_OBJECT

public:
	BleGattCharacteristicBluez(const QDBusConnection &conn,
	                           const QString &path,
	                           const QVariantMap &properties,
	                           QObject *parent = nullptr);
	~BleGattCharacteristicBluez() final;

public:
	bool isValid() const override;
	BleUuid uuid() const override;
	int instanceId() const override;
	Flags flags() const override;

	void setCacheable(bool cacheable) override;
	bool cacheable() const override;

	QSharedPointer<BleGattService> service() const override;

	QList< QSharedPointer<BleGattDescriptor> > descriptors() const override;
	QSharedPointer<BleGattDescriptor> descriptor(BleUuid descUuid) const override;

	Future<QByteArray> readValue() override;
	Future<> writeValue(const QByteArray &value) override;
	Future<> writeValueWithoutResponse(const QByteArray &value) override;

	Future<> enableNotifications(bool enable) override;

	int timeout() const override;
	void setTimeout(int timeout) override;

	void setBluezVersion(const QVersionNumber &bluezVersion);

private slots:
	void onNotificationEnableReply(QDBusPendingCallWatcher *watcher,
	                               QSharedPointer<Promise<>> promise);
	void onNotifyPipeClosed();

private:
	friend class BleGattProfileBluez;

	void addDescriptor(const QSharedPointer<BleGattDescriptorBluez> &descriptor);

private:
	const QDBusObjectPath m_path;

	QWeakPointer<BleGattService> m_service;
	QDBusObjectPath m_servicePath;

	QSharedPointer<BluezGattCharacteristicInterface> m_proxy;

	bool m_valid;
	Flags m_flags;
	BleUuid m_uuid;
	int m_instanceId;

	bool m_useNewDBusApi;

	QSharedPointer<BleGattNotifyPipe> m_notifyPipe;

	QMap<BleUuid, QSharedPointer<BleGattDescriptorBluez>> m_descriptors;
};



#endif // !defined(BLUEZ_BLEGATTCHARACTERISTIC_P_H)
