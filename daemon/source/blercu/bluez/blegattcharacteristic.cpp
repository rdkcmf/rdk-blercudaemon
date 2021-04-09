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
//  blegattcharacteristic.cpp
//  SkyBluetoothRcu
//

#include "blegattcharacteristic_p.h"
#include "blegattdescriptor_p.h"
#include "blegattnotifypipe.h"
#include "blegatthelpers.h"
#include "blercu/blercuerror.h"

#include "interfaces/bluezgattcharacteristicinterface.h"

#include "utils/logging.h"

#include <QDBusPendingCallWatcher>
#include <QDBusError>

// -----------------------------------------------------------------------------
/*!
	\internal

	Constructs a new object parsing the details of the GATT characteristic from
	the dictionary received from bluez over dbus.

	The following is an example of the \a properties we get over dbus when a
	service is found

	\startcode

		dict entry(
			string "org.bluez.GattCharacteristic1"
			array [
				dict entry(
					string "UUID"
					variant                   string "00020001-bdf0-407c-aaff-d09967f31acd"
				)
				dict entry(
					string "Service"
					variant                   object path "/org/bluez/hci0/dev_1C_A2_B1_BE_EF_02/service003d"
				)
				dict entry(
					string "Value"
						variant                   array [
						]
				)
				dict entry(
					string "Flags"
					variant                   array [
						string "read"
						string "write"
					]
				)
			]
		)

	\endcode
 */
BleGattCharacteristicBluez::BleGattCharacteristicBluez(const QDBusConnection &conn,
                                                       const QString &path,
                                                       const QVariantMap &properties,
                                                       QObject *parent)
	: BleGattCharacteristic(parent)
	, m_path(path)
	, m_valid(false)
	, m_flags(0)
	, m_instanceId(0)
{
	// get the uuid of the service
	const QVariant uuidVar = properties[QStringLiteral("UUID")];
	if (!uuidVar.canConvert<QUuid>()) {
		qWarning() << "invalid uuid property of gatt characteristic" << uuidVar;
		return;
	}

	m_uuid = BleUuid(uuidVar.value<QUuid>());


	// get the parent service object path (used for constructing the tree)
	const QVariant servicePathVar = properties[QStringLiteral("Service")];
	if (servicePathVar.canConvert<QDBusObjectPath>())
		m_servicePath = servicePathVar.value<QDBusObjectPath>();
	else
		qWarning() << "failed to get the service path of the characteristic"
		           << "with uuid" << m_uuid << "(" << servicePathVar << ")";


	// the flags are a string array
	const QVariant flagsVar = properties[QStringLiteral("Flags")];
	if (!flagsVar.canConvert<QStringList>()) {
		qWarning() << "invalid flags of gatt characteristic" << flagsVar;
	} else {

		// possible flags
		static const QMap<QString, Flag> flagsMap = {
			{ QStringLiteral("broadcast"),                   Broadcast },
			{ QStringLiteral("read"),                        Read },
			{ QStringLiteral("write-without-response"),      WriteWithoutResponse },
			{ QStringLiteral("write"),                       Write },
			{ QStringLiteral("notify"),                      Notify },
			{ QStringLiteral("indicate"),                    Indicate },
			{ QStringLiteral("authenticated-signed-writes"), AuthenticatedSignedWrites },
			{ QStringLiteral("reliable-write"),              ReliableWrite },
			{ QStringLiteral("writable-auxiliaries"),        WritableAuxiliaries },
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

	// the instance id is used to distinguish between different instances of the
	// same service, for bluez we use the dbus path id, ie. a typical dbus
	// path would be '/org/bluez/hci0/dev_D4_B8_FF_11_76_EE/service0043/char004c' we
	// trim off the last bit and parse the 004c as the instance id
	QString serviceId = path.section('/', -1);
	if (sscanf(qPrintable(serviceId), "char%04x", &m_instanceId) != 1) {
		qWarning("failed to parse characteristic '%s' to get the instance id",
		         qPrintable(serviceId));
		m_instanceId = -1;
	}


	// create a dbus proxy for the descriptor object
	m_proxy =
		QSharedPointer<BluezGattCharacteristicInterface>::create(QStringLiteral("org.bluez"),
		                                                         path, conn);

	// and we're done
	m_valid = (m_proxy && m_proxy->isValid());
}

// -----------------------------------------------------------------------------
/*!

 */
BleGattCharacteristicBluez::~BleGattCharacteristicBluez()
{

}

// -----------------------------------------------------------------------------
/*!
	\fn bool BleGattCharacteristic::isValid() const

	Returns \c true if the object was successifully created from the java / JNI
	class object.

 */
bool BleGattCharacteristicBluez::isValid() const
{
	return m_valid;
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Sets the bluez version number which is used to determine which API to use
	for the dbus GATT characteristic calls.

 */
void BleGattCharacteristicBluez::setBluezVersion(const QVersionNumber &bluezVersion)
{
	if (m_proxy && (bluezVersion > QVersionNumber(5, 47))) {
		m_proxy->useNewDBusApi(true);
	}
}

// -----------------------------------------------------------------------------
/*!
	\fn bool BleGattCharacteristic::timeout() const

	Returns the current value of the timeout in milliseconds. \c -1 means the
	default timeout (usually 25 seconds).

 */
int BleGattCharacteristicBluez::timeout() const
{
	if (m_proxy)
		return m_proxy->timeout();
	else
		return -1;
}

// -----------------------------------------------------------------------------
/*!
	\fn bool BleGattCharacteristic::setTimeout(int timeout) const

	Sets the timeout in milliseconds for all future DBus calls to timeout. \c -1
	means the default timeout (usually 25 seconds).

 */
void BleGattCharacteristicBluez::setTimeout(int timeout)
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
	\fn BleUuid BleGattCharacteristic::uuid() const

	Returns the UUID of the characteristic.  If the characteristic is not valid
	the returned value is undefined.

 */
BleUuid BleGattCharacteristicBluez::uuid() const
{
	return m_uuid;
}

// -----------------------------------------------------------------------------
/*!
	\fn BleUuid BleGattCharacteristic::instanceId() const

	Returns the instance ID for this characteristic. If a remote device offers
	multiple characteristics with the same UUID, the instance ID is used to
	distinguish between characteristics.

 */
int BleGattCharacteristicBluez::instanceId() const
{
	return m_instanceId;
}

// -----------------------------------------------------------------------------
/*!
	\fn BleGattCharacteristic::Flags BleGattCharacteristic::flags() const

	Returns the properties and permission flags for the characteristic. If the
	characteristic is not valid the returned value is undefined.

 */
BleGattCharacteristic::Flags BleGattCharacteristicBluez::flags() const
{
	return m_flags;
}

// -----------------------------------------------------------------------------
/*!
	\fn QSharedPointer<BleGattService> BleGattCharacteristic::service() const

	Returns a shared pointer to the parent service for the characteristic.

 */
QSharedPointer<BleGattService> BleGattCharacteristicBluez::service() const
{
#if QT_VERSION <= QT_VERSION_CHECK(5, 3, 0)
	return m_service.toStrongRef();
#else
	return m_service.lock();
#endif
}

// -----------------------------------------------------------------------------
/*!
	\fn QList< QSharedPointer<BleGattDescriptor> > BleGattCharacteristic::descriptors() const

	Returns a list of all the descriptors attached to this characteristic.

 */
QList< QSharedPointer<BleGattDescriptor> > BleGattCharacteristicBluez::descriptors() const
{
	QList< QSharedPointer<BleGattDescriptor> > descriptors;
	descriptors.reserve(m_descriptors.size());

	for (const QSharedPointer<BleGattDescriptorBluez> &descriptor : m_descriptors)
		descriptors.append(descriptor);

	return descriptors;
}

// -----------------------------------------------------------------------------
/*!
	\fn QSharedPointer<BleGattDescriptor> BleGattCharacteristic::descriptor(BleUuid descUuid) const

	Returns the shared pointer to the descriptor with the given UUID. If no
	descriptor exists then a null shared pointer is returned.


 */
QSharedPointer<BleGattDescriptor> BleGattCharacteristicBluez::descriptor(BleUuid descUuid) const
{
	return qSharedPointerCast<BleGattDescriptor>(m_descriptors.value(descUuid));
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Adds the descriptor to the characteristic, basically just inserts it into the
	internal multimap.

 */
void BleGattCharacteristicBluez::addDescriptor(const QSharedPointer<BleGattDescriptorBluez> &descriptor)
{
	m_descriptors.insert(descriptor->uuid(), descriptor);
}

// -----------------------------------------------------------------------------
/*!
	\fn void BleGattCharacteristic::setCacheable(bool cacheable)

	Sets whether this characteristic is \a cacheable; cacheable characteristics
	store the last read / written value and on a new read won't actually send a
	request to the remote device, instead will return the last read / written
	value.

	By default the cacheable property is \c false.

 */
void BleGattCharacteristicBluez::setCacheable(bool cacheable)
{
	Q_UNUSED(cacheable);

	qWarning("cacheable not yet implemented for characteristic");
}

// -----------------------------------------------------------------------------
/*!
	\fn bool BleGattDescriptor::cacheable() const

	Returns the current cacheable property value.

	\see BleGattDescriptor::setCacheable()
 */
bool BleGattCharacteristicBluez::cacheable() const
{
	qWarning("cacheable not yet implemented for characteristic");
	return false;
}

// -----------------------------------------------------------------------------
/*!
	\fn Future<QByteArray> BleGattCharacteristic::readValue()

	Requests a read on the characteristic. This is an async operation, the
	result is returned in the Future object.

 */
Future<QByteArray> BleGattCharacteristicBluez::readValue()
{
	// sanity check
	if (Q_UNLIKELY(!m_proxy || !m_proxy->isValid()))
		return Future<QByteArray>::createErrored(QStringLiteral("com.sky.Error.Failed"),
		                                         QStringLiteral("no proxy connection"));

	// send the read request and put the reply on in future
	QDBusPendingReply<QByteArray> reply = m_proxy->ReadValue();
	return dbusPendingReplyToFuture<QByteArray>(reply);
}

// -----------------------------------------------------------------------------
/*!
	\fn Future<void> BleGattCharacteristic::writeValue(const QByteArray &value)

	Requests a write on the characteristic. This is an async operation, the
	result is returned in the Future object.

 */
Future<> BleGattCharacteristicBluez::writeValue(const QByteArray &value)
{
	// sanity check
	if (Q_UNLIKELY(!m_proxy || !m_proxy->isValid()))
		return Future<>::createErrored(QStringLiteral("com.sky.Error.Failed"),
		                               QStringLiteral("no proxy connection"));

	// send the read request and put the reply on in future
	QDBusPendingReply<> reply = m_proxy->WriteValue(value);
	return dbusPendingReplyToFuture<>(reply);
}

// -----------------------------------------------------------------------------
/*!
	\fn Future<void> BleGattCharacteristic::writeValueWithoutResponse(const QByteArray &value)

	Requests a writeWithoutResponse (aka WRITE_CMD) on the characteristic. This
	is an async operation, the result is returned in the Future object.

 */
Future<> BleGattCharacteristicBluez::writeValueWithoutResponse(const QByteArray &value)
{
	// sanity check
	if (Q_UNLIKELY(!m_proxy || !m_proxy->isValid()))
		return Future<>::createErrored(QStringLiteral("com.sky.Error.Failed"),
		                               QStringLiteral("no proxy connection"));

	// set the write without response
	QVariantMap flags;
	flags.insert(QStringLiteral("type"), QStringLiteral("write-without-response"));

	// send the read request and put the reply on in future
	QDBusPendingReply<> reply = m_proxy->WriteValue(value, flags);
	return dbusPendingReplyToFuture<>(reply);
}

// -----------------------------------------------------------------------------
/*!
	\fn Future<void> BleGattCharacteristic::enableNotifications(bool enable)

	Request notifications be enabled or disabled on the characteristic.

	Bluez sets the CCCD value in the descriptor to tell the remote device to
	send notifications automatically.

 */
Future<> BleGattCharacteristicBluez::enableNotifications(bool enable)
{
	// sanity check notifications are supported
	if (!m_flags.testFlag(Notify)) {
		qError() << "notifications not supported for" << m_uuid;
		return Future<>::createErrored(QStringLiteral("not supported"));
	}

	// check if notifications are already enabled / disabled
	if ((enable && m_notifyPipe) || (!enable && !m_notifyPipe))
		return Future<>::createFinished();

	// check if disabling, in which case we just need to close pipe
	if (!enable) {
		m_notifyPipe.reset();
		return Future<>::createFinished();
	}


	// so must be enabling the notifications, in which case we need to request
	// a notification pipe from bluez
	QDBusPendingReply<QDBusUnixFileDescriptor, quint16> reply = m_proxy->AcquireNotify();
	QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(reply, this);

	// create a promise to signal the result of the notification enable
	QSharedPointer<Promise<>> promise = QSharedPointer<Promise<>>::create();

	// install a callback on the completion of the request
	QObject::connect(watcher, &QDBusPendingCallWatcher::finished,
	                 this, std::bind(&BleGattCharacteristicBluez::onNotificationEnableReply,
	                                 this, std::placeholders::_1, promise));

	// return the future (triggered when the promise completes)
	return promise->future();
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Called when we receive a dbus reply to our request to enable notifications on
	the characteristic.

 */
void BleGattCharacteristicBluez::onNotificationEnableReply(QDBusPendingCallWatcher *call,
                                                           QSharedPointer<Promise<>> promise)
{

	// clean up the pending call on the next time through the event loop
	call->deleteLater();

	// check for error and log it
	QDBusPendingReply<QDBusUnixFileDescriptor, quint16> reply = *call;
	if (reply.isError()) {
		const QDBusError error = reply.error();
		qError() << "failed to acquire notify due to" << error;
		promise->setError(error.name(), error.message());
		return;
	}

	// get / check the args
	QDBusUnixFileDescriptor pipeFd = reply.argumentAt<0>();
	if (!pipeFd.isValid()) {
		qError("invalid notify pipe fd from bluez");
		promise->setError(BleRcuError::errorString(BleRcuError::General),
		                  QStringLiteral("Invalid notify pipe fd from bluez"));
		return;
	}

	quint16 mtu = reply.argumentAt<1>();
	if (mtu < 23) {
		qError("invalid MTU size on the notify pipe (%hd bytes)", mtu);
		promise->setError(BleRcuError::errorString(BleRcuError::General),
		                  QStringLiteral("Invalid MTU size from bluez"));
		return;
	}

	// wrap the file descriptor in a new GATT notification pipe object
	m_notifyPipe = QSharedPointer<BleGattNotifyPipe>(new BleGattNotifyPipe(pipeFd, mtu),
	                                                 &QObject::deleteLater);
	if (!m_notifyPipe || !m_notifyPipe->isValid()) {
		m_notifyPipe.reset();
		promise->setError(BleRcuError::errorString(BleRcuError::General),
		                  QStringLiteral("Invalid pipe fd from bluez"));
		return;
	}

	// connect to the events signalled when the pipe closes or a
	// notification is received
	QObject::connect(m_notifyPipe.data(), &BleGattNotifyPipe::notification,
	                 this, &BleGattCharacteristicBluez::valueChanged);
	QObject::connect(m_notifyPipe.data(), &BleGattNotifyPipe::closed,
	                 this, &BleGattCharacteristicBluez::onNotifyPipeClosed);

	// success
	promise->setFinished();
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Slot called when the notification pipe is closed, bluez does this if the remote
	device disconnects.

 */
void BleGattCharacteristicBluez::onNotifyPipeClosed()
{
	// it is safe to delete the notify pipe here as the shared pointer uses
	// deleteLater meaning the actual memory will be freed the next time through
	// the event loop
	m_notifyPipe.reset();
}
