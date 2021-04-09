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
//  blegattprofile_p.h
//  SkyBluetoothRcu
//

#ifndef BLUEZ_BLEGATTPROFILE_P_H
#define BLUEZ_BLEGATTPROFILE_P_H

#include "../blegattprofile.h"

#include <QMultiMap>
#include <QVariantMap>
#include <QVersionNumber>
#include <QDBusConnection>
#include <QDBusObjectPath>
#include <QDBusPendingCallWatcher>


class BleGattManager;
class BleGattServiceBluez;


class BleGattProfileBluez : public BleGattProfile
{
	Q_OBJECT

public:
	BleGattProfileBluez(const QDBusConnection &bluezDBusConn,
	                    const QDBusObjectPath &bluezDBusPath,
	                    QObject *parent = nullptr);
	~BleGattProfileBluez() final;

public:
	bool isValid() const override;
	bool isEmpty() const override;

	void updateProfile() override;

	QList< QSharedPointer<BleGattService> > services() const override;
	QList< QSharedPointer<BleGattService> > services(const BleUuid &serviceUuid) const override;
	QSharedPointer<BleGattService> service(const BleUuid &serviceUuid) const override;

private slots:
	void onGetObjectsReply(QDBusPendingCallWatcher *call);

private:
	void updateBluezVersion(const QVariantMap &properties);
	void dumpGattTree();

private:
	const QDBusConnection m_dbusConn;
	const QDBusObjectPath m_dbusPath;

	QVersionNumber m_bluezVersion;

	bool m_valid;
	QMultiMap<BleUuid, QSharedPointer<BleGattServiceBluez>> m_services;
};



#endif // !defined(BLUEZ_BLEGATTPROFILE_P_H)
