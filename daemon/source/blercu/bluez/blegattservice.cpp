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
//  blergattservice.cpp
//  SkyBluetoothRcu
//

#include "blegattservice_p.h"
#include "blegattcharacteristic_p.h"

#include <QDBusObjectPath>


// -----------------------------------------------------------------------------
/*!
	Constructs a new object parsing the details of the GATT service from the
	dictionary received from bluez.

	The following is an example of the \a properties we get over dbus when a
	service is found

	\startcode

		dict entry(
			string "org.bluez.GattService1"
			array [
				dict entry(
					string "UUID"
					variant                   string "00010001-bdf0-407c-aaff-d09967f31acd"
				)
				dict entry(
					string "Device"
					variant                   object path "/org/bluez/hci0/dev_1C_A2_B1_BE_EF_02"
				)
				dict entry(
					string "Primary"
					variant                   boolean true
				)
				dict entry(
					string "Includes"
					variant                   array [
					]
				)
			]
		)

	\endcode

 */
BleGattServiceBluez::BleGattServiceBluez(const QDBusConnection &conn,
                                         const QString &path,
                                         const QVariantMap &properties,
                                         QObject *parent)
	: BleGattService(parent)
	, m_path(path)
	, m_valid(false)
	, m_primary(false)
	, m_instanceId(0)
{

	// get the uuid of the service
	const QVariant uuidVar = properties[QStringLiteral("UUID")];
	if (!uuidVar.canConvert<QUuid>()) {
		qWarning() << "invalid uuid property of gatt service" << uuidVar;
		return;
	}

	m_uuid = BleUuid(uuidVar.value<QUuid>());

	// get the parent device object path (only used for sanity checking)
	const QVariant devicePathVar = properties[QStringLiteral("Device")];
	if (devicePathVar.canConvert<QDBusObjectPath>())
		m_devicePath = devicePathVar.value<QDBusObjectPath>();
	else
		qWarning() << "failed to get the device path of the service with uuid"
		           << m_uuid;


	// check if this is a primary service
	const QVariant primaryVar = properties[QStringLiteral("Primary")];
	if (primaryVar.canConvert<bool>())
		m_primary = primaryVar.value<bool>();


	// the instance id is used to distinguish between different instances of the
	// same service, for bluez we use the dbus path id, ie. a typical dbus
	// path would be '/org/bluez/hci0/dev_D4_B8_FF_11_76_EE/service0043' we
	// trim off the last bit and parse the 0043 as the instance id
	QString serviceId = path.section('/', -1);
	if (sscanf(qPrintable(serviceId), "service%04x", &m_instanceId) != 1) {
		qWarning("failed to parse service '%s' to get the instance id",
		         qPrintable(serviceId));
		m_instanceId = -1;
	}

	m_valid = true;
}

BleGattServiceBluez::~BleGattServiceBluez()
{

}

// -----------------------------------------------------------------------------
/*!
	\fn bool BleGattService::isValid() const

	Returns \c true if the object was successifully created from the java / JNI
	class object.

 */
bool BleGattServiceBluez::isValid() const
{
	return m_valid;
}

// -----------------------------------------------------------------------------
/*!
	\fn BleUuid BleGattService::uuid() const

	Returns the UUID of the service.  If the service is not valid the returned
	value is undefined.

 */
BleUuid BleGattServiceBluez::uuid() const
{
	return m_uuid;
}

// -----------------------------------------------------------------------------
/*!
	\fn BleUuid BleGattService::instanceId() const

	Returns the instance ID for this service. If a remote device offers multiple
	service with the same UUID, the instance ID is used to distuinguish between
	services.

 */
int BleGattServiceBluez::instanceId() const
{
	return m_instanceId;
}

// -----------------------------------------------------------------------------
/*!
	\fn bool BleGattService::primary() const

	Returns \c true if this service is the primary service.

 */
bool BleGattServiceBluez::primary() const
{
	return m_primary;
}

// -----------------------------------------------------------------------------
/*!

 */
QList< QSharedPointer<BleGattCharacteristic> > BleGattServiceBluez::characteristics() const
{
	QList< QSharedPointer<BleGattCharacteristic> > characteristics;
	characteristics.reserve(m_characteristics.size());

	for (const QSharedPointer<BleGattCharacteristicBluez> &characteristic : m_characteristics)
		characteristics.append(characteristic);

	return characteristics;
}

// -----------------------------------------------------------------------------
/*!

 */
QList< QSharedPointer<BleGattCharacteristic> > BleGattServiceBluez::characteristics(BleUuid charUuid) const
{
	QList< QSharedPointer<BleGattCharacteristic> > characteristics;
	characteristics.reserve(m_characteristics.size());

	for (const QSharedPointer<BleGattCharacteristicBluez> &characteristic : m_characteristics) {
		if (characteristic->uuid() == charUuid)
			characteristics.append(characteristic);
	}

	return characteristics;
}

// -----------------------------------------------------------------------------
/*!

 */
QSharedPointer<BleGattCharacteristic> BleGattServiceBluez::characteristic(BleUuid charUuid) const
{
	return qSharedPointerCast<BleGattCharacteristic>(m_characteristics.value(charUuid));
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Adds the characteristic to the service, basically just inserts it into the
	internal multimap.

 */
void BleGattServiceBluez::addCharacteristic(const QSharedPointer<BleGattCharacteristicBluez> &characteristic)
{
	m_characteristics.insertMulti(characteristic->uuid(), characteristic);
}

