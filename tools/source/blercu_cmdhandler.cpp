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
//  blercu_cmdhandler.cpp
//  BleRcuConsole
//
//
//

#include "blercu_cmdhandler.h"

#include "blercu/blercucontroller1_interface.h"
#include "blercu/blercudebug1_interface.h"
#include "blercu/blercudevice1_interface.h"
#include "blercu/blercuinfrared1_interface.h"
#include "blercu/blercuupgrade1_interface.h"
#include "blercu/blercuhcicapture1_interface.h"

#include "utils/audiowavfile.h"

#include <QDebug>
#include <QDBusPendingReply>

#include <functional>

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>



#define USER_INPUT_KEY_STANDBY               (0xE000U)
#define USER_INPUT_KEY_VOLUME_UP             (0xE003U)
#define USER_INPUT_KEY_VOLUME_DOWN           (0xE004U)
#define USER_INPUT_KEY_MUTE                  (0xE005U)
#define USER_INPUT_KEY_TV                    (0xE010U)

namespace {
	enum PairingState
	{
		Initialsing,
		Idle,
		Searching,
		Pairing,
		Complete,
		Failed
	};
} // namespace

BleRcuCmdHandler::BleRcuCmdHandler(const QDBusConnection &bus,
                                   const QString &service,
                                   QObject *parent)
	: BaseCmdHandler(parent)
	, m_bus(bus)
	, m_serviceName(service)
{
	// initalise the dbus interfaces
	initBleRcuInterfaces(m_bus, m_serviceName,
	                     QStringLiteral("/com/sky/blercu/controller"));
}

BleRcuCmdHandler::~BleRcuCmdHandler()
{
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Initialises the blercu dbus interfaces.

 */
void BleRcuCmdHandler::initBleRcuInterfaces(const QDBusConnection &bus,
                                            const QString &service,
                                            const QString &path)
{
	// get the debug interface on the controller object
	m_blercuDebug1 = QSharedPointer<ComSkyBleRcuDebug1Interface>::create(service, path, bus);
	if (!m_blercuDebug1 || !m_blercuDebug1->isValid()) {
		qWarning() << "failed to create"
		           << ComSkyBleRcuDebug1Interface::staticInterfaceName() << "proxy";
		m_blercuDebug1.clear();
	}

	// get the hci capture interface on the controller object
	m_blercuHciCapture1 = QSharedPointer<ComSkyBleRcuHciCapture1Interface>::create(service, path, bus);
	if (!m_blercuHciCapture1 || !m_blercuHciCapture1->isValid()) {
		qWarning() << "failed to create"
		           << ComSkyBleRcuHciCapture1Interface::staticInterfaceName() << "proxy";
		m_blercuHciCapture1.clear();
	}


	// get the controller interface
	m_blercuController1 = QSharedPointer<ComSkyBleRcuController1Interface>::create(service, path, bus);
	if (!m_blercuController1 || !m_blercuController1->isValid()) {
		qCritical() << "failed to create"
		            << ComSkyBleRcuController1Interface::staticInterfaceName() << "proxy";
		m_blercuController1.clear();
		return;
	}

	// install signal handlers on the device added / removed signals
	if (m_blercuController1) {
		QObject::connect(m_blercuController1.data(), &ComSkyBleRcuController1Interface::DeviceAdded,
		                 this, &BleRcuCmdHandler::onDeviceAdded);
		QObject::connect(m_blercuController1.data(), &ComSkyBleRcuController1Interface::DeviceRemoved,
		                 this, &BleRcuCmdHandler::onDeviceRemoved);
		QObject::connect(m_blercuController1.data(), &ComSkyBleRcuController1Interface::pairingChanged,
		                 this, &BleRcuCmdHandler::onPairingStateChanged);
		QObject::connect(m_blercuController1.data(), &ComSkyBleRcuController1Interface::stateChanged,
		                 this, &BleRcuCmdHandler::onStateChanged);
	}


	// get the initial list of devices
	QDBusPendingReply< QList<QDBusObjectPath> > reply = m_blercuController1->GetDevices();
	reply.waitForFinished();
	if (reply.isError()) {
		showDBusError(reply.error());
		return;
	}

	// next go query each device object path
	const QList<QDBusObjectPath> devicePaths = reply;
	for (const QDBusObjectPath &objectPath : devicePaths) {

		// add the device to the internal map
		addDevice(objectPath);
	}
}

// -----------------------------------------------------------------------------
/*!
	\internal


 */
void BleRcuCmdHandler::addDevice(const QDBusObjectPath &path)
{
	//
	QSharedPointer<ComSkyBleRcuDevice1Interface> proxy =
		QSharedPointer<ComSkyBleRcuDevice1Interface>::create(m_serviceName, path.path(), m_bus);
	if (!proxy || !proxy->isValid()) {
		qWarning() << "failed to create dbus interface to" << path.path();
		return;
	}

	// get the device address
	BleAddress bdaddr(proxy->address());
	if (bdaddr.isNull()) {
		qWarning() << "invalid bdaddr for device at" << path.path();
		return;
	}

	// add the device to the map
	m_devices.insert(bdaddr, proxy);

	// connect to the property change signals on the device
	QObject::connect(proxy.data(), &ComSkyBleRcuDevice1Interface::batteryLevelChanged,
	                 this, std::bind(&BleRcuCmdHandler::onBatteryLevelChanged, this, bdaddr, std::placeholders::_1));
	QObject::connect(proxy.data(), &ComSkyBleRcuDevice1Interface::connectedChanged,
	                 this, std::bind(&BleRcuCmdHandler::onConnectedChanged, this, bdaddr, std::placeholders::_1));
	QObject::connect(proxy.data(), &ComSkyBleRcuDevice1Interface::irCodeChanged,
	                 this, std::bind(&BleRcuCmdHandler::onIrCodeChanged, this, bdaddr, std::placeholders::_1));
	QObject::connect(proxy.data(), &ComSkyBleRcuDevice1Interface::nameChanged,
	                 this, std::bind(&BleRcuCmdHandler::onNameChanged, this, bdaddr, std::placeholders::_1));
	QObject::connect(proxy.data(), &ComSkyBleRcuDevice1Interface::touchModeChanged,
	                 this, std::bind(&BleRcuCmdHandler::onTouchModeChanged, this, bdaddr, std::placeholders::_1));
	QObject::connect(proxy.data(), &ComSkyBleRcuDevice1Interface::audioStreamingChanged,
	                 this, std::bind(&BleRcuCmdHandler::onAudioStreamingChanged, this, bdaddr, std::placeholders::_1));
	QObject::connect(proxy.data(), &ComSkyBleRcuDevice1Interface::audioGainLevelChanged,
	                 this, std::bind(&BleRcuCmdHandler::onAudioGainLevelChanged, this, bdaddr, std::placeholders::_1));


	// create the proxy to the f/w upgrade service
	QSharedPointer<ComSkyBleRcuUpgrade1Interface> upgradeProxy =
		QSharedPointer<ComSkyBleRcuUpgrade1Interface>::create(m_serviceName, path.path(), m_bus);
	if (!upgradeProxy || !upgradeProxy->isValid()) {
		qWarning() << "failed to create dbus f/w upgrade interface to" << path.path();

	} else {

		// add the device to the map
		m_deviceUpgrades.insert(bdaddr, upgradeProxy);

		// connect to the property change upgrade signals
		QObject::connect(upgradeProxy.data(), &ComSkyBleRcuUpgrade1Interface::upgradingChanged,
		                 this, std::bind(&BleRcuCmdHandler::onFwUpgradeStateChanged, this, bdaddr, std::placeholders::_1));
		QObject::connect(upgradeProxy.data(), &ComSkyBleRcuUpgrade1Interface::progressChanged,
		                 this, std::bind(&BleRcuCmdHandler::onFwUpgradeProgressChanged, this, bdaddr, std::placeholders::_1));
	}

}

// -----------------------------------------------------------------------------
/*!
	\internal


 */
void BleRcuCmdHandler::removeDevice(const BleAddress &bdaddr)
{
	m_devices.remove(bdaddr);
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Prints the \a error out the console.
 */
void BleRcuCmdHandler::showDBusError(const QDBusError &error) const
{
	qWarning() << "dbus error" << error.name() << error.message();
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Get pairing state name from state.
 */

QString BleRcuCmdHandler::getPairingStateName(qint32 status) const
{
	switch(status) {
		case Initialsing: return "Initialising"; break;
		case Idle:        return "Idle"; break;
		case Searching:   return "Searching"; break;
		case Pairing:     return "Pairing"; break;
		case Complete:    return "Complete"; break;
		case Failed:      return "Failed"; break;
		default:          return "Unknown"; break;
	}
}

// -----------------------------------------------------------------------------
/*!
	Returns \c true if all the required dbus interfaces are setup.

 */
bool BleRcuCmdHandler::isValid()
{
	return (m_blercuController1 && m_blercuController1->isValid());
}

// -----------------------------------------------------------------------------
/*!
	Returns the console string to display in the interactive console.

 */
QString BleRcuCmdHandler::prompt()
{
	return QStringLiteral("[blercu]# ");
}

// -----------------------------------------------------------------------------
/*!
	Displays the current state of the controller.

 */
void BleRcuCmdHandler::show()
{
	if (!m_blercuController1 || !m_blercuController1->isValid()) {
		qWarning("Missing one or more required dbus interfaces");
		return;
	}

	// send the request to the daemon
	quint32 state = m_blercuController1->state();

	// we should now have everything (that's possible) to get from the daemon,
	// so print it all out the console
	qWarning().nospace() << "Controller";
	qWarning().nospace() << "\tState: " << getPairingStateName(state);
	qWarning().nospace() << "\tPairing: " << (m_blercuController1->pairing() ? "yes" : "no");
	qWarning().nospace() << "";

}

// -----------------------------------------------------------------------------
/*!
	Slot called when the user types 'devices', this command takes no arguments.

	This command lists all the devices managed by the daemon.

 */
void BleRcuCmdHandler::listDevices()
{
	if (!m_blercuController1 || !m_blercuController1->isValid()) {
		qWarning("Missing one or more required dbus interfaces");
		return;
	}

	if (m_devices.empty()) {
		qWarning("No devices");
		return;
	}

	QMap<BleAddress, QSharedPointer<ComSkyBleRcuDevice1Interface>>::const_iterator it = m_devices.begin();
	for (; it != m_devices.end(); ++it) {

		const BleAddress &bdaddr = it.key();
		const QSharedPointer<ComSkyBleRcuDevice1Interface> &proxy = it.value();

		QString name = proxy->name();

		qWarning().nospace() << "Device" << bdaddr << name;
	}
}

// -----------------------------------------------------------------------------
/*!
	Slot called when the user types 'connected-devices'.

	This just sends out a request to the daemon to get the list of connected
	devices.

 */
void BleRcuCmdHandler::listConnectedDevices()
{
	if (!m_blercuController1 || !m_blercuController1->isValid()) {
		qWarning("Missing one or more required dbus interfaces");
		return;
	}

	if (m_devices.empty()) {
		qWarning("No devices");
		return;
	}

	QMap<BleAddress, QSharedPointer<ComSkyBleRcuDevice1Interface>>::const_iterator it = m_devices.begin();
	for (; it != m_devices.end(); ++it) {

		const BleAddress &bdaddr = it.key();
		const QSharedPointer<ComSkyBleRcuDevice1Interface> &proxy = it.value();

		if (proxy->connected()) {
			const QString name = proxy->name();
			qWarning().nospace() << "Device" << bdaddr << name;
		}
	}
}

// -----------------------------------------------------------------------------
/*!
	Slot called when the user types 'pair on <code>'.

	This command sends out the request to start pairing.

 */
void BleRcuCmdHandler::startPairing(quint8 pairingCode)
{
	if (!m_blercuController1 || !m_blercuController1->isValid()) {
		qWarning("Missing one or more required dbus interfaces");
		return;
	}

	// send the request to the daemon
	QDBusPendingReply<> reply = m_blercuController1->StartPairing(pairingCode);
	reply.waitForFinished();
	if (reply.isError())
		showDBusError(reply.error());
}

// -----------------------------------------------------------------------------
/*!
	Slot called when the user types 'pair off'.

	This command sends out the request to stop pairing.

 */
void BleRcuCmdHandler::cancelPairing()
{
	if (!m_blercuController1 || !m_blercuController1->isValid()) {
		qWarning("Missing one or more required dbus interfaces");
		return;
	}

	// send the request to the daemon
	QDBusPendingReply<> reply = m_blercuController1->CancelPairing();
	reply.waitForFinished();
	if (reply.isError())
		showDBusError(reply.error());
}

// -----------------------------------------------------------------------------
/*!
	Slot called when the user types 'sacn <timeout>'.

	This command sends out the request to start scanning.

 */
void BleRcuCmdHandler::startScanning(uint32_t timeout)
{
	if (!m_blercuController1 || !m_blercuController1->isValid()) {
		qWarning("Missing one or more required dbus interfaces");
		return;
	}

	// send the request to the daemon
	QDBusPendingReply<> reply = m_blercuController1->StartScanning(timeout);
	reply.waitForFinished();
	if (reply.isError())
		showDBusError(reply.error());
}

// -----------------------------------------------------------------------------
/*!
	Slot called when the user types 'unpair <dev>'.

	This command unpairs the device and removes it from the daemon

 */
void BleRcuCmdHandler::unPairDevice(const BleAddress &device)
{
	Q_UNUSED(device);
	qWarning("Not implemented on BLERCU interface");
}

// -----------------------------------------------------------------------------
/*!
	Slot called when the user types 'info <dev>'.

	This command requests a bunch of stuff from the daemon

 */
void BleRcuCmdHandler::deviceInfo(const BleAddress &device)
{
	if (!m_devices.contains(device)) {
		qWarning() << "No device with address" << device;
		return;
	}

	QSharedPointer<ComSkyBleRcuDevice1Interface> proxy = m_devices[device];

	
	// we should now have everything (that's possible) to get from the daemon,
	// so print it all out the console
	qWarning() << "Device" << device.toString();
	qWarning() << "\tName: " << proxy->name();
	qWarning() << "\tConnected: " << (proxy->connected() ? "yes" : "no");
	qWarning() << "\tBatteryLevel: " << proxy->batteryLevel();
	qWarning() << "\tManufacturer: " << proxy->manufacturer();
	qWarning() << "\tModel: " << proxy->model();
	qWarning() << "\tSerialNumber: " << proxy->serialNumber();
	qWarning() << "\tSoftwareRevision: " << proxy->softwareRevision();
	qWarning() << "\tFirmwareRevision: " << proxy->firmwareRevision();
	qWarning() << "\tHardwareRevision: " << proxy->hardwareRevision();
	qWarning() << "\tTVCodeId: " << proxy->irCode();

	const quint32 touchMode = proxy->touchMode();
	qWarning() << "\tTouch: Pad: " << ((touchMode & 0x1) ? "enabled" : "disabled");
	qWarning() << "\tTouch: Slider: " << ((touchMode & 0x2) ? "enabled" : "disabled");

	qWarning() << "\tTouch: Settable: " << (proxy->touchModeSettable() ? "yes" : "no");

	qWarning() << "\tAudio: Streaming:" << (proxy->audioStreaming() ? "yes" : "no");
	qWarning() << "\tAudio: Gain:" << proxy->audioGainLevel();

	QDBusPendingReply<quint32, quint32, quint32> reply = proxy->GetAudioStatus();
	reply.waitForFinished();
	if (reply.isError()) {
		showDBusError(reply.error());
	} else {
		qWarning() << "\tAudio: LastError:" << reply.argumentAt<0>();
		qWarning() << "\tAudio: PacketsReceived:" << reply.argumentAt<1>();
		qWarning() << "\tAudio: PacketsExpected:" << reply.argumentAt<2>();
	}
}

// -----------------------------------------------------------------------------
/*!
	Slot called when the user types 'findme <dev> <off/mid/high>'.

	This sends a dbus request to the daemon to start / stop triggering findme.

 */
void BleRcuCmdHandler::findMe(const BleAddress &device, FindMeLevel level)
{
	// get the dbus proxy for the device
	QSharedPointer<ComSkyBleRcuDevice1Interface> proxy = m_devices.value(device);
	if (!proxy || !proxy->isValid()) {
		qWarning() << "No device with address" << device;
		return;
	}

	// send the request to the daemon
	QDBusPendingReply<> reply = proxy->FindMe(quint8(level), -1);
	reply.waitForFinished();
	if (reply.isError())
		showDBusError(reply.error());
}

// -----------------------------------------------------------------------------
/*!
	Slot called when the user types 'audio <dev> <start/stop>'.

	This sends a dbus request to the daemon to start / stop audio streaming.

 */
void BleRcuCmdHandler::startAudioStreaming(const BleAddress &device,
                                           AudioStreamingCodec codec,
                                           const QString &filePath)
{
	// get the dbus proxy for the device
	QSharedPointer<ComSkyBleRcuDevice1Interface> proxy = m_devices.value(device);
	if (!proxy || !proxy->isValid()) {
		qWarning() << "No device with address" << device;
		return;
	}

	// check the encoding type
	if (codec != PCM) {
		qWarning() << "Currently only PCM recordings are supported";
		return;
	}

	// create the wav file object to store the audio data
	m_wavFile = QSharedPointer<AudioWavFile>::create(filePath);
	if (!m_wavFile || !m_wavFile->isOpen()) {
		m_wavFile.clear();
		return;
	}

	// convert the encoding type
	quint32 encoding = 0;
	switch (codec) {
		case ADPCM: encoding = 1;   break;
		case PCM:   encoding = 2;   break;
	}

	// send the request to the daemon
	QDBusPendingReply<QDBusUnixFileDescriptor> reply = proxy->StartAudioStreaming(encoding);
	reply.waitForFinished();
	if (reply.isError())
		showDBusError(reply.error());

	// spawn an audio reader object to take the data and encode in a wav file
	m_wavFile->setPipeSource(reply.value().fileDescriptor());
}

// -----------------------------------------------------------------------------
/*!
	Slot called when the user types 'audio <dev> <stop>'.

	This sends a dbus request to the daemon to stop audio streaming.

 */
void BleRcuCmdHandler::stopAudioStreaming(const BleAddress &device)
{
	// get the dbus proxy for the device
	QSharedPointer<ComSkyBleRcuDevice1Interface> proxy = m_devices.value(device);
	if (!proxy || !proxy->isValid()) {
		qWarning() << "No device with address" << device;
		return;
	}

	// send the request to the daemon to stop audio streaming
	QDBusPendingReply<> reply = proxy->StopAudioStreaming();
	reply.waitForFinished();
	if (reply.isError())
		showDBusError(reply.error());

	// close the free the .wav file, this should stop the audio streaming
	// automatically
	m_wavFile.clear();

}

// -----------------------------------------------------------------------------
/*!
	Slot called when the user types 'set-audio-gain-level <dev> [level]'.

	This sends a dbus request to the daemon to set the audio gain level for the
	given device.

 */
void BleRcuCmdHandler::setAudioStreamingGain(const BleAddress &device, int level)
{
	// get the dbus proxy for the device
	QSharedPointer<ComSkyBleRcuDevice1Interface> proxy = m_devices.value(device);
	if (!proxy || !proxy->isValid()) {
		qWarning() << "No device with address" << device;
		return;
	}

	// send the request to the daemon
	proxy->setAudioGainLevel(level);
}

// -----------------------------------------------------------------------------
/*!
	Slot called when the user types 'touchpad <dev> <on/off>'.

	This sends a dbus request to the daemon to toggle the touch mode bits

 */
void BleRcuCmdHandler::setTrackpad(const BleAddress &device, bool on)
{
	Q_UNUSED(device);
	Q_UNUSED(on);
	qWarning("Not implemented on BLERCU interface");
}

// -----------------------------------------------------------------------------
/*!
	Slot called when the user types 'slider <dev> <on/off>'.

	This sends a dbus request to the daemon to toggle the touch mode bits

 */
void BleRcuCmdHandler::setSlider(const BleAddress &device, bool on)
{
	Q_UNUSED(device);
	Q_UNUSED(on);
	qWarning("Not implemented on BLERCU interface");
}


// -----------------------------------------------------------------------------
/*!
	\internal

	Converts a name of a signalto a key code.
 */
quint16 BleRcuCmdHandler::irSignalNameToKeyCode(const QString &name) const
{
	if (name.compare("power", Qt::CaseInsensitive) == 0)
		return USER_INPUT_KEY_STANDBY;
	if (name.compare("volume_up", Qt::CaseInsensitive) == 0)
		return USER_INPUT_KEY_VOLUME_UP;
	if (name.compare("volume_down", Qt::CaseInsensitive) == 0)
		return USER_INPUT_KEY_VOLUME_DOWN;
	if (name.compare("mute", Qt::CaseInsensitive) == 0)
		return USER_INPUT_KEY_MUTE;
	if (name.compare("input", Qt::CaseInsensitive) == 0)
		return USER_INPUT_KEY_TV;

	return 0;
}

// -----------------------------------------------------------------------------
/*!
	Slot called when the user types 'ir-program <dev> <code> [signals...]'.


 */
void BleRcuCmdHandler::programIrSignals(const BleAddress &device, quint32 code,
                                        const QStringList &irSignals)
{
	// get the dbus proxy for the device
	QSharedPointer<ComSkyBleRcuDevice1Interface> proxy = m_devices.value(device);
	if (!proxy || !proxy->isValid()) {
		qWarning() << "No device with address" << device;
		return;
	}

	// convert the signal string list a key code list
	QList<quint16> signals_;
	for (const QString &irSignal : irSignals) {

		quint16 keyCode = irSignalNameToKeyCode(irSignal);
		if (keyCode == 0) {
			qWarning() << "Invalid IR signal name";
			return;
		}

		signals_.append(keyCode);
	}

	// send the request to the daemon
	QDBusPendingReply<> reply = proxy->ProgramIrSignals(code, signals_);
	reply.waitForFinished();
	if (reply.isError())
		showDBusError(reply.error());
}

// -----------------------------------------------------------------------------
/*!
	Slot called when the user types 'ir-erase <dev>'.

 */
void BleRcuCmdHandler::eraseIrSignals(const BleAddress &device)
{
	// get the dbus proxy for the device
	QSharedPointer<ComSkyBleRcuDevice1Interface> proxy = m_devices.value(device);
	if (!proxy || !proxy->isValid()) {
		qWarning() << "No device with address" << device;
		return;
	}

	// send the request to the daemon
	QDBusPendingReply<> reply = proxy->EraseIrSignals();
	reply.waitForFinished();
	if (reply.isError())
		showDBusError(reply.error());
}

// -----------------------------------------------------------------------------
/*!
	Slot called when the user types 'ir-send <dev> <signal>'.

 */
void BleRcuCmdHandler::sendIrSignal(const BleAddress &device, const QString &signal)
{
	// get the dbus proxy for the device
	QSharedPointer<ComSkyBleRcuDevice1Interface> proxy = m_devices.value(device);
	if (!proxy || !proxy->isValid()) {
		qWarning() << "No device with address" << device;
		return;
	}

	quint16 keyCode = irSignalNameToKeyCode(signal);
	if (keyCode == 0) {
		qWarning() << "Invalid IR signal name";
		return;
	}

	// send the request to the daemon
	QDBusPendingReply<> reply = proxy->SendIrSignal(keyCode);
	reply.waitForFinished();
	if (reply.isError())
		showDBusError(reply.error());
}

// -----------------------------------------------------------------------------
/*!
	Slot called when the user types 'ir-parse-edid <dev> [async] <edid>'.

 */
void BleRcuCmdHandler::parseEDID(const BleAddress &device, bool async,
                                 const QByteArray &edid)
{
	// get the dbus proxy for the device
	QSharedPointer<ComSkyBleRcuDevice1Interface> proxy = m_devices.value(device);
	if (!proxy || !proxy->isValid()) {
		qWarning() << "No device with address" << device;
		return;
	}

	// we don't support async mode (aka send results in signal rather than reply)
	if (async) {
		qWarning() << "Async mode not supported on BleRcu interface";
		return;
	}

	// use the above proxy object path to get the infrared interface proxy
	ComSkyBleRcuInfrared1Interface infraredProxy(m_serviceName, proxy->path(), m_bus);
	if (!infraredProxy.isValid()) {
		qWarning() << "Failed to create infrared1 interface proxy object";
		return;
	}

	QDBusPendingReply<IrCodeList> reply = infraredProxy.GetCodesFromEDID(edid);
	reply.waitForFinished();
	if (reply.isError()) {
		showDBusError(reply.error());
		return;
	}

	// print out all the IR codes found
	IrCodeList irCodes = reply;
	for (const qint32 &irCode : irCodes) {
		qWarning().nospace() << irCode << ",";
	}
}

// -----------------------------------------------------------------------------
/*!
	Slot called when the user types 'ir-get-manuf <dev> <search> [sort] [amp/tv]'.

 */
void BleRcuCmdHandler::getIrManufacturers(const BleAddress &device,
                                          const QString &search,
                                          IrLookupType type,
                                          bool sort)
{
	// get the dbus proxy for the device
	QSharedPointer<ComSkyBleRcuDevice1Interface> proxy = m_devices.value(device);
	if (!proxy || !proxy->isValid()) {
		qWarning() << "No device with address" << device;
		return;
	}

	// use the above proxy object path to get the infrared interface proxy
	ComSkyBleRcuInfrared1Interface infraredProxy(m_serviceName, proxy->path(), m_bus);
	if (!infraredProxy.isValid()) {
		qWarning() << "Failed to create infrared1 interface proxy object";
		return;
	}

	// generate the flags for the given type and sort settings
	quint32 flags = 0;
	if (type == IrLookupType::TV)
		flags |= 0x4;
	else if (type == IrLookupType::AVAmp)
		flags |= 0x2;

	if (!sort)
		flags |= 0x1;

	//
	QDBusPendingReply<quint64, QStringList> reply = infraredProxy.GetManufacturers(search, flags, -1, -1);
	reply.waitForFinished();
	if (reply.isError()) {
		showDBusError(reply.error());
		return;
	}

	// print out all the manufacturers found
	const QStringList manufacturers = reply.argumentAt<1>();
	if (manufacturers.isEmpty()) {
		qWarning("No manufacturers found for the given type and search term");
		return;
	}

	for (const QString &manufacturer : manufacturers) {
		qWarning().nospace() << manufacturer << ",";
	}
}

// -----------------------------------------------------------------------------
/*!
	Slot called when the user types 'ir-get-model <dev> <manuf> <search> [amp/tv]'.

 */
void BleRcuCmdHandler::getIrModels(const BleAddress &device,
                                   const QString &manuf,
                                   const QString &search,
                                   IrLookupType type)
{
	// get the dbus proxy for the device
	QSharedPointer<ComSkyBleRcuDevice1Interface> proxy = m_devices.value(device);
	if (!proxy || !proxy->isValid()) {
		qWarning() << "No device with address" << device;
		return;
	}

	// use the above proxy object path to get the infrared interface proxy
	ComSkyBleRcuInfrared1Interface infraredProxy(m_serviceName, proxy->path(), m_bus);
	if (!infraredProxy.isValid()) {
		qWarning() << "Failed to create infrared1 interface proxy object";
		return;
	}

	// generate the flags for the given type and sort settings
	quint32 flags = 0;
	if (type == IrLookupType::TV)
		flags |= 0x4;
	else if (type == IrLookupType::AVAmp)
		flags |= 0x2;

	//
	QDBusPendingReply<quint64, QStringList> reply = infraredProxy.GetModels(manuf, search, flags, -1, -1);
	reply.waitForFinished();
	if (reply.isError()) {
		showDBusError(reply.error());
		return;
	}

	// print out all the models found
	const QStringList models = reply.argumentAt<1>();
	if (models.isEmpty()) {
		qWarning("No models found for the given type and search term");
		return;
	}

	for (const QString &model : models) {
		qWarning().nospace() << model << ",";
	}
}

// -----------------------------------------------------------------------------
/*!
	Slot called when the user types 'ir-get-codes <dev> <manuf> <model> [amp/tv]'.

 */
void BleRcuCmdHandler::getIrCodes(const BleAddress &device, const QString &manuf,
                                  const QString &model, IrLookupType type)
{
	// get the dbus proxy for the device
	QSharedPointer<ComSkyBleRcuDevice1Interface> proxy = m_devices.value(device);
	if (!proxy || !proxy->isValid()) {
		qWarning() << "No device with address" << device;
		return;
	}

	// use the above proxy object path to get the infrared interface proxy
	ComSkyBleRcuInfrared1Interface infraredProxy(m_serviceName, proxy->path(), m_bus);
	if (!infraredProxy.isValid()) {
		qWarning() << "Failed to create infrared1 interface proxy object";
		return;
	}

	// generate the flags for the given type and sort settings
	quint32 flags = 0;
	if (type == IrLookupType::TV)
		flags |= 0x4;
	else if (type == IrLookupType::AVAmp)
		flags |= 0x2;

	//
	QDBusPendingReply<IrCodeList> reply = infraredProxy.GetCodes(manuf, model, flags);
	reply.waitForFinished();
	if (reply.isError()) {
		showDBusError(reply.error());
		return;
	}

	// print out all the models found
	const IrCodeList codes = reply;
	if (codes.isEmpty()) {
		qWarning("No IR codes found for the given manufacturer and model");
		return;
	}

	for (const qint32 &code : codes) {
		qWarning().nospace() << code << ",";
	}
}

// -----------------------------------------------------------------------------
/*!
	Slot called when the user types 'fw-upgrade start <filepath>'

 */
void BleRcuCmdHandler::fwStartUpgrade(const BleAddress &device,
                                      const QString &filePath)
{
	// get the dbus proxy for the device
	QSharedPointer<ComSkyBleRcuUpgrade1Interface> proxy = m_deviceUpgrades.value(device);
	if (!proxy || !proxy->isValid()) {
		qWarning() << "No device with address" << device;
		return;
	}

	// open the file
	int fd = ::open(qPrintable(filePath), O_CLOEXEC | O_RDONLY);
	if (fd < 0) {
		qErrnoWarning(errno, "Failed to open file @ '%s'", qPrintable(filePath));
		return;
	}

	QDBusUnixFileDescriptor fileDescriptor(fd);

	if (::close(fd) != 0)
		qErrnoWarning(errno, "Failed to close file @ '%s'", qPrintable(filePath));

	// request the start of the upgrade
	QDBusPendingReply<> reply = proxy->StartUpgrade(fileDescriptor);
	reply.waitForFinished();
	if (reply.isError())
		showDBusError(reply.error());
}

// -----------------------------------------------------------------------------
/*!
	Slot called when the user types 'fw-upgrade cancel'

 */
void BleRcuCmdHandler::fwCancelUpgrade(const BleAddress &device)
{
	// get the dbus proxy for the device
	QSharedPointer<ComSkyBleRcuUpgrade1Interface> proxy = m_deviceUpgrades.value(device);
	if (!proxy || !proxy->isValid()) {
		qWarning() << "No device with address" << device;
		return;
	}

	// request the cancel of the upgrade
	QDBusPendingReply<> reply = proxy->CancelUpgrade();
	reply.waitForFinished();
	if (reply.isError())
		showDBusError(reply.error());
}

// -----------------------------------------------------------------------------
/*!
	Slot called when the user types 'log-level' with no args.

 */
void BleRcuCmdHandler::getLogLevel() const
{
	if (!m_blercuDebug1) {
		qWarning() << "Failed to get debug interface";
		return;
	}

	const quint32 levels = m_blercuDebug1->logLevels();
	qWarning("Log levels enabled:");
	qWarning("\tdebug     : %s", (levels & 0x020) ? "yes" : "no");
	qWarning("\tinfo      : %s", (levels & 0x010) ? "yes" : "no");
	qWarning("\tmilestone : %s", (levels & 0x008) ? "yes" : "no");
	qWarning("\twarning   : %s", (levels & 0x004) ? "yes" : "no");
	qWarning("\terror     : %s", (levels & 0x002) ? "yes" : "no");
	qWarning("\tfatal     : %s", (levels & 0x001) ? "yes" : "no");
}

// -----------------------------------------------------------------------------
/*!
	Slot called when the user types 'log-level <fatal/error/warning/...>'.

 */
void BleRcuCmdHandler::setLogLevel(const QString &level)
{
	static const QMap<QString, quint32> logLevels = {
		{ QStringLiteral("fatal"),     0x001 },
		{ QStringLiteral("error"),     0x003 },
		{ QStringLiteral("warning"),   0x007 },
		{ QStringLiteral("milestone"), 0x00f },
		{ QStringLiteral("info"),      0x01f },
		{ QStringLiteral("debug"),     0x03f },
	};

	if (!logLevels.contains(level)) {
		qWarning("unknown log level");
		return;
	}

	m_blercuDebug1->setLogLevels(logLevels[level]);
}

// -----------------------------------------------------------------------------
/*!
	Slot called when the user types 'log-syslog'.

 */
void BleRcuCmdHandler::getLogSyslog() const
{
	if (!m_blercuDebug1) {
		qWarning() << "Failed to get debug interface";
		return;
	}

	const bool enabled = m_blercuDebug1->logToSysLog();
	qWarning("%sabled", enabled ? "en" : "dis");
}

// -----------------------------------------------------------------------------
/*!
	Slot called when the user types 'log-syslog <on/off>'.

 */
void BleRcuCmdHandler::setLogSyslog(bool enable)
{
	if (!m_blercuDebug1) {
		qWarning() << "Failed to get debug interface";
		return;
	}

	m_blercuDebug1->setLogToSysLog(enable);
}

// -----------------------------------------------------------------------------
/*!
	Slot called when the user types 'log-ethalog'.

 */
void BleRcuCmdHandler::getLogEthanlog() const
{
	if (!m_blercuDebug1) {
		qWarning() << "Failed to get debug interface";
		return;
	}

	const bool enabled = m_blercuDebug1->logToEthanLog();
	qWarning("%sabled", enabled ? "en" : "dis");
}

// -----------------------------------------------------------------------------
/*!
	Slot called when the user types 'log-ethalog <on/off>'.

 */
void BleRcuCmdHandler::setLogEthanlog(bool enable)
{
	if (!m_blercuDebug1) {
		qWarning() << "Failed to get debug interface";
		return;
	}

	m_blercuDebug1->setLogToEthanLog(enable);
}

// -----------------------------------------------------------------------------
/*!
	Slot called when the user types 'hci-capture'.

 */
void BleRcuCmdHandler::getHciCaptureState() const
{
	if (!m_blercuHciCapture1) {
		qWarning() << "Failed to get hci capture interface";
		return;
	}

	const bool enabled = m_blercuHciCapture1->isCapturing();
	qWarning("%sabled", enabled ? "en" : "dis");
}

// -----------------------------------------------------------------------------
/*!
	Slot called when the user types 'hci-capture on'.

 */
void BleRcuCmdHandler::enableHciCapture()
{
	if (!m_blercuHciCapture1) {
		qWarning() << "Failed to get hci capture interface";
		return;
	}

	QDBusPendingReply<> reply = m_blercuHciCapture1->Enable();
	reply.waitForFinished();
	if (reply.isError())
		showDBusError(reply.error());
}

// -----------------------------------------------------------------------------
/*!
	Slot called when the user types 'hci-capture off'.

 */
void BleRcuCmdHandler::disableHciCapture()
{
	if (!m_blercuHciCapture1) {
		qWarning() << "Failed to get hci capture interface";
		return;
	}

	QDBusPendingReply<> reply = m_blercuHciCapture1->Disable();
	reply.waitForFinished();
	if (reply.isError())
		showDBusError(reply.error());
}

// -----------------------------------------------------------------------------
/*!
	Slot called when the user types 'hci-capture clear'.

 */
void BleRcuCmdHandler::clearHciCapture()
{
	if (!m_blercuHciCapture1) {
		qWarning() << "Failed to get hci capture interface";
		return;
	}

	QDBusPendingReply<> reply = m_blercuHciCapture1->Clear();
	reply.waitForFinished();
	if (reply.isError())
		showDBusError(reply.error());
}

// -----------------------------------------------------------------------------
/*!
	Slot called when the user types 'hci-capture dump <file>'.

 */
void BleRcuCmdHandler::dumpHciCapture(const QString &filePath)
{
	if (!m_blercuHciCapture1) {
		qWarning() << "Failed to get hci capture interface";
		return;
	}

	// try and open the file for storing the dump
	int fd = ::open(qPrintable(filePath), O_CLOEXEC | O_WRONLY | O_CREAT | O_TRUNC, 0666);
	if (fd < 0) {
		qErrnoWarning(errno, "Failed to open file @ '%s'", qPrintable(filePath));
		return;
	}

	QDBusUnixFileDescriptor fileDescriptor(fd);

	if (::close(fd) != 0)
		qErrnoWarning(errno, "Failed to close file @ '%s'", qPrintable(filePath));


	// request the daemon to dump to the given file
	QDBusPendingReply<> reply = m_blercuHciCapture1->Dump(fileDescriptor);
	reply.waitForFinished();
	if (reply.isError())
		showDBusError(reply.error());
}

// -----------------------------------------------------------------------------
/*!
	Slot called when the com.sky.BleRcuController1.DeviceAdded dbus signal
	is received.

 */
void BleRcuCmdHandler::onDeviceAdded(const QDBusObjectPath &path,
                                     const QString &address)
{
	Q_UNUSED(path);

	addDevice(path);

	qWarning().nospace() << "[NEW] Device" << address;
}

// -----------------------------------------------------------------------------
/*!
	Slot called when the com.sky.BleRcuController1.DeviceRemoved dbus signal
	is received.

 */
void BleRcuCmdHandler::onDeviceRemoved(const QDBusObjectPath &path,
                                       const QString &address)
{
	Q_UNUSED(path);

	removeDevice(BleAddress(address));

	qWarning().nospace() << "[DEL] Device" << address;
}

// -----------------------------------------------------------------------------
/*!
	Slot called when the com.sky.BleRcuController1.Pairing property change
	signal is received.

 */
void BleRcuCmdHandler::onPairingStateChanged(bool isPairing)
{
	qWarning().nospace() << "[CHG] Pairing:" << (isPairing ? "yes" : "no");
}

// -----------------------------------------------------------------------------
/*!
	Slot called when the com.sky.BleRcuController1.Status property change
	signal is received.

 */
void BleRcuCmdHandler::onStateChanged(quint32 status)
{
	qWarning().nospace() << "[CHG] State:" << getPairingStateName(status);
}

// -----------------------------------------------------------------------------
/*!
	Slot called when the com.sky.BleRcuDevice1.BatteryLevel property change
	signal is received.

 */
void BleRcuCmdHandler::onBatteryLevelChanged(const BleAddress &device, quint8 level)
{
	qWarning().nospace() << "[CHG] Device" << device << "BatteryLevel:" << level;
}

// -----------------------------------------------------------------------------
/*!
	Slot called when the com.sky.BleRcuDevice1.Connected property change
	signal is received.

 */
void BleRcuCmdHandler::onConnectedChanged(const BleAddress &device, bool connected)
{
	qWarning().nospace() << "[CHG] Device" << device << "Connected:" << (connected ? "yes" : "no");
}

// -----------------------------------------------------------------------------
/*!
	Slot called when the com.sky.BleRcuDevice1.Name property change
	signal is received.

 */
void BleRcuCmdHandler::onNameChanged(const BleAddress &device, const QString &name)
{
	qWarning().nospace() << "[CHG] Device" << device << "Name:" << name;
}

// -----------------------------------------------------------------------------
/*!
	Slot called when the com.sky.BleRcuDevice1.IrCode property change
	signal is received.

 */
void BleRcuCmdHandler::onIrCodeChanged(const BleAddress &device, quint32 code)
{
	qWarning().nospace() << "[CHG] Device" << device << "IrCode:" << code;
}

// -----------------------------------------------------------------------------
/*!
	Slot called when the com.sky.BleRcuDevice1.TouchMode property change
	signal is received.

 */
void BleRcuCmdHandler::onTouchModeChanged(const BleAddress &device, quint32 mode)
{
	qWarning().nospace() << "[CHG] Device" << device << "TouchMode:" << mode;
}

// -----------------------------------------------------------------------------
/*!
	Slot called when the com.sky.BleRcuDevice1.AudioStreaming property change
	signal is received.

 */
void BleRcuCmdHandler::onAudioStreamingChanged(const BleAddress &device, bool streaming)
{
	qWarning().nospace() << "[CHG] Device" << device << "AudioStreaming:" << (streaming ? "yes" : "no");
}

// -----------------------------------------------------------------------------
/*!
	Slot called when the com.sky.BleRcuDevice1.AudioGainLevel property change
	signal is received.

 */
void BleRcuCmdHandler::onAudioGainLevelChanged(const BleAddress &device, qint32 gainLevel)
{
	qWarning().nospace() << "[CHG] Device" << device << "AudioGainLevel:" << gainLevel;
}

// -----------------------------------------------------------------------------
/*!
	Slot called when the com.sky.BleRcuUpdate1.Upgrading property change
	signal is received.

 */
void BleRcuCmdHandler::onFwUpgradeStateChanged(const BleAddress &device, bool isUpgrading)
{
	qWarning().nospace() << "[CHG] Device" << device << "F/W Upgrading:" << isUpgrading;
}

// -----------------------------------------------------------------------------
/*!
	Slot called when the com.sky.BleRcuDevice1.Progress property change
	signal is received.

 */
void BleRcuCmdHandler::onFwUpgradeProgressChanged(const BleAddress &device, qint32 progress)
{
	qWarning().nospace() << "[CHG] Device" << device << "F/W Upgrade Progress:" << progress;
}

