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
//  blegattdescriptor.cpp
//  SkyBluetoothRcu
//

#include "blegattdescriptor_p.h"
#include "blegatthelpers.h"

#include "interfaces/bluezgattdescriptorinterface.h"

#include "utils/logging.h"


// -----------------------------------------------------------------------------
/*!
	\internal

	Constructs a new object parsing the details of the GATT descriptor from
	the dictionary received from bluez over dbus.

	The following is an example of the \a properties we get over dbus when a
	service is found

	\startcode

		dict entry(
			string "org.bluez.GattDescriptor1"
			array [
				dict entry(
					string "UUID"
					variant                   string "00030003-bdf0-407c-aaff-d09967f31acd"
				)
				dict entry(
					string "Characteristic"
					variant                   object path "/org/bluez/hci0/dev_1C_A2_B1_BE_EF_02/service003d/char0040"
				)
				dict entry(
					string "Value"
					variant                   array [
					]
				)
			]
		)

	\endcode
 */
BleGattDescriptorBluez::BleGattDescriptorBluez(const QDBusConnection &conn,
                                               const QString &path,
                                               const QVariantMap &properties,
                                               QObject *parent)
	: BleGattDescriptor(parent)
	, m_path(path)
	, m_valid(false)
	, m_flags(0)
	, m_cacheable(false)
{
	// get the uuid of the service
	const QVariant uuidVar = properties[QStringLiteral("UUID")];
	if (!uuidVar.canConvert<QUuid>()) {
		qWarning() << "invalid uuid property of gatt descriptor" << uuidVar;
		return;
	}

	m_uuid = BleUuid(uuidVar.value<QUuid>());


	// get the parent characteristic object path (used for constructing the tree)
	const QVariant charPathVar = properties[QStringLiteral("Characteristic")];
	if (charPathVar.canConvert<QDBusObjectPath>())
		m_characteristicPath = charPathVar.value<QDBusObjectPath>();
	else
		qWarning() << "failed to get the characteristic path of the descriptor"
		           << "with uuid" << m_uuid;


	// currently it seems bluez doesn't support the 'Flags' property, so just
	// skip it for now
#if 0
	// the flags are a string array
	const QVariant flagsVar = properties[QStringLiteral("Flags")];
	if (!flagsVar.canConvert<QStringList>()) {
		qWarning() << "invalid flags of gatt descriptor" << flagsVar;
	} else {

		// possible flags
		static const QMap<QString, Flag> flagsMap = {
			{ QStringLiteral("read"),                        Read },
			{ QStringLiteral("write"),                       Write },
			{ QStringLiteral("encrypt-read"),                EncryptRead },
			{ QStringLiteral("encrypt-write"),               EncryptWrite },
			{ QStringLiteral("encrypt-authenticated-read"),  EncryptAuthenticatedRead },
			{ QStringLiteral("encrypt-authenticated-write"), EncryptAuthenticatedWrite },
		};

		// parse all the flags
		const QStringList flags = flagsVar.value<QStringList>();
		for (const QString &flag : flags) {

			QMap<QString, Flag>::const_iterator it = flagsMap.find(flag.toLower());
			if (it == flagsMap.end())
				qWarning() << "unknown flag for gatt characteristic" << flag;
			else
				m_flags |= it.value();
		}
	}
#endif

	// create a dbus proxy for the descriptor object
	m_proxy =
		QSharedPointer<BluezGattDescriptorInterface>::create(QStringLiteral("org.bluez"),
		                                                     path, conn);

	// and we're done
	m_valid = (m_proxy && m_proxy->isValid());
}

// -----------------------------------------------------------------------------
/*!

 */
BleGattDescriptorBluez::~BleGattDescriptorBluez()
{
}

// -----------------------------------------------------------------------------
/*!
	\fn bool BleGattDescriptor::timeout() const

	Returns the current value of the timeout in milliseconds. \c -1 means the
	default timeout (usually 25 seconds).

 */
int BleGattDescriptorBluez::timeout() const
{
	if (m_proxy)
		return m_proxy->timeout();
	else
		return -1;
}

// -----------------------------------------------------------------------------
/*!
	\fn bool BleGattDescriptor::setTimeout(int timeout) const

	Sets the timeout in milliseconds for all future DBus calls to timeout. \c -1
	means the default timeout (usually 25 seconds).

 */
void BleGattDescriptorBluez::setTimeout(int timeout)
{
	if (m_proxy) {
		if (timeout < 0)
			m_proxy->setTimeout(-1);
		else
			m_proxy->setTimeout(qBound(1000, timeout, 60000));
	}
}

// -----------------------------------------------------------------------------
/*!
	\fn bool BleGattDescriptor::isValid() const

	Returns \c true if the object was successifully created from the java / JNI
	class object.

 */
bool BleGattDescriptorBluez::isValid() const
{
	return m_valid;
}

// -----------------------------------------------------------------------------
/*!
	\fn BleUuid BleGattDescriptor::uuid() const

	Returns the UUID of the descriptor.  If the descriptor is not valid the
	returned value is undefined.

 */
BleUuid BleGattDescriptorBluez::uuid() const
{
	return m_uuid;
}

// -----------------------------------------------------------------------------
/*!
	\fn BleGattDescriptor::Flags BleGattDescriptor::flags() const

	Returns the properties of the descriptor. If the descriptor is not valid the
	returned value is undefined.

 */
BleGattDescriptor::Flags BleGattDescriptorBluez::flags() const
{
	return m_flags;
}

// -----------------------------------------------------------------------------
/*!
	\fn QSharedPointer<BleGattCharacteristic> BleGattDescriptor::characteristic() const

	Returns the parent characteristic for this descriptor.

 */
QSharedPointer<BleGattCharacteristic> BleGattDescriptorBluez::characteristic() const
{
#if QT_VERSION >= QT_VERSION_CHECK(5, 4, 0)
	return m_characteristic.lock();
#else
	return m_characteristic.toStrongRef();
#endif
}

// -----------------------------------------------------------------------------
/*!
	\fn void BleGattDescriptor::setCacheable(bool cacheable)

	Sets whether this descriptor is \a cacheable; cacheable descriptors store the
	last read / written value and on a new read won't actually send a request to
	the remote device, instead will return the last read / written value.

	By default the cacheable property is \c false.

 */
void BleGattDescriptorBluez::setCacheable(bool cacheable)
{
	if (m_cacheable != cacheable)
		m_lastValue.clear();

	m_cacheable = cacheable;
}

// -----------------------------------------------------------------------------
/*!
	\fn bool BleGattDescriptor::cacheable() const

	Returns the current cacheable property value.

	\see BleGattDescriptor::setCacheable()
 */
bool BleGattDescriptorBluez::cacheable() const
{
	return m_cacheable;
}

// -----------------------------------------------------------------------------
/*!
	\fn Future<QByteArray> BleGattDescriptor::readValue()

	Requests a read of the value stored in the descriptor.  The request is async
	with the result being returned in the future object.

 */
Future<QByteArray> BleGattDescriptorBluez::readValue()
{
	// sanity check
	if (Q_UNLIKELY(!m_proxy || !m_proxy->isValid()))
		return Future<QByteArray>::createErrored(QStringLiteral("com.sky.Error.Failed"),
		                                         QStringLiteral("no proxy connection"));

	// if the descriptor is cacheable and we have the last read / written value
	// store then just return that
	if (m_cacheable && !m_lastValue.isNull())
		return Future<QByteArray>::createFinished(m_lastValue);

	// otherwise start the transaction
	QDBusPendingReply<QByteArray> reply = m_proxy->ReadValue();
	Future<QByteArray> result = dbusPendingReplyToFuture<QByteArray>(reply);

	// if the result is cacheable then install a functor on the result to store
	// the value read
	if (m_cacheable) {

		std::function<void(const QByteArray&)> functor =
			[this](const QByteArray &value)
			{
				m_lastValue = value;
			};

		if (!result.isError()) {
			if (result.isFinished())
				m_lastValue = result.result();
			else
				result.connectFinished(this, functor);
		}
	}

	return result;
}

// -----------------------------------------------------------------------------
/*!
	\fn Future<void> BleGattDescriptor::writeValue()

	Requests a write of the value stored in the descriptor.  The request is async
	with the result being returned in the future object.

 */
Future<> BleGattDescriptorBluez::writeValue(const QByteArray &value)
{
	// sanity check
	if (Q_UNLIKELY(!m_proxy || !m_proxy->isValid()))
		return Future<>::createErrored(QStringLiteral("com.sky.Error.Failed"),
		                               QStringLiteral("no proxy connection"));

	QDBusPendingReply<> reply = m_proxy->WriteValue(value);
	Future<> result = dbusPendingReplyToFuture<>(reply);

	// if the result is cacheable then install a functor on the positive
	// result to save the written value
	if (m_cacheable) {

		std::function<void()> functor =
			[this, value]()
			{
				m_lastValue = value;
			};

		if (!result.isError()) {
			if (result.isFinished())
				m_lastValue = value;
			else
				result.connectFinished(this, functor);
		}
	}

	return result;
}

