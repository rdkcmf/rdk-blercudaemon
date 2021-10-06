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
//  blercudevice1_adaptor.cpp
//  BleRcuDaemon
//

#include "blercudevice1_adaptor.h"

#include "blercu/blercudevice.h"
#include "blercu/bleservices/blercuaudioservice.h"
#include "blercu/bleservices/blercubatteryservice.h"
#include "blercu/bleservices/blercutouchservice.h"
#include "blercu/bleservices/blercudeviceinfoservice.h"
#include "blercu/bleservices/blercuinfraredservice.h"
#include "blercu/bleservices/blercufindmeservice.h"
#include "blercu/bleservices/blercuremotecontrolservice.h"

#include "blercu/blercuerror.h"
#include "utils/logging.h"

#include <QDBusArgument>

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#define USER_INPUT_KEY_STANDBY               (0xE000U)
#define USER_INPUT_KEY_VOLUME_UP             (0xE003U)
#define USER_INPUT_KEY_VOLUME_DOWN           (0xE004U)
#define USER_INPUT_KEY_MUTE                  (0xE005U)
#define USER_INPUT_KEY_TV                    (0xE010U)



QDBusArgument &operator<<(QDBusArgument &argument, const CdiKeyCodeList& cdiKeyCodes)
{
	argument.beginArray(qMetaTypeId<quint16>());

	for (quint16 cdiKeyCode : cdiKeyCodes)
		argument << cdiKeyCode;

	argument.endArray();
	return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, CdiKeyCodeList &cdiKeyCodes)
{
	argument.beginArray();

	while (!argument.atEnd()) {
		quint16 cdiKeyCode;
		argument >> cdiKeyCode;
		cdiKeyCodes.append(cdiKeyCode);
	}

	argument.endArray();
	return argument;
}




BleRcuDevice1Adaptor::BleRcuDevice1Adaptor(const QSharedPointer<BleRcuDevice> &device,
                                           const QDBusObjectPath &objPath,
                                           QObject *parent)
	: DBusAbstractAdaptor(parent)
	, m_device(device)
	, m_dbusObjPath(objPath)
{
	// register the dbus type, only need to do this once
	static QAtomicInteger<bool> isRegistered(false);
	if (isRegistered.testAndSetAcquire(false, true)) {
		qDBusRegisterMetaType<CdiKeyCodeList>();
		qDBusRegisterMetaType<IrKeyWaveforms>();
	}
	
	// don't auto relay signals, we do this manually
	setAutoRelaySignals(false);

	// connect to the device ready and name changed signals
	QObject::connect(m_device.data(), &BleRcuDevice::readyChanged,
	                 this, &BleRcuDevice1Adaptor::onReadyChanged);
	QObject::connect(m_device.data(), &BleRcuDevice::nameChanged,
	                 this, &BleRcuDevice1Adaptor::onNameChanged);


	// get the battery service and connect to the level change signal
	const QSharedPointer<const BleRcuBatteryService> battService = m_device->batteryService();
	QObject::connect(battService.data(), &BleRcuBatteryService::levelChanged,
	                 this, &BleRcuDevice1Adaptor::onBatteryLevelChanged);


	// get the audio service and connect to the streaming change signal
	const QSharedPointer<const BleRcuAudioService> audioService = m_device->audioService();
	QObject::connect(audioService.data(), &BleRcuAudioService::streamingChanged,
	                 this, &BleRcuDevice1Adaptor::onAudioStreamingChanged);
	QObject::connect(audioService.data(), &BleRcuAudioService::gainLevelChanged,
	                 this, &BleRcuDevice1Adaptor::onAudioGainLevelChanged);
	QObject::connect(audioService.data(), &BleRcuAudioService::audioCodecsChanged,
	                 this, &BleRcuDevice1Adaptor::onAudioCodecsChanged);


	// get the touch service and connect to the mode change signal
	const QSharedPointer<const BleRcuTouchService> touchService = m_device->touchService();
	QObject::connect(touchService.data(), &BleRcuTouchService::modeChanged,
	                 this, &BleRcuDevice1Adaptor::onTouchModeChanged);
	QObject::connect(touchService.data(), &BleRcuTouchService::modeSettabilityChanged,
	                 this, &BleRcuDevice1Adaptor::onTouchModeSettabilityChanged);


	// get the infrared service and connect to the ir code change signal
	const QSharedPointer<const BleRcuInfraredService> infraredService = m_device->infraredService();
	QObject::connect(infraredService.data(), &BleRcuInfraredService::codeIdChanged,
	                 this, &BleRcuDevice1Adaptor::onCodeIdChanged);


	// get the device info service and connect to any / all change signals
	const QSharedPointer<const BleRcuDeviceInfoService> deviceInfoService = m_device->deviceInfoService();
	QObject::connect(deviceInfoService.data(), &BleRcuDeviceInfoService::manufacturerNameChanged,
	                 this, &BleRcuDevice1Adaptor::onManufacturerNameChanged);
	QObject::connect(deviceInfoService.data(), &BleRcuDeviceInfoService::modelNumberChanged,
	                 this, &BleRcuDevice1Adaptor::onModelNumberChanged);
	QObject::connect(deviceInfoService.data(), &BleRcuDeviceInfoService::serialNumberChanged,
	                 this, &BleRcuDevice1Adaptor::onSerialNumberChanged);
	QObject::connect(deviceInfoService.data(), &BleRcuDeviceInfoService::hardwareRevisionChanged,
	                 this, &BleRcuDevice1Adaptor::onHardwareRevisionChanged);
	QObject::connect(deviceInfoService.data(), &BleRcuDeviceInfoService::firmwareVersionChanged,
	                 this, &BleRcuDevice1Adaptor::onFirmwareVersionChanged);
	QObject::connect(deviceInfoService.data(), &BleRcuDeviceInfoService::softwareVersionChanged,
	                 this, &BleRcuDevice1Adaptor::onSoftwareVersionChanged);


	// get the remote control service and connect to any / all change signals
	const QSharedPointer<const BleRcuRemoteControlService> remoteControlService = m_device->remoteControlService();
	QObject::connect(remoteControlService.data(), &BleRcuRemoteControlService::unpairReasonChanged,
	                 this, &BleRcuDevice1Adaptor::onUnpairReasonChanged);
	QObject::connect(remoteControlService.data(), &BleRcuRemoteControlService::rebootReasonChanged,
	                 this, &BleRcuDevice1Adaptor::onRebootReasonChanged);
	QObject::connect(remoteControlService.data(), &BleRcuRemoteControlService::lastKeypressChanged,
	                 this, &BleRcuDevice1Adaptor::onLastKeypressChanged);
}

BleRcuDevice1Adaptor::~BleRcuDevice1Adaptor()
{
	// destructor
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Utility function to emit a property change notification over the dbus
	interface.

 */
template <typename T>
void BleRcuDevice1Adaptor::emitPropertyChanged(const QString &propName,
                                               const T &propValue) const
{
	sendPropertyChangeNotification<T>(m_dbusObjPath.path(), propName, propValue);
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Utility function to send an error in reply to a dbus request

 */
void BleRcuDevice1Adaptor::sendError(const QDBusMessage &request,
                                     BleRcuError::ErrorType errorType,
                                     const QString &errorMessage) const
{
	sendErrorReply(request, BleRcuError::errorString(errorType), errorMessage);
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Utility function to send an error in reply to a dbus request

 */
void BleRcuDevice1Adaptor::sendError(const QDBusMessage &request,
                                     QDBusError::ErrorType errorType,
                                     const QString &errorMessage) const
{
	sendErrorReply(request, QDBusError::errorString(errorType), errorMessage);
}

// -----------------------------------------------------------------------------
/*!
	DBus get property call for com.sky.BleRcuDevice1.Address

 */
QString BleRcuDevice1Adaptor::address() const
{
	BleAddress bdaddr = m_device->address();
	return bdaddr.toString();
}

// -----------------------------------------------------------------------------
/*!
	DBus get property call for com.sky.BleRcuDevice1.Controller

 */
QDBusObjectPath BleRcuDevice1Adaptor::controller() const
{
	// FIXME: for now this path is hardcoded, it must match whatever we set
	// when adding the controller dbus object
	return QDBusObjectPath(QStringLiteral("/com/sky/blercu/controller"));
}

// -----------------------------------------------------------------------------
/*!
	DBus get property call for com.sky.BleRcuDevice1.Connected

	\note The connected property we expose to the outside world is actually our
	internal 'ready' state, which means the device is connected, paired and all
	the services have been (successifully) started.
 */
bool BleRcuDevice1Adaptor::connected() const
{
	return m_device->isReady();
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Slot called by the \l{BleRcuDevice} parent object when when the ready state
	of the device changes. We hook this signal to emit a property change signal
	over dbus.

 */
void BleRcuDevice1Adaptor::onReadyChanged(bool ready)
{
	emitPropertyChanged(QStringLiteral("Connected"), ready);
}

// -----------------------------------------------------------------------------
/*!
	DBus get property call for com.sky.BleRcuDevice1.Name

 */
QString BleRcuDevice1Adaptor::name() const
{
	return m_device->name();
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Slot called by the \l{BleRcuDevice} parent object when when the name of the
	device changes. We hook this signal to emit a property change signal over
	dbus.

 */
void BleRcuDevice1Adaptor::onNameChanged(const QString &name)
{
	emitPropertyChanged(QStringLiteral("Name"), name);
}

// -----------------------------------------------------------------------------
/*!
	DBus get property call for com.sky.BleRcuDevice1.AudioGainLevel

 */
quint8 BleRcuDevice1Adaptor::audioGainLevel() const
{
	const QSharedPointer<const BleRcuAudioService> service = m_device->audioService();
	return service->gainLevel();
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Slot called by the \l{BleRcuAudioService} object when when the audio gain
	level changes. We hook this signal to emit a property change signal over
	dbus.

 */
void BleRcuDevice1Adaptor::onAudioGainLevelChanged(quint8 gainLevel)
{
	emitPropertyChanged(QStringLiteral("AudioGainLevel"), gainLevel);
}

// -----------------------------------------------------------------------------
/*!
	DBus get property call for com.sky.BleRcuDevice1.AudioCodecs

 */
quint32 BleRcuDevice1Adaptor::audioCodecs() const
{
	const QSharedPointer<const BleRcuAudioService> service = m_device->audioService();
	return service->audioCodecs();
}
// -----------------------------------------------------------------------------
/*!
	\internal

	Slot called by the \l{BleRcuAudioService} object when when the audio codecs
	changes. We hook this signal to emit a property change signal over
	dbus.

 */
void BleRcuDevice1Adaptor::onAudioCodecsChanged(quint32 codecs)
{
	emitPropertyChanged(QStringLiteral("AudioCodecs"), codecs);
}

// -----------------------------------------------------------------------------
/*!
	DBus get property call to set the com.sky.BleRcuDevice1.AudioGainLevel property

 */
void BleRcuDevice1Adaptor::setAudioGainLevel(quint8 value)
{
	const QSharedPointer<BleRcuAudioService> service = m_device->audioService();
	service->setGainLevel(value);
}

// -----------------------------------------------------------------------------
/*!
	DBus get property call for com.sky.BleRcuDevice1.AudioStreaming

 */
bool BleRcuDevice1Adaptor::audioStreaming() const
{
	const QSharedPointer<const BleRcuAudioService> service = m_device->audioService();
	return service->isStreaming();
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Slot called by the \l{BleRcuAudioService} object when when the audio
	streaming is started / stopped. We hook this signal to emit a property
	change signal over dbus.

 */
void BleRcuDevice1Adaptor::onAudioStreamingChanged(bool streaming)
{
	emitPropertyChanged(QStringLiteral("AudioStreaming"), streaming);
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Convertor function to convert a \l{FileDescriptor} type to a \l{QVariant} of
	type \l{QDBusUnixFileDescriptor}
 */
QList<QVariant> BleRcuDevice1Adaptor::convertFileDescriptor(const FileDescriptor &desc)
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
	DBus method call handler for com.sky.BleRcuDevice1.StartAudioStreamingTo

 */
void BleRcuDevice1Adaptor::StartAudioStreamingTo(quint32 encoding,
                                                 const QString &filePath,
                                                 const QDBusMessage &request)
{
	// only enable this API on debug builds
#if (AI_BUILD_TYPE == AI_DEBUG)

	// sanity check and convert the encoding value
	BleRcuAudioService::Encoding audioEncoding = BleRcuAudioService::InvalidEncoding;
	switch (encoding) {
		case 1:
			audioEncoding = BleRcuAudioService::ADPCM;
			break;
		case 2:
			audioEncoding = BleRcuAudioService::PCM16;
			break;
		default:
			sendError(request, BleRcuError::InvalidArg,
			          QStringLiteral("Invalid encoding value"));
			return;
	}

	// open / create the file to write to
	int fd = open(qPrintable(filePath), O_CLOEXEC | O_CREAT | O_WRONLY, 0644);
	if (fd < 0) {
		qErrnoWarning(errno, "failed to open / create file @ '%s'", qPrintable(filePath));
		sendError(request, BleRcuError::IoDevice,
		          QStringLiteral("Failed to open / create file for writing"));
		return;
	}

	// get the service and request to start streaming
	const QSharedPointer<BleRcuAudioService> service = m_device->audioService();
	Future<> result = service->startStreamingTo(audioEncoding, fd);

	// can now close the fd (the audio service will have dup'd it if it needs
	// it around
	if (close(fd) != 0)
		qErrnoWarning(errno, "failed to close file @ '%s'", qPrintable(filePath));

	// connect the result future to a dbus reply
	connectFutureToDBusReply(request, result);

#else  // (AI_BUILD_TYPE != AI_DEBUG)

	sendError(request, BleRcuError::Rejected, QStringLiteral("Not supported"));

#endif // (AI_BUILD_TYPE != AI_DEBUG)

}

// -----------------------------------------------------------------------------
/*!
	DBus method call handler for com.sky.BleRcuDevice1.StartAudioStreaming

 */
void BleRcuDevice1Adaptor::StartAudioStreaming(quint32 encoding,
                                               const QDBusMessage &request)
{
	// sanity check and convert the encoding value
	BleRcuAudioService::Encoding audioEncoding = BleRcuAudioService::InvalidEncoding;
	switch (encoding) {
		case 1:
			audioEncoding = BleRcuAudioService::ADPCM;
			break;
		case 2:
			audioEncoding = BleRcuAudioService::PCM16;
			break;
		default:
			sendError(request, BleRcuError::InvalidArg,
			          QStringLiteral("Invalid encoding value"));
			return;
	}

	// get the service and request to start streaming
	const QSharedPointer<BleRcuAudioService> service = m_device->audioService();
	Future<FileDescriptor> result = service->startStreaming(audioEncoding);

	// need a custom converter to convert from a FileDescriptor object to
	// real fd we can send over dbus
	const std::function<QList<QVariant>(const FileDescriptor&)>
		convertor = &BleRcuDevice1Adaptor::convertFileDescriptor;

	// connect the result future to a dbus reply
	connectFutureToDBusReply(request, result, convertor);
}

// -----------------------------------------------------------------------------
/*!
	DBus method call handler for com.sky.BleRcuDevice1.StopAudioStreaming

 */
void BleRcuDevice1Adaptor::StopAudioStreaming(const QDBusMessage &request)
{
	// get the service and request to stop streaming
	const QSharedPointer<BleRcuAudioService> service = m_device->audioService();
	Future<> result = service->stopStreaming();

	// connect the result future to a dbus reply
	connectFutureToDBusReply(request, result);
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Convertor function to convert a \l{BleRcuAudioService::PacketStats} type to
	a couple of \c quint32 values.
 */
QList<QVariant> BleRcuDevice1Adaptor::convertStatusInfo(const BleRcuAudioService::StatusInfo &info)
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
	DBus method call handler for com.sky.BleRcuDevice1.GetAudioPacketStats

 */
void BleRcuDevice1Adaptor::GetAudioStatus(const QDBusMessage &request)
{
	// get the service and request to start streaming
	const QSharedPointer<BleRcuAudioService> service = m_device->audioService();
	Future<BleRcuAudioService::StatusInfo> result = service->status();

	// need a custom converter to convert from a PacketStats structure to two
	// uint32 values
	const std::function<QList<QVariant> (const BleRcuAudioService::StatusInfo&)>
		convertor = &BleRcuDevice1Adaptor::convertStatusInfo;

	// connect the result future to a dbus reply
	connectFutureToDBusReply(request, result, convertor);
}

// -----------------------------------------------------------------------------
/*!
	DBus get property call for com.sky.BleRcuDevice1.BatteryLevel

 */
quint8 BleRcuDevice1Adaptor::batteryLevel() const
{
	const QSharedPointer<const BleRcuBatteryService> service = m_device->batteryService();
	const int level = service->level();

	if (level < 0)
		return quint8(255);
	else
		return qMin<quint8>(level, 100);
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Slot called by the \l{BleRcuBatteryService} object when when the battery
	level changes. We hook this signal to emit a property change signal over dbus.

 */
void BleRcuDevice1Adaptor::onBatteryLevelChanged(int batteryLevel)
{
	quint8 level;
	if (batteryLevel < 0)
		level = quint8(255);
	else
		level = qMin<quint8>(batteryLevel, 100);

	emitPropertyChanged<quint8>(QStringLiteral("BatteryLevel"), level);
}

// -----------------------------------------------------------------------------
/*!
	DBus get property call for com.sky.BleRcuDevice1.FirmwareRevision

 */
QString BleRcuDevice1Adaptor::firmwareRevision() const
{
	const QSharedPointer<const BleRcuDeviceInfoService> service = m_device->deviceInfoService();
	return service->firmwareVersion();
}

// -----------------------------------------------------------------------------
/*!
	DBus get property call for com.sky.BleRcuDevice1.SoftwareRevision

 */
QString BleRcuDevice1Adaptor::softwareRevision() const
{
	const QSharedPointer<const BleRcuDeviceInfoService> service = m_device->deviceInfoService();
	return service->softwareVersion();
}

// -----------------------------------------------------------------------------
/*!
	DBus get property call for com.sky.BleRcuDevice1.HardwareRevision

 */
QString BleRcuDevice1Adaptor::hardwareRevision() const
{
	const QSharedPointer<const BleRcuDeviceInfoService> service = m_device->deviceInfoService();
	return service->hardwareRevision();
}

// -----------------------------------------------------------------------------
/*!
	DBus get property call for com.sky.BleRcuDevice1.Manufacturer

 */
QString BleRcuDevice1Adaptor::manufacturer() const
{
	const QSharedPointer<const BleRcuDeviceInfoService> service = m_device->deviceInfoService();
	return service->manufacturerName();
}

// -----------------------------------------------------------------------------
/*!
	DBus get property call for com.sky.BleRcuDevice1.Manufacturer

 */
QString BleRcuDevice1Adaptor::model() const
{
	const QSharedPointer<const BleRcuDeviceInfoService> service = m_device->deviceInfoService();
	return service->modelNumber();
}

// -----------------------------------------------------------------------------
/*!
	DBus get property call for com.sky.BleRcuDevice1.Manufacturer

 */
QString BleRcuDevice1Adaptor::serialNumber() const
{
	const QSharedPointer<const BleRcuDeviceInfoService> service = m_device->deviceInfoService();
	return service->serialNumber();
}

// -----------------------------------------------------------------------------
/*!
	\internal
 
	Called when the firmware version string has changed, we just emit the
	property change signal.

 */
void BleRcuDevice1Adaptor::onFirmwareVersionChanged(const QString &fwVersion)
{
	emitPropertyChanged<QString>(QStringLiteral("FirmwareRevision"), fwVersion);
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Called when the software version string has changed, we just emit the
	property change signal.

 */
void BleRcuDevice1Adaptor::onSoftwareVersionChanged(const QString &swVersion)
{
	emitPropertyChanged<QString>(QStringLiteral("SoftwareRevision"), swVersion);
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Called when the hardware version string has changed, we just emit the
	property change signal.

 */
void BleRcuDevice1Adaptor::onHardwareRevisionChanged(const QString &hwVersion)
{
	emitPropertyChanged<QString>(QStringLiteral("HardwareRevision"), hwVersion);
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Called when the manufacturer name string has changed, we just emit the
	property change signal.

 */
void BleRcuDevice1Adaptor::onManufacturerNameChanged(const QString &name)
{
	emitPropertyChanged<QString>(QStringLiteral("Manufacturer"), name);
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Called when the model number string has changed, we just emit the property
	change signal.

 */
void BleRcuDevice1Adaptor::onModelNumberChanged(const QString &model)
{
	emitPropertyChanged<QString>(QStringLiteral("Model"), model);
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Called when the serial number string has changed, we just emit the property
	change signal.

 */
void BleRcuDevice1Adaptor::onSerialNumberChanged(const QString &serial)
{
	emitPropertyChanged<QString>(QStringLiteral("SerialNumber"), serial);
}

// -----------------------------------------------------------------------------
/*!
	DBus get property call for com.sky.BleRcuDevice1.TouchMode

 */
quint32 BleRcuDevice1Adaptor::touchMode() const
{
	const QSharedPointer<const BleRcuTouchService> service = m_device->touchService();
	BleRcuTouchService::TouchModeOptions modeOptions = service->mode();

	// convert the options back to bit-flags
	quint32 flags = 0;
	if (modeOptions & BleRcuTouchService::TrackPadEnabled)
		flags |= 0x01;
	if (modeOptions & BleRcuTouchService::SliderEnabled)
		flags |= 0x02;

	// return the flags
	return flags;
}

// -----------------------------------------------------------------------------
/*!
	DBus get property call for com.sky.BleRcuDevice1.TouchModeSettable

 */
bool BleRcuDevice1Adaptor::touchModeSettable() const
{
	const QSharedPointer<const BleRcuTouchService> service = m_device->touchService();
	return service->modeSettable();
}

// -----------------------------------------------------------------------------
/*!
	DBus method call handler for com.sky.BleRcuDevice1.SetTouchMode

 */
void BleRcuDevice1Adaptor::SetTouchMode(quint32 mode, const QDBusMessage &request)
{
	const QSharedPointer<BleRcuTouchService> service = m_device->touchService();

	// if any unknown bits are set then (for now) that's treated as an error
	Future<> result;
	if (mode & ~0x3) {
		result = Future<>::createErrored(BleRcuError::errorString(BleRcuError::InvalidArg),
		                                 QStringLiteral("Invalid mode argument"));

	} else {

		// convert the supplied flags to a set of touch mode options
		BleRcuTouchService::TouchModeOptions modeOptions = 0;
		if (mode & 0x01)
			modeOptions |= BleRcuTouchService::TrackPadEnabled;
		if (mode & 0x02)
			modeOptions |= BleRcuTouchService::SliderEnabled;

		// ask the service to make the change
		result = service->setMode(modeOptions);
	}

	connectFutureToDBusReply(request, result);
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Slot called by the \l{BleRcuTouchService} object when when the touch mode
	changes. We hook this signal to emit a property change signal over dbus.

 */
void BleRcuDevice1Adaptor::onTouchModeChanged(quint8 touchMode)
{
	emitPropertyChanged<quint32>(QStringLiteral("TouchMode"), quint32(touchMode));
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Slot called by the \l{BleRcuTouchService} object when when the settability
	of the touch mode changes. We hook this signal to emit a property change
	signal over dbus.

 */
void BleRcuDevice1Adaptor::onTouchModeSettabilityChanged(bool settable)
{
	emitPropertyChanged<bool>(QStringLiteral("TouchModeSettable"), settable);
}

// -----------------------------------------------------------------------------
/*!
	DBus method call handler for com.sky.BleRcuDevice1.FindMe

 */
void BleRcuDevice1Adaptor::FindMe(quint8 level, qint32 duration,
                                  const QDBusMessage &request)
{
	Q_UNUSED(duration)

	const QSharedPointer<BleRcuFindMeService> service = m_device->findMeService();

	// call the method on the device, the result is async so we just need to
	// connect it to the dbus message for a delay response
	Future<> result;
	if (level == 0)
		result = service->stopBeeping();
	else if (level == 1)
		result = service->startBeeping(BleRcuFindMeService::Mid);
	else if (level == 2)
		result = service->startBeeping(BleRcuFindMeService::High);
	else {
		result = Future<>::createErrored(BleRcuError::errorString(BleRcuError::InvalidArg),
		                                 QStringLiteral("Invalid status argument"));
	}

	connectFutureToDBusReply(request, result);
}

// -----------------------------------------------------------------------------
/*!
	DBus get property call for com.sky.BleRcuDevice1.irCode

 */
qint32 BleRcuDevice1Adaptor::irCode() const
{
	const QSharedPointer<const BleRcuInfraredService> service = m_device->infraredService();
	return service->codeId();
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Slot called by the \l{BleRcuInfraredService} object when when the ir code id
	changes. We hook this signal to emit a property change signal over dbus.

 */
void BleRcuDevice1Adaptor::onCodeIdChanged(qint32 codeId)
{
	emitPropertyChanged(QStringLiteral("IrCode"), codeId);
}

// -----------------------------------------------------------------------------
/*!
	DBus method call handler for com.sky.BleRcuDevice1.EraseIrSignals

 */
void BleRcuDevice1Adaptor::EraseIrSignals(const QDBusMessage &request)
{
	const QSharedPointer<BleRcuInfraredService> service = m_device->infraredService();

	// erase the signals and convert the async results to a dbus reply
	Future<> result = service->eraseIrSignals();
	connectFutureToDBusReply(request, result);
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Converts a 16-bit CDI key code to an enum that the \l{BleRcuInfraredService}
	will understand.
 */
Qt::Key BleRcuDevice1Adaptor::convertCDIKeyCode(quint16 cdiKeyCode) const
{
	switch (cdiKeyCode) {
		case USER_INPUT_KEY_STANDBY:
			return Qt::Key_Standby;
		case USER_INPUT_KEY_VOLUME_UP:
			return Qt::Key_VolumeUp;
		case USER_INPUT_KEY_VOLUME_DOWN:
			return Qt::Key_VolumeDown;
		case USER_INPUT_KEY_MUTE:
			return Qt::Key_VolumeMute;
		case USER_INPUT_KEY_TV:
			return Qt::Key_Settings;

		default:
			return Qt::Key_unknown;
	}
}

// -----------------------------------------------------------------------------
/*!
	DBus method call handler for com.sky.BleRcuDevice1.ProgramIrSignals

	\note For some reason the QtDBus system doesn't seem to automatically
	demarshall the (qint32, QList<quint16>) arguments for this method call so
	we have to do it manually.

 */
void BleRcuDevice1Adaptor::ProgramIrSignals(qint32 codeId, const CdiKeyCodeList &keyCodes,
                                            const QDBusMessage &request)
{
	const QSharedPointer<BleRcuInfraredService> service = m_device->infraredService();

	// convert the CDI supplied key codes to our local enums
	QSet<Qt::Key> keys;
	if (keyCodes.isEmpty()) {

		// if the key set is empty we set all the available keys
		keys = { Qt::Key_Standby,
		         Qt::Key_Settings,  // input select
		         Qt::Key_VolumeUp,
		         Qt::Key_VolumeDown,
		         Qt::Key_VolumeMute };
	} else {

		// add all the supplied keys to the set
		for (const quint16 &cdiKeyCode : keyCodes) {

			// convert to a key code, on failure emit error and give up
			Qt::Key keyCode = convertCDIKeyCode(cdiKeyCode);
			if (keyCode == Qt::Key_unknown) {
				sendError(request, BleRcuError::InvalidArg,
				          QString("Invalid signal key code '%1'").arg(cdiKeyCode));
				return;
			}

			// add to the set to program
			keys.insert(keyCode);
		}
	}

	// program the signals and convert the async results to a dbus reply
	Future<> result = service->programIrSignals(codeId, keys);
	connectFutureToDBusReply(request, result);
}
void BleRcuDevice1Adaptor::ProgramIrSignalWaveforms(const IrKeyWaveforms &irWaveforms,
                                            const QDBusMessage &request)
{
	const QSharedPointer<BleRcuInfraredService> service = m_device->infraredService();

	QMap<Qt::Key, QByteArray> irSignalData;

	if (irWaveforms.isEmpty()) {
		sendError(request, BleRcuError::InvalidArg,
					QString("No waveform data received"));
		return;
	} else {
		// convert the CDI supplied key codes to our local enums and populate a map
		QMap<quint16, QByteArray>::const_iterator it = irWaveforms.begin();
		for (; it != irWaveforms.end(); ++it) {
			// convert to a key code, on failure emit error and give up
			Qt::Key keyCode = convertCDIKeyCode(it.key());
			if (keyCode == Qt::Key_unknown) {
				sendError(request, BleRcuError::InvalidArg,
				          QString("Invalid signal key code '%1'").arg(it.key()));
				return;
			}
			// add to the set to program
			irSignalData.insert(keyCode, it.value());
		}
	}

	// program the signals and convert the async results to a dbus reply
	Future<> result = service->programIrSignalWaveforms(irSignalData);
	connectFutureToDBusReply(request, result);
}

// -----------------------------------------------------------------------------
/*!
	DBus method call handler for com.sky.BleRcuDevice1.SendIrSignal

 */
void BleRcuDevice1Adaptor::SendIrSignal(quint16 keyCode,
                                        const QDBusMessage &request)
{
	const QSharedPointer<BleRcuInfraredService> service = m_device->infraredService();

	// convert the CDI supplied key code to our local enums
	Qt::Key key = convertCDIKeyCode(keyCode);
	if (key == Qt::Key_unknown) {
		sendError(request, BleRcuError::InvalidArg,
		          QString("Invalid signal key code '%1'").arg(keyCode));
		return;
	}

	// erase the signals and convert the async results to a dbus reply
	Future<> result = service->emitIrSignal(key);
	connectFutureToDBusReply(request, result);
}

// -----------------------------------------------------------------------------
/*!
	DBus method call handler for com.sky.BleRcuDevice1.SendRcuAction

 */
void BleRcuDevice1Adaptor::SendRcuAction(quint8 action, const QDBusMessage &message)
{
	const QSharedPointer<BleRcuRemoteControlService> service = m_device->remoteControlService();

	// erase the signals and convert the async results to a dbus reply
	Future<> result = service->sendRcuAction(action);
	connectFutureToDBusReply(message, result);
}

// -----------------------------------------------------------------------------
/*!
	DBus get property call for com.sky.BleRcuDevice1.UnpairReason

 */
quint8 BleRcuDevice1Adaptor::unpairReason() const
{
	const QSharedPointer<const BleRcuRemoteControlService> service = m_device->remoteControlService();
	return service->unpairReason();
}
// -----------------------------------------------------------------------------
/*!
	DBus get property call for com.sky.BleRcuDevice1.RebootReason

 */
quint8 BleRcuDevice1Adaptor::rebootReason() const
{
	const QSharedPointer<const BleRcuRemoteControlService> service = m_device->remoteControlService();
	return service->rebootReason();
}
// -----------------------------------------------------------------------------
/*!
	DBus get property call for com.sky.BleRcuDevice1.LastKeypress

 */
quint8 BleRcuDevice1Adaptor::lastKeypress() const
{
	const QSharedPointer<const BleRcuRemoteControlService> service = m_device->remoteControlService();
	return service->lastKeypress();
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Slot called by the \l{BleRcuTouchService} object when when the touch mode
	changes. We hook this signal to emit a property change signal over dbus.

 */
void BleRcuDevice1Adaptor::onUnpairReasonChanged(quint8 reason)
{
	emitPropertyChanged<quint8>(QStringLiteral("UnpairReason"), reason);
}

void BleRcuDevice1Adaptor::onRebootReasonChanged(quint8 reason)
{
	emitPropertyChanged<quint8>(QStringLiteral("RebootReason"), reason);
}

void BleRcuDevice1Adaptor::onLastKeypressChanged(quint8 key)
{
	emitPropertyChanged<quint8>(QStringLiteral("LastKeypress"), key);
}
