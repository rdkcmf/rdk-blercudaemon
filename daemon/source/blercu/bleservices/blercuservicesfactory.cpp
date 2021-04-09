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
//  blercuservicesfactory.cpp
//  SkyBluetoothRcu
//

#include "blercuservicesfactory.h"

#include "gatt/gatt_services.h"

#include "configsettings/configsettings.h"
#include "irdb/irdatabase.h"
#include "utils/logging.h"


// -----------------------------------------------------------------------------
/*!
	\class BleRcuServicesFactory
	\brief Factory class for creating BLE RCU service objects.

	The returned services object stores shared pointers to all the individual
	service objects attached to the given gatt profile.



 */



BleRcuServicesFactory::BleRcuServicesFactory(const QSharedPointer<const ConfigSettings> &config,
                                             const QSharedPointer<const IrDatabase> &irDatabase)
	: m_config(config)
	, m_irDatabase(irDatabase)
{
}

// -----------------------------------------------------------------------------
/*!
	Creates a \l{BleRcuServices} object and returns it.

	The \a address is the bluetooth mac address and the OUI of that is used to
	determine the services implementation to use.  For EC10x RCUs we use the
	GATT services, for the Ruwido RCUs we tunnel messages over the HID
	descriptors.


 */
QSharedPointer<BleRcuServices> BleRcuServicesFactory::createServices(const BleAddress &address,
                                                                     const QSharedPointer<BleGattProfile> &gattProfile,
																	 const QString &name)
{
	// use the OUI of the device to determine the flavor of services needed
	ConfigModelSettings settings = m_config->modelSettings(address.oui());
	if (Q_UNLIKELY(!settings.isValid())) {
		if (name.isEmpty()) {
			qError() << "no model settings for device with address " << address;
			return QSharedPointer<BleRcuServices>(nullptr);
		} else {
			// Not found based on OUI query, search by name instead
			settings = m_config->modelSettings(name);
			if (Q_UNLIKELY(!settings.isValid())) {
				qError() << "no model settings for device " << address << " with name " << name;
				return QSharedPointer<BleRcuServices>(nullptr);
			}
		}
	}

	// create the service type based on the communication method
	switch (settings.servicesType()) {

		case ConfigModelSettings::GattServiceType:
			return QSharedPointer<GattServices>::create(address, gattProfile, m_irDatabase);

		default:
			qError("service interface not supported");
			return QSharedPointer<BleRcuServices>(nullptr);
	}
}

