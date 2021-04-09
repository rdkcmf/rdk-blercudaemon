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
//  blegattservice.h
//  SkyBluetoothRcu
//

#ifndef BLEGATTSERVICE_H
#define BLEGATTSERVICE_H

#include "utils/bleuuid.h"

#include <QObject>
#include <QSharedPointer>
#include <QList>


class BleGattCharacteristic;


class BleGattService : public QObject
{
	Q_OBJECT

protected:
	BleGattService(QObject *parent = nullptr)
		: QObject(parent)
	{ }

public:
	virtual ~BleGattService()
	{ }

public:
	virtual bool isValid() const = 0;
	virtual BleUuid uuid() const = 0;
	virtual int instanceId() const = 0;
	virtual bool primary() const = 0;

	virtual QList< QSharedPointer<BleGattCharacteristic> > characteristics() const = 0;
	virtual QList< QSharedPointer<BleGattCharacteristic> > characteristics(BleUuid charUuid) const = 0;
	virtual QSharedPointer<BleGattCharacteristic> characteristic(BleUuid charUuid) const = 0;

};

QDebug operator<<(QDebug dbg, const BleGattService &service);


#endif // !defined(BLEGATTSERVICE_H)
