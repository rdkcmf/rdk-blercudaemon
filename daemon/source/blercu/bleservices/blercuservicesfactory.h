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
//  blercuservicesfactory.h
//  BleRcuDaemon
//

#ifndef BLERCUSERVICESFACTORY_H
#define BLERCUSERVICESFACTORY_H

#include "utils/bleaddress.h"

#include <QString>
#include <QSharedPointer>


class ConfigSettings;
class IrDatabase;
class IpcServicesFactory;

class BleRcuServices;
class BleGattProfile;


class BleRcuServicesFactory
{

public:
	BleRcuServicesFactory(const QSharedPointer<const ConfigSettings> &config,
	                      const QSharedPointer<const IrDatabase> &irDatabase);
	~BleRcuServicesFactory() = default;

public:
	QSharedPointer<BleRcuServices>
		createServices(const BleAddress &address,
		               const QSharedPointer<BleGattProfile> &gattProfile,
					   const QString &name="");

private:
	const QSharedPointer<const ConfigSettings> m_config;
	const QSharedPointer<const IrDatabase> m_irDatabase;
};


#endif // !defined(BLERCUSERVICESFACTORY_H)
