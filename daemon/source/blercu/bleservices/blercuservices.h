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
//  blercuservices.h
//  SkyBluetoothRcu
//

#ifndef BLERCUSERVICES_H
#define BLERCUSERVICES_H

#include <QObject>
#include <QSharedPointer>

#include "utils/dumper.h"


class BleRcuAudioService;
class BleRcuBatteryService;
class BleRcuDeviceInfoService;
class BleRcuFindMeService;
class BleRcuInfraredService;
class BleRcuTouchService;
class BleRcuUpgradeService;


class BleRcuServices : public QObject
{
	Q_OBJECT

protected:
	explicit BleRcuServices(QObject *parent)
		: QObject(parent)
	{ }

public:
	~BleRcuServices() override = default;

public:
	virtual void dump(Dumper out) const = 0;

	virtual bool isValid() const = 0;
	virtual bool isReady() const = 0;

	virtual bool start() = 0;
	virtual void stop() = 0;

	virtual QSharedPointer<BleRcuAudioService> audioService() const = 0;
	virtual QSharedPointer<BleRcuDeviceInfoService> deviceInfoService() const = 0;
	virtual QSharedPointer<BleRcuBatteryService> batteryService() const = 0;
	virtual QSharedPointer<BleRcuFindMeService> findMeService() const = 0;
	virtual QSharedPointer<BleRcuInfraredService> infraredService() const = 0;
	virtual QSharedPointer<BleRcuTouchService> touchService() const = 0;
	virtual QSharedPointer<BleRcuUpgradeService> upgradeService() const = 0;

signals:
	void ready();

};


#endif // !defined(BLERCUSERVICES_H)
