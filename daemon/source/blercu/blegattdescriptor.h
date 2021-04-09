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
//  blergattdescriptor.h
//  SkyBluetoothRcu
//

#ifndef BLEGATTDESCRIPTOR_H
#define BLEGATTDESCRIPTOR_H


#include "utils/bleuuid.h"
#include "utils/future.h"

#include <QObject>
#include <QByteArray>
#include <QSharedDataPointer>


class BleGattCharacteristic;


class BleGattDescriptor : public QObject
{
	Q_OBJECT

protected:
	BleGattDescriptor(QObject *parent = nullptr)
		: QObject(parent)
	{ }

public:
	virtual ~BleGattDescriptor()
	{ }

public:
	enum Flag {
		Read = 0x001,
		Write = 0x002,
		EncryptRead = 0x004,
		EncryptWrite = 0x008,
		EncryptAuthenticatedRead = 0x010,
		EncryptAuthenticatedWrite = 0x020
	};
	Q_DECLARE_FLAGS(Flags, Flag)

public:
	virtual bool isValid() const = 0;
	virtual BleUuid uuid() const = 0;
	virtual Flags flags() const = 0;

	virtual void setCacheable(bool cacheable) = 0;
	virtual bool cacheable() const = 0;

	virtual Future<QByteArray> readValue() = 0;
	virtual Future<void> writeValue(const QByteArray &value) = 0;

	virtual int timeout() const = 0;
	virtual void setTimeout(int timeout) = 0;

	virtual QSharedPointer<BleGattCharacteristic> characteristic() const = 0;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(BleGattDescriptor::Flags)


QDebug operator<<(QDebug dbg, const BleGattDescriptor &descriptor);


#endif // !defined(BLEGATTDESCRIPTOR_H)
