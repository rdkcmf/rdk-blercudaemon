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
//  blercuremotecontrolservice.h
//  SkyBluetoothRcu
//

#ifndef BLERCUREMOTECONTROLSERVICE_H
#define BLERCUREMOTECONTROLSERVICE_H

#include "utils/future.h"
#include <QObject>


class BleRcuRemoteControlService : public QObject
{
	Q_OBJECT

protected:
	explicit BleRcuRemoteControlService(QObject *parent)
		: QObject(parent)
	{ }

public:
	~BleRcuRemoteControlService() override = default;

public:
	virtual quint8 unpairReason() const = 0;
	virtual quint8 rebootReason() const = 0;
	virtual quint8 lastKeypress() const = 0;
	virtual Future<> sendRcuAction(quint8 action) = 0;

signals:
	void unpairReasonChanged(quint8 reason);
	void rebootReasonChanged(quint8 reason);
	void lastKeypressChanged(quint8 key);
};

#endif // !defined(BLERCUREMOTECONTROLSERVICE_H)
