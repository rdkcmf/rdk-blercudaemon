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
//  blergattcharacteristic.h
//  SkyBluetoothRcu
//

#ifndef BLEGATTCHARACTERISTIC_H
#define BLEGATTCHARACTERISTIC_H


#include "utils/bleuuid.h"
#include "utils/future.h"

#include <QObject>
#include <QList>
#include <QByteArray>
#include <QSharedDataPointer>


class BleGattService;
class BleGattDescriptor;


class BleGattCharacteristic : public QObject
{
	Q_OBJECT

protected:
	BleGattCharacteristic(QObject *parent = nullptr)
		: QObject(parent)
	{ }

public:
	virtual ~BleGattCharacteristic()
	{ }

public:
	enum Flag {
		Broadcast = 0x0001,
		Read = 0x0002,
		WriteWithoutResponse = 0x0004,
		Write = 0x0008,
		Notify = 0x0010,
		Indicate = 0x0020,
		AuthenticatedSignedWrites = 0x0040,
		ReliableWrite = 0x0080,
		WritableAuxiliaries = 0x0100,
		EncryptRead = 0x0200,
		EncryptWrite = 0x0400,
		EncryptAuthenticatedRead = 0x0800,
		EncryptAuthenticatedWrite = 0x1000
	};
	Q_DECLARE_FLAGS(Flags, Flag)

public:
	virtual bool isValid() const = 0;
	virtual BleUuid uuid() const = 0;
	virtual int instanceId() const = 0;
	virtual Flags flags() const = 0;

	virtual void setCacheable(bool cacheable) = 0;
	virtual bool cacheable() const = 0;

	virtual QSharedPointer<BleGattService> service() const = 0;

	virtual QList< QSharedPointer<BleGattDescriptor> > descriptors() const = 0;
	virtual QSharedPointer<BleGattDescriptor> descriptor(BleUuid descUuid) const = 0;

	virtual Future<QByteArray> readValue() = 0;
	virtual Future<> writeValue(const QByteArray &value) = 0;
	virtual Future<> writeValueWithoutResponse(const QByteArray &value) = 0;

	virtual Future<> enableNotifications(bool enable) = 0;

	virtual int timeout() const = 0;
	virtual void setTimeout(int timeout) = 0;

signals:
	void valueChanged(const QByteArray &value);

};

Q_DECLARE_OPERATORS_FOR_FLAGS(BleGattCharacteristic::Flags)


QDebug operator<<(QDebug dbg, const BleGattCharacteristic &characteristic);


#endif // !defined(BLEGATTCHARACTERISTIC_H)
