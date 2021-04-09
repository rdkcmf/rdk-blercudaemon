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
//  blercuvoice1_adaptor.cpp
//  SkyBluetoothRcu
//

#include "blercuvoice1_adaptor.h"
#include "blercu/blercucontroller.h"
#include "blercu/blercudevice.h"
#include "blercu/blercuerror.h"
#include "blercu/bleservices/blercuaudioservice.h"

#include "utils/logging.h"



BleRcuVoice1Adaptor::BleRcuVoice1Adaptor(const QSharedPointer<BleRcuController> &controller,
                                         QObject *parent)
	: DBusAbstractAdaptor(parent)
	, m_controller(controller)
{
	// don't auto relay signals, we don't have any signals
	setAutoRelaySignals(false);

}

BleRcuVoice1Adaptor::~BleRcuVoice1Adaptor()
{
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Converter function to convert a \l{FileDescriptor} type to a \l{QVariant} of
	type \l{QDBusUnixFileDescriptor}
 */
QList<QVariant> BleRcuVoice1Adaptor::convertFileDescriptor(const FileDescriptor &desc)
{
	// if the fd is not valid then don't return anything
	if (!desc.isValid())
		return QList<QVariant>();

	// store the fd within a QDBusUnixFileDescriptor and send that over the bus
	QDBusUnixFileDescriptor dbusFd(desc.fd());
	return QList<QVariant>( { QVariant::fromValue(dbusFd) } );
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Converter function to convert a \l{BleRcuAudioService::PacketStats} type to
	a couple of \c quint32 values.
 */
QList<QVariant> BleRcuVoice1Adaptor::convertStatusInfo(const BleRcuAudioService::StatusInfo &info)
{
	QList<QVariant> packetStats = {
		QVariant::fromValue<quint32>(info.lastError),
		QVariant::fromValue<quint32>(info.actualPackets),
		QVariant::fromValue<quint32>(info.expectedPackets)
	};

	return packetStats;
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Attempts to get the RCU device from the given address.  Returns a null
	shared pointer if the device is not paired.

 */
QSharedPointer<BleRcuDevice> BleRcuVoice1Adaptor::getDevice(const QString &bdaddr) const
{
	// first sanity check the bdaddr is valid
	BleAddress address(bdaddr);
	if (address.isNull()) {
		qWarning("invalid bdaddr supplied");
		return QSharedPointer<BleRcuDevice>();
	}

	// return the device
	return m_controller->managedDevice(address);
}

// -----------------------------------------------------------------------------
/*!
	DBus method call for com.sky.blercu.Voice1.StartAudioStreaming

 */
void BleRcuVoice1Adaptor::StartAudioStreaming(const QString &bdaddr, uint encoding,
                                              const QDBusMessage &message)
{
	// try and get the device with the given address, will fail if device not paired
	QSharedPointer<BleRcuDevice> device = getDevice(bdaddr);
	if (!device) {
		sendErrorReply(message, BleRcuError::errorString(BleRcuError::General),
		               QStringLiteral("Unknown device"));
		return;
	}

	// sanity check and convert the encoding value
	BleRcuAudioService::Encoding audioEncoding;
	switch (encoding) {
		case 1:
			audioEncoding = BleRcuAudioService::ADPCM;
			break;
		case 2:
			audioEncoding = BleRcuAudioService::PCM16;
			break;
		default:
			sendErrorReply(message, BleRcuError::errorString(BleRcuError::InvalidArg),
			              QStringLiteral("Invalid encoding value"));
			return;
	}

	// get the service and request to start streaming
	const QSharedPointer<BleRcuAudioService> service = device->audioService();
	Future<FileDescriptor> result = service->startStreaming(audioEncoding);

	// need a custom converter to convert from a FileDescriptor object to
	// real fd we can send over dbus
	const std::function<QList<QVariant>(const FileDescriptor&)>
		converter = &BleRcuVoice1Adaptor::convertFileDescriptor;

	// connect the result future to a dbus reply
	connectFutureToDBusReply(message, result, converter);
}

// -----------------------------------------------------------------------------
/*!
	DBus method call for com.sky.blercu.Voice1.GetAudioStatus


 */
void BleRcuVoice1Adaptor::GetAudioStatus(const QString &bdaddr,
                                         const QDBusMessage &message)
{
	// try and get the device with the given address, will fail if device not paired
	QSharedPointer<BleRcuDevice> device = getDevice(bdaddr);
	if (!device) {
		sendErrorReply(message, BleRcuError::errorString(BleRcuError::General),
		               QStringLiteral("Unknown device"));
		return;
	}

	// get the service and request to start streaming
	const QSharedPointer<BleRcuAudioService> service = device->audioService();
	Future<BleRcuAudioService::StatusInfo> result = service->status();

	// need a custom converter to convert from a PacketStats structure to two
	// uint32 values
	const std::function<QList<QVariant> (const BleRcuAudioService::StatusInfo&)>
		converter = &BleRcuVoice1Adaptor::convertStatusInfo;

	// connect the result future to a dbus reply
	connectFutureToDBusReply(message, result, converter);
}


