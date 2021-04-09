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
//  blercutouchservice.h
//  SkyBluetoothRcu
//

#ifndef BLERCUTOUCHSERVICE_H
#define BLERCUTOUCHSERVICE_H

#include "utils/future.h"

#include <QFlags>
#include <QObject>
#include <QString>
#include <QSharedPointer>


class BleRcuTouchService : public QObject
{
	Q_OBJECT

protected:
	explicit BleRcuTouchService(QObject *parent)
		: QObject(parent)
	{ }

public:
	~BleRcuTouchService() override = default;

public:
	enum TouchModeOption {
		TrackPadEnabled = 0x1,
		SliderEnabled = 0x2,
	};
	Q_DECLARE_FLAGS(TouchModeOptions, TouchModeOption)
#if QT_VERSION <= QT_VERSION_CHECK(5, 4, 0)
	Q_FLAGS(TouchModeOptions)
#else
	Q_FLAG(TouchModeOptions)
#endif

	virtual bool modeSettable() const = 0;

	virtual TouchModeOptions mode() const = 0;
	virtual Future<> setMode(TouchModeOptions mode) = 0;

signals:
	void modeSettabilityChanged(bool settable);
	void modeChanged(TouchModeOptions mode);

};

Q_DECLARE_OPERATORS_FOR_FLAGS(BleRcuTouchService::TouchModeOptions)


#endif // !defined(BLERCUTOUCHSERVICE_H)
