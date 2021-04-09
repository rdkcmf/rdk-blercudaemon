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
//  blegattprofile.h
//  SkyBluetoothRcu
//

#ifndef BLEGATTPROFILE_H
#define BLEGATTPROFILE_H

#include "utils/bleuuid.h"

#include <QObject>
#include <QSharedPointer>
#include <QDebug>
#include <QList>


class BleGattService;


class BleGattProfile : public QObject
{
	Q_OBJECT

protected:
	BleGattProfile(QObject *parent = nullptr)
		: QObject(parent)
	{ }

public:
	virtual ~BleGattProfile()
	{ }

public:
	virtual bool isValid() const = 0;
	virtual bool isEmpty() const = 0;

	virtual void updateProfile() = 0;

	virtual QList< QSharedPointer<BleGattService> > services() const = 0;
	virtual QList< QSharedPointer<BleGattService> > services(const BleUuid &serviceUuid) const = 0;
	virtual QSharedPointer<BleGattService> service(const BleUuid &serviceUuid) const = 0;

signals:
	void updateCompleted();
};


QDebug operator<<(QDebug dbg, const BleGattProfile &profile);


#endif // !defined(BLEGATTPROFILE_H)
