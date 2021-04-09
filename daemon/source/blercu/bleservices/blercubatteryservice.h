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
//  blercubatteryservice.h
//  SkyBluetoothRcu
//

#ifndef BLERCUBATTERYSERVICE_H
#define BLERCUBATTERYSERVICE_H

#include <QObject>


class BleRcuBatteryService : public QObject
{
	Q_OBJECT

protected:
	explicit BleRcuBatteryService(QObject *parent)
		: QObject(parent)
	{ }

public:
	~BleRcuBatteryService() override = default;

public:
	virtual int level() const = 0;

signals:
	void levelChanged(int level);
};

#endif // !defined(BLERCUBATTERYSERVICE_H)
