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
//  blercuanalytics.h
//  SkyBluetoothRcu
//

#ifndef BLERCUANALYTICS_H
#define BLERCUANALYTICS_H

#include "utils/bleaddress.h"

#include <QObject>
#include <QMap>
#include <QString>
#include <QSharedPointer>


class ConfigSettings;


class BleRcuAnalytics : public QObject
{
	Q_OBJECT

public:
	BleRcuAnalytics(const QSharedPointer<const ConfigSettings> &config,
	                QObject *parent = nullptr);
	~BleRcuAnalytics();

public slots:
	void logDeviceAdded(const BleAddress &address);
	void logDeviceRemoved(const BleAddress &address);
	void logPairingStateChange(bool pairing);

private:
	QMap<quint32, QString> m_ouiToType;
};

#endif // !defined(BLERCUANALYTICS_H)
