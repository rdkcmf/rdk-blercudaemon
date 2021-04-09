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
//  blercufindmeservice.h
//  SkyBluetoothRcu
//

#ifndef BLERCUFINDMESERVICE_H
#define BLERCUFINDMESERVICE_H

#include "utils/future.h"

#include <QObject>
#include <QSharedPointer>



class BleRcuFindMeService : public QObject
{
	Q_OBJECT

protected:
	explicit BleRcuFindMeService(QObject *parent)
		: QObject(parent)
	{ }

public:
	~BleRcuFindMeService() override = default;

public:
	enum State {
		BeepingOff = 0,
		BeepingMid = 1,
		BeepingHigh = 2
	};
#if QT_VERSION > QT_VERSION_CHECK(5, 4, 0)
	Q_ENUM(State)
#else
	Q_ENUMS(State)
#endif

	virtual State state() const = 0;

	enum Level {
		Mid = 1,
		High = 2
	};
#if QT_VERSION > QT_VERSION_CHECK(5, 4, 0)
	Q_ENUM(Level)
#else
	Q_ENUMS(Level)
#endif

	inline Future<> startBeeping(Level level)
	{
		return this->startBeeping(level, -1);
	}

	virtual Future<> startBeeping(Level level, int duration) = 0;
	virtual Future<> stopBeeping() = 0;

};


#endif // !defined(BLERCUFINDMESERVICE_H)
