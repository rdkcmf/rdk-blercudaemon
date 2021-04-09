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
//  blercuanalytics.cpp
//  SkyBluetoothRcu
//

#include "blercuanalytics.h"
#include "configsettings/configsettings.h"
#include "utils/logging.h"



// -----------------------------------------------------------------------------
/*!
	\class BleRcuAnalytics
	\brief Simple object to log the state of the daemon for analytics

	Very little to this class, just here as a recipient of signals from the
	controller for logging purposes.

 */



BleRcuAnalytics::BleRcuAnalytics(const QSharedPointer<const ConfigSettings> &config,
                                 QObject *parent)
	: QObject(parent)
{
	// construct the map of BDADDR OUI to RCU type
	const QList<ConfigModelSettings> models = config->modelSettings();
	for (const ConfigModelSettings &model : models)
		m_ouiToType[model.oui()] = model.name();

}

BleRcuAnalytics::~BleRcuAnalytics()
{
}

// -----------------------------------------------------------------------------
/*!
	Logs the addition of a BLE RCU device

 */
void BleRcuAnalytics::logDeviceAdded(const BleAddress &address)
{
	const quint32 oui = address.oui();

	if (!m_ouiToType.contains(oui)) {
		qProdLog("Unknown bluetooth RCU added");
		return;
	}

	qProdLog("%s bluetooth RCU added", qPrintable(m_ouiToType[oui]));
}

// -----------------------------------------------------------------------------
/*!
	Logs the removal of a BLE RCU device

 */
void BleRcuAnalytics::logDeviceRemoved(const BleAddress &address)
{
	const quint32 oui = address.oui();

	if (!m_ouiToType.contains(oui)) {
		qProdLog("Unknown bluetooth RCU removed");
		return;
	}

	qProdLog("%s bluetooth RCU removed", qPrintable(m_ouiToType[oui]));
}

// -----------------------------------------------------------------------------
/*!
	Logs the state changes of the pairing state machine.

 */
void BleRcuAnalytics::logPairingStateChange(bool pairing)
{
	if (pairing)
		qProdLog("started bluetooth RCU pairing procedure");
	else
		qProdLog("finished bluetooth RCU pairing procedure");
}

