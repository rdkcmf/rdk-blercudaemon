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
//  gatt_upgradeservice.cpp
//  SkyBluetoothRcu
//

#include "gatt_upgradeservice.h"

#include "blercu/blegattservice.h"
#include "blercu/blegattcharacteristic.h"
#include "blercu/blegattdescriptor.h"

#include "utils/fwimagefile.h"
#include "utils/logging.h"
#include "utils/crc32.h"

#include <QtEndian>

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>


/// Possible packet opcodes
#define OPCODE_WRQ                    quint8(0x0 << 6)
#define OPCODE_DATA                   quint8(0x1 << 6)
#define OPCODE_ACK                    quint8(0x2 << 6)
#define OPCODE_ERROR                  quint8(0x3 << 6)

#define OPCODE_MASK                   quint8(0x3 << 6)

/// The maximum number of data bytes in a DATA packet
#define FIRMWARE_PACKET_MTU           18


/// The structure representing the contents of the control point characteristic
struct Q_PACKED ControlPoint {
	quint32 deviceModelId;
	quint32 firmwareVersion;
	quint32 firmwareCrc32;
};
Q_STATIC_ASSERT(sizeof(ControlPoint) == 12);




GattUpgradeService::GattUpgradeService()
	: BleRcuUpgradeService(nullptr)
	, m_ready(false)
	, m_progress(-1)
	, m_windowSize(5)
{

	// set the timer as single shot and set the default timeout time
	// (6000ms is chosen because it's slightly longer than the 5s slave latency)
	m_timeoutTimer.setInterval(6000);
	m_timeoutTimer.setSingleShot(true);

	// connect up the timer signal, the timer is not started till the upgrade
	// process is begun
	QObject::connect(&m_timeoutTimer, &QTimer::timeout,
	                 this, &GattUpgradeService::onTimeout);


	// initialise the state machine for the upgrades
	init();

}

GattUpgradeService::~GattUpgradeService()
{
	// clean up the firmware file
	m_fwFile.reset();
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Sets up the state machine used for the firmware download.

 */
void GattUpgradeService::init()
{
	// set the name of the state machine for logging
	m_stateMachine.setObjectName(QStringLiteral("GattUpgradeService"));

	// log the transitions at milestone level
	m_stateMachine.setTransistionLogLevel(QtInfoMsg, &milestone());

	// add all the states
	m_stateMachine.addState(InitialState, QStringLiteral("Initial"));

	m_stateMachine.addState(SendingSuperState, QStringLiteral("SendingSuperState"));
	m_stateMachine.addState(SendingSuperState, SendingWriteRequestState, QStringLiteral("SendingWriteRequest"));
	m_stateMachine.addState(SendingSuperState, SendingDataState, QStringLiteral("SendingData"));

	m_stateMachine.addState(ErroredState, QStringLiteral("Errored"));
	m_stateMachine.addState(FinishedState, QStringLiteral("Finished"));


	// add the transitions       From State           ->    Event              ->   To State
	m_stateMachine.addTransition(InitialState,              CancelledEvent,         FinishedState);
	m_stateMachine.addTransition(InitialState,              StopServiceEvent,       ErroredState);
	m_stateMachine.addTransition(InitialState,              EnableNotifyErrorEvent, ErroredState);
	m_stateMachine.addTransition(InitialState,              ReadErrorEvent,         ErroredState);
	m_stateMachine.addTransition(InitialState,              FinishedSetupEvent,     SendingWriteRequestState);

	m_stateMachine.addTransition(SendingSuperState,         CancelledEvent,         FinishedState);
	m_stateMachine.addTransition(SendingSuperState,         StopServiceEvent,       ErroredState);
	m_stateMachine.addTransition(SendingSuperState,         WriteErrorEvent,        ErroredState);
	m_stateMachine.addTransition(SendingSuperState,         PacketErrorEvent,       ErroredState);
	m_stateMachine.addTransition(SendingSuperState,         TimeoutErrorEvent,      ErroredState);
	m_stateMachine.addTransition(SendingWriteRequestState,  PacketAckEvent,         SendingDataState);
	m_stateMachine.addTransition(SendingDataState,          CompleteEvent,          FinishedState);

	m_stateMachine.addTransition(ErroredState,              CompleteEvent,          FinishedState);


	// connect to the state entry and exit signals
	QObject::connect(&m_stateMachine, &StateMachine::entered,
	                 this, &GattUpgradeService::onStateEntry);
	QObject::connect(&m_stateMachine, &StateMachine::exited,
	                 this, &GattUpgradeService::onStateExit);


	// set the initial state
	m_stateMachine.setInitialState(InitialState);
	m_stateMachine.setFinalState(FinishedState);
}

// -----------------------------------------------------------------------------
/*!
	Returns the uuid of the upgrade service.

 */
BleUuid GattUpgradeService::uuid()
{
	return BleUuid(BleUuid::SkyQFirmwareUpgrade);
}

// -----------------------------------------------------------------------------
/*!
	This service is always ready.

 */
bool GattUpgradeService::isReady() const
{
	return m_ready;
}

// -----------------------------------------------------------------------------
/*!
	Should be called whenever the bluez GATT profile is updated, this is used
	to update the internal dbus proxies to the GATT characteristics &
	descriptors.


 */
bool GattUpgradeService::start(const QSharedPointer<const BleGattService> &gattService)
{
	// this service doesn't have a 'ready' state-machine, it's always ready if
	// started and not ready when stopped
	m_ready = true;


	// try and get the gatt characteristic for the OTA control point
	if (!m_controlCharacteristic || !m_controlCharacteristic->isValid()) {

		m_controlCharacteristic = gattService->characteristic(BleUuid::FirmwareControlPoint);
		if (!m_controlCharacteristic || !m_controlCharacteristic->isValid()) {
			qWarning("failed get the f/w upgrade control point gatt proxy");
			m_controlCharacteristic.reset();
			return true;
		}

	}

	// try and get the gatt characteristic for the OTA packet
	if (!m_packetCharacteristic || !m_packetCharacteristic->isValid()) {

		m_packetCharacteristic = gattService->characteristic(BleUuid::FirmwarePacket);
		if (!m_packetCharacteristic || !m_packetCharacteristic->isValid()) {
			qWarning("failed get the f/w upgrade packet gatt proxy");
			m_packetCharacteristic.reset();
			return true;
		}

		// connect to the notification signal for when the remote device
		// is sending ACK or ERROR packets back to us
		QObject::connect(m_packetCharacteristic.data(), &BleGattCharacteristic::valueChanged,
		                 this, &GattUpgradeService::onPacketNotification,
		                 Qt::UniqueConnection);
	}

	// try and get the gatt window size descriptor, this is optional
	if (!m_windowSizeDescriptor || !m_windowSizeDescriptor->isValid()) {

		m_windowSizeDescriptor =
			m_packetCharacteristic->descriptor(BleUuid::FirmwarePacketWindowSize);
		if (!m_windowSizeDescriptor || !m_windowSizeDescriptor->isValid()) {

			// this descriptor is optional so don't log an error if it wasn't found
			m_windowSizeDescriptor.reset();
		}

	}

	return true;
}

// -----------------------------------------------------------------------------
/*!

 */
void GattUpgradeService::stop()
{
	// no longer ready
	m_ready = false;

	// if a download is in progress then it's cancelled
	if (m_stateMachine.isRunning()) {
		m_lastError = QStringLiteral("Device disconnected");
		m_stateMachine.postEvent(StopServiceEvent);
	}
}

// -----------------------------------------------------------------------------
/*!
	\internal

 */
Future<> GattUpgradeService::createFutureError(BleRcuError::ErrorType type,
                                               const QString &message) const
{
	return Future<>::createErrored(BleRcuError::errorString(type), message);
}

// -----------------------------------------------------------------------------
/*!
	\reimp

	Attempts to starts the firmware update.

 */
Future<> GattUpgradeService::startUpgrade(const QSharedPointer<FwImageFile> &fwFile)
{
	// if state machine is running it means an upgrade is already in progress
	if (m_stateMachine.isRunning())
		return createFutureError(BleRcuError::Busy,
		                         QStringLiteral("Upgrade in progress"));

	// reset the state (just in case it wasn't cleaned up correctly)
	m_fwFile.clear();

	// check we have the necessary characteristic proxes
	if (!m_packetCharacteristic || !m_controlCharacteristic)
		return createFutureError(BleRcuError::General,
		                         QStringLiteral("Upgrade service not ready"));

	// sanity check the f/w image file is valid
	if (!fwFile || !fwFile->isValid())
		return createFutureError(BleRcuError::FileNotFound,
		                         QStringLiteral("Invalid file descriptor"));


	// store the firmware image file locally
	m_fwFile = fwFile;

	// set the initial progress
	m_progress = 0;

	// finally start the state machine
	m_stateMachine.start();

	// create a promise to signal success of failure once we get to a certain
	// place in the state-machine
	m_startPromise = QSharedPointer< Promise<> >::create();
	return m_startPromise->future();
}

// -----------------------------------------------------------------------------
/*!
	Attempts to stop / cancel a running firmware upgrade.

	This event may return without the upgrading being stopped, instead obvserve
	the finished() signal.

 */
Future<> GattUpgradeService::cancelUpgrade()
{
	// if state machine is not running then no need to do anything
	if (!m_stateMachine.isRunning())
		return createFutureError(BleRcuError::Rejected,
		                         QStringLiteral("Upgrade not running"));

	// post a cancel to the running state machine
	m_stateMachine.postEvent(CancelledEvent);

	// we've requested to cancel the upgrade so this is success
	return Future<>::createFinished();
}

// -----------------------------------------------------------------------------
/*!
	Returns \c true if we're currently in the process of updating the firmware
	image.

 */
bool GattUpgradeService::upgrading() const
{
	return m_stateMachine.isRunning();
}

// -----------------------------------------------------------------------------
/*!
	Returns the current progress of an upgrade as a percentage value from 0 to
	100.  If an upgrade is not in progress then -1 is returned.

 */
int GattUpgradeService::progress() const
{
	return m_progress;
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Called on entry to a new state or super state.
 */
void GattUpgradeService::onStateEntry(int state)
{
	switch (state) {
		case InitialState:
			onEnteredInitialState();
			break;
		case SendingWriteRequestState:
			onEnteredSendWriteRequestState();
			break;
		case SendingDataState:
			onEnteredSendingDataState();
			break;

		case ErroredState:
			onEnteredErroredState();
			break;
		case FinishedState:
			onEnteredFinishedState();
			break;

		default:
			break;
	}
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Called on exit from a state or super state.
 */
void GattUpgradeService::onStateExit(int state)
{
	Q_UNUSED(state);

}

// -----------------------------------------------------------------------------
/*!
	\internal


 */
void GattUpgradeService::onEnteredInitialState()
{
	// clear the setup flags
	m_setupFlags = 0;

	// all the following operations are async, they will set a flag in m_setupFlags
	// once complete or in case of an error they will post error events to the
	// state machine object


	// enable notifications from the packet characteristic
	enablePacketNotifications();

	// read the control point, used to verify the f/w image is indeed for the
	// target RCU device
	readControlPoint();

	// check if a window size descriptor exists, if so try and read it's value
	if (m_windowSizeDescriptor.isNull()) {

		// no characteristic so just use the default value and pretend we
		// actually read the value and go with the default
		m_windowSize = 5;
		m_setupFlags |= ReadWindowSize;

	} else {

		// read the packet size descriptor
		readWindowSize();
	}

	// emit the started signal
	emit upgradingChanged(true);
	emit progressChanged(m_progress);
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Request notifications to be enabled on the f/w packet characteristic. This
	is potentially an async operation; on success the EnabledNotifications flag
	will be set, on failure an EnableNotifyErrorEvent will be posted to
	the state-machine.

 */
void GattUpgradeService::enablePacketNotifications()
{
	// sanity checks
	if (Q_UNLIKELY(m_packetCharacteristic.isNull()))
		return;


	// lambda called if an error occurs enabling the notifications
	const std::function<void(const QString&, const QString&)> errorCallback =
		[this](const QString &errorName, const QString &errorMessage)
		{
			qError() << "failed enable notifications due to" << errorName << errorMessage;
			m_lastError = errorMessage;

			// something has gone titsup
			if (m_stateMachine.isRunning())
				m_stateMachine.postEvent(EnableNotifyErrorEvent);
		};

	// lambda called if notifications are successfully enabled
	const std::function<void()> successCallback =
		[this]()
		{
			// it's all good so update the flags
			setSetupFlag(EnabledNotifications);
		};


	// enable notifications on the characteristic and check for any immediate
	// success or failures
	Future<> result = m_packetCharacteristic->enableNotifications(true);
	if (!result.isValid() || result.isError()) {
		errorCallback(result.errorName(), result.errorMessage());
		return;
	} else if (result.isFinished()) {
		successCallback();
		return;
	}

	// connect functors to the future async completion
	result.connectErrored(this, errorCallback);
	result.connectFinished(this, successCallback);
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Requests a read on the control pointer characteristic.  This is an async
	operation, when it completes it will set the VerifiedDeviceModel bit in
	the m_setupFlags field. On failure it will post a

 */
void GattUpgradeService::readControlPoint()
{
	// sanity checks
	if (Q_UNLIKELY(m_controlCharacteristic.isNull()))
		return;


	// lambda called if an error occurs enabling the notifications
	const std::function<void(const QString&, const QString&)> errorCallback =
		[this](const QString &errorName, const QString &errorMessage)
		{
			qError() << "failed to read control point characteristic due to"
			         << errorName << errorMessage;
			m_lastError = errorMessage;

			// something has gone titsup
			if (m_stateMachine.isRunning())
				m_stateMachine.postEvent(ReadErrorEvent);
		};

	// lambda called if notifications are successifully enabled
	const std::function<void(const QByteArray &value)> successCallback =
		[this](const QByteArray &value)
		{
			// check the value size and actual value
			if (value.length() != sizeof(ControlPoint)) {
				qError("invalid length of OTA control point");
				m_lastError = "Invalid data length in OTA Control Point characteristic";

				if (m_stateMachine.isRunning())
					m_stateMachine.postEvent(ReadErrorEvent);
			}

			qInfo() << "OTA control point data" << arrayToHex(value);

			ControlPoint ctrlPoint;
			memcpy(&ctrlPoint, value.constData(), sizeof(ControlPoint));

			ctrlPoint.deviceModelId = qToLittleEndian(ctrlPoint.deviceModelId);
			const quint8 deviceManufId = quint8(ctrlPoint.deviceModelId >> 24);
			const quint8 deviceMajor   = quint8(ctrlPoint.deviceModelId >> 16);
			const quint8 deviceMinor   = quint8(ctrlPoint.deviceModelId >> 8);
			const quint8 deviceMicro   = quint8(ctrlPoint.deviceModelId >> 0);

			const QVersionNumber expectedHwVersion = m_fwFile->hwVersion();

			// currently neither vendor has correctly implemented the h/w id
			// header in their firmware image files, so for now we only log
			// the discrepancy rather than reject the f/w file outright
#if 1
			if (m_fwFile->manufacturerId() != deviceManufId) {
				qWarning("mismatched manuf id (f/w file 0x%02hhx, device 0x%02hhx)",
				         m_fwFile->manufacturerId(), deviceManufId);
			}

			if ((expectedHwVersion.majorVersion() != deviceMajor) ||
			    (expectedHwVersion.minorVersion() != deviceMinor) ||
			    (expectedHwVersion.microVersion() != deviceMicro)) {
				qWarning("mismatched h/w revisions (f/w file %s, device %hhu.%hhu.%hhu)",
				         qPrintable(expectedHwVersion.toString()),
				         deviceMajor, deviceMinor, deviceMicro);
			}

			if (true) {
#else
			if (m_fwFile->manufacturerId() != deviceManufId) {

				qWarning("mismatched manuf id (f/w file 0x%02hhx, device 0x%02hhx)",
				         m_fwFile->manufacturerId(), deviceManufId);
				m_lastError = QStringLiteral("Firmware file manufacturer id doesn't match device");

			} else if ((expectedHwVersion.majorVersion() != deviceMajor) ||
			           (expectedHwVersion.minorVersion() != deviceMinor) ||
			           (expectedHwVersion.microVersion() != deviceMicro)) {

				qWarning("mismatched h/w revisions (f/w file %s, device %hhu.%hhu.%hhu)",
				         qPrintable(expectedHwVersion.toString()),
				         deviceMajor, deviceMinor, deviceMicro);
				m_lastError = QStringLiteral("Firmware file h/w revision doesn't match device");

			} else {

				qInfo("h/w ident matches firmware file");
#endif

				// it's all good so update the flags
				setSetupFlag(VerifiedDeviceModel);
			}
		};



	// perform the read request
	Future<QByteArray> result = m_controlCharacteristic->readValue();
	if (!result.isValid() || result.isError()) {
		errorCallback(result.errorName(), result.errorMessage());
		return;
	} else if (result.isFinished()) {
		successCallback(result.result());
		return;
	}

	// connect functors to the future async completion
	result.connectErrored(this, errorCallback);
	result.connectFinished(this, successCallback);
}

// -----------------------------------------------------------------------------
/*!
	\internal


 */
void GattUpgradeService::readWindowSize()
{
	// sanity checks
	if (Q_UNLIKELY(m_windowSizeDescriptor.isNull()))
		return;


	// lambda called if an error occurs enabling the notifications
	const std::function<void(const QString&, const QString&)> errorCallback =
		[this](const QString &errorName, const QString &errorMessage)
		{
			qError() << "failed to read window size descriptor due to"
			         << errorName << errorMessage;
			m_lastError = errorMessage;

			// something has gone titsup
			if (m_stateMachine.isRunning())
				m_stateMachine.postEvent(ReadErrorEvent);
		};

	// lambda called if on successful read of the characteristic
	const std::function<void(const QByteArray&)> successCallback =
		[this](const QByteArray &value)
		{
			if (value.length() != 1) {
				qError("invalid length of window size data");
				m_lastError = "Invalid data length in OTA Packet Window Size descriptor";

			} else {

				m_windowSize = static_cast<uint>(value.at(0));
				if (m_windowSize <= 0) {
					qError("invalid window size value");
					m_lastError = "Invalid OTA Packet Window Size descriptor value";

				} else {
					qInfo("read window size of %u packets", m_windowSize);
					setSetupFlag(ReadWindowSize);
					return;

				}
			}

			// something has gone titsup
			if (m_stateMachine.isRunning())
				m_stateMachine.postEvent(ReadErrorEvent);
		};



	// perform the read request
	Future<QByteArray> result = m_windowSizeDescriptor->readValue();
	if (!result.isValid() || result.isError()) {
		errorCallback(result.errorName(), result.errorMessage());
		return;
	} else if (result.isFinished()) {
		successCallback(result.result());
		return;
	}

	// connect functors to the future async completion
	result.connectErrored(this, errorCallback);
	result.connectFinished(this, successCallback);
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Called when one of the setup phases is complete, when all flags are set then
 	we signal the state machine that we're ready to go.

 */
void GattUpgradeService::setSetupFlag(SetupFlag flag)
{
	if (Q_UNLIKELY(m_setupFlags & flag))
		qWarning("setup flag already set?");

	m_setupFlags |= flag;

	if (m_setupFlags.testFlag(EnabledNotifications) &&
		m_setupFlags.testFlag(ReadWindowSize) &&
	    m_setupFlags.testFlag(VerifiedDeviceModel)) {
		m_stateMachine.postEvent(FinishedSetupEvent);
	}
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Called on entry to the state were we send the first write request.

 */
void GattUpgradeService::onEnteredSendWriteRequestState()
{
	// reset the last ACK'ed block
	m_lastAckBlockId = -1;

	// send the initial write request
	sendWRQ();

	// reset the timeout counter
	m_timeoutCounter = 0;

	// write request has been sent, start the timeout timer as we expect an ACK
	// back pretty snappy
	m_timeoutTimer.start();
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Called on entry to the state were we send the first data packet. This is
	where we complete the start promise as we're finally up and transferring
	data to the RCU.

 */
void GattUpgradeService::onEnteredSendingDataState()
{
	if (Q_UNLIKELY(!m_startPromise)) {
		qError("start promise already completed?");
		return;
	}

	m_startPromise->setFinished();
	m_startPromise.reset();
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Called on entry to the errored state, this is just a transitional state
	used to emit an error() signal before moving on to the Finished state

 */
void GattUpgradeService::onEnteredErroredState()
{
	if (Q_UNLIKELY(m_lastError.isEmpty()))
		m_lastError = "Unknown error";

	// if there is still an outstanding promise it means we never completed
	// the initial phase, so release the promise now with an error
	if (m_startPromise) {
		m_startPromise->setError(BleRcuError::errorString(BleRcuError::General),
		                         m_lastError);
		m_startPromise.reset();

	} else {
		// no start promise so failed during data transfer, for this we should
		// emit an error signal
		emit error(m_lastError);
	}

	m_lastError.clear();

	// move to the finished state
	m_stateMachine.postEvent(CompleteEvent);
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Called on entry to the Finished state, here we just clean up any resources
	that may be left open.

 */
void GattUpgradeService::onEnteredFinishedState()
{
	// stop the timeout timer if running
	m_timeoutTimer.stop();

	// get the number of blocks in the f/w file
	const int fwBlockCount =
		static_cast<int>((m_fwFile->size() + (FIRMWARE_PACKET_MTU - 1)) / FIRMWARE_PACKET_MTU);

	// close / release the firmware file
	m_fwFile.reset();

	// disable notifications from the packet characteristic (we don't care
	// about the result of the operation FIXME).
	if (m_packetCharacteristic)
		m_packetCharacteristic->enableNotifications(false);

	// it's possible we never completed the start promise as the user could
	// have cancelled, if this happens we complete with an error
	if (m_startPromise) {
		m_startPromise->setError(BleRcuError::errorString(BleRcuError::Rejected),
		                         QStringLiteral("Upgrade cancelled"));
		m_startPromise.reset();
	}

	// emit the upgrade complete signal if we managed to send some data to
	// the RCU ... this is a workaround for an issue with the UEI RCU where
	// they don't ack the final block of data and therefore we think the
	// transfer has failed but it probably succeeded
	if ((fwBlockCount > m_windowSize) &&
	    (m_lastAckBlockId >= (fwBlockCount - m_windowSize))) {
		emit upgradeComplete();
	}

	// emit the finished signal
	emit upgradingChanged(false);

	// reset the progress and emit a progress changed signal
	m_progress = -1;
	// emit progressChanged(m_progress);
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Requests a write of \a value to the packet characteristic.

 */
void GattUpgradeService::doPacketWrite(const QByteArray &value)
{
	// sanity check
	if (Q_UNLIKELY(m_packetCharacteristic.isNull()))
		return;

	// lambda called if an error occurs writing to the packet characteristic,
	// just use to log an error message, the timeout(s) will handle the retry
	const std::function<void(const QString&, const QString&)> errorCallback =
		[this](const QString &errorName, const QString &errorMessage)
		{
			qError() << "failed to write to OTA packet char due to"
			         << errorName << errorMessage;
		};


	// send the write without response request
	Future<> result = m_packetCharacteristic->writeValueWithoutResponse(value);
	if (!result.isValid() || result.isError())
		errorCallback(result.errorName(), result.errorMessage());
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Sends a WRQ packet to the remote BLE device over the packet gatt
 	characteristic.

 */
void GattUpgradeService::sendWRQ()
{
	// construct the WRQ packet
	struct Q_PACKED {
		quint8 opCode;
		quint8 reserved;
		quint32 length;
		quint32 version;
		quint32 crc32;
	} writePacket;

	QVersionNumber version = m_fwFile->version();
	quint32 fwVersion = quint32(version.majorVersion() & 0xffff) << 16
	                  | quint32(version.minorVersion() & 0xff)   << 8
	                  | quint32(version.microVersion() & 0xff)   << 0;

	writePacket.opCode = OPCODE_WRQ;
	writePacket.reserved = 0x00;
	writePacket.length = qToLittleEndian<quint32>(static_cast<quint32>(m_fwFile->size()));
	writePacket.version = qToLittleEndian<quint32>(fwVersion);
	writePacket.crc32 = qToLittleEndian<quint32>(m_fwFile->crc32());

	qDebug("sending WRQ packet (length:0x%08x version:0x%08x crc32:0x%08x)",
	       writePacket.length, writePacket.version, writePacket.crc32);

	QByteArray value(reinterpret_cast<const char*>(&writePacket), sizeof(writePacket));


	// send the WRQ packet
	doPacketWrite(value);
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Sends the next window of firmware upgrade packets.

	If any packets were queued but not written to the pipe then they are
	discarded before new packets are pushed into the q

 */
void GattUpgradeService::sendDATA()
{
	//
	quint16 blockId = static_cast<quint16>(m_lastAckBlockId + 1);

	// (re)seek to the right block
	if (!m_fwFile->seek((blockId - 1) * FIRMWARE_PACKET_MTU)) {
		qWarning("failed to seek to location of block %hu", blockId);

		m_lastError = "Failed seeking to correct place in firmware file";
		m_stateMachine.postEvent(WriteErrorEvent);
		return;
	}

	// buffer for storing DATA packets
	struct Q_PACKED {
		quint8 header[2];
		quint8 body[18];
	} packet;

	// fire off a bunch of DATA packets for the next window
	for (int i = 0; i < m_windowSize; i++) {

		// read up to 18 bytes of data
		qint64 rd = m_fwFile->read(packet.body, FIRMWARE_PACKET_MTU);
		if ((rd != FIRMWARE_PACKET_MTU) && !m_fwFile->atEnd())
			qWarning("read too few bytes but not at end of file?");

		// set the block id
		packet.header[0] = OPCODE_DATA | quint8((blockId >> 8) & 0x3f);
		packet.header[1] = quint8(blockId & 0xff);

		// send / queue the data packet
		QByteArray value(reinterpret_cast<const char*>(&packet),
		                 static_cast<int>(2 + rd));

		// do the packet write
		doPacketWrite(value);

		// increment the block number
		blockId++;

		// break out if we've sent a packet smaller than 18 bytes
		if (rd < FIRMWARE_PACKET_MTU)
			break;
	}
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Called when the timeout timer expires, this is enable when either a WRQ or
	DATA packet is written and we expect a reply.

 */
void GattUpgradeService::onTimeout()
{
	// sanity check the state machine is running
	if (!m_stateMachine.isRunning())
		return;

	qDebug("f/w upgrade timed-out in state %d", m_stateMachine.state());

	// check if we've timed-out to many times
	if (m_timeoutCounter++ > 3) {
		qWarning("timeout counter exceeded in state %d", m_stateMachine.state());

		// simply inject the timeout event into the state machine if running
		m_lastError = QStringLiteral("Timed-out");
		m_stateMachine.postEvent(TimeoutErrorEvent);
		return;
	}

	// (re)start the timer
	m_timeoutTimer.start();

	// re-send the data based on current state
	if (m_stateMachine.inState(SendingWriteRequestState)) {

		// send the initial write request
		sendWRQ();

	} else if (m_stateMachine.inState(SendingDataState)) {

		// send the next window of DATA packets
		sendDATA();

	}
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Called when we receive a notification packet from the PACKET characteristic.
	The packet will either contain an ACK or ERROR packet.

 */
void GattUpgradeService::onPacketNotification(const QByteArray &value)
{
	qDebug() << "received packet notification" << arrayToHex(value);

	// every notification should be 2 bytes in size
	if (Q_UNLIKELY(value.length() != 2)) {
		qWarning("length of notification packet is not 2 bytes (actual %d)",
		         value.length());
		return;
	}

	// get a pointer to the raw notification
	const quint8 *data = reinterpret_cast<const quint8*>(value.constData());

	// got a two byte packet, check the opcode (warning the following code
	// can trigger a state machine transition)
	switch (data[0] & OPCODE_MASK) {
		case OPCODE_ACK:
			onACKPacket(data);
			break;
		case OPCODE_ERROR:
			onERRORPacket(data);
			break;
		default:
			qWarning("unexpected notification opcode 0x%02hhx",
			         quint8(data[0] & OPCODE_MASK));
			break;
	}

}

// -----------------------------------------------------------------------------
/*!
	\internal

	Called when an ACK packet has been received.

 */
void GattUpgradeService::onACKPacket(const quint8 data[2])
{
	// get the block id of the ack
	int blockId = (quint16(data[0] & 0x3f) << 8)
	            | (quint16(data[1] & 0xff) << 0);

	qDebug("received ACK %u", blockId);

	// if not in the sending super state ignore the ACK
	if (Q_UNLIKELY(!m_stateMachine.isRunning() ||
	               !m_stateMachine.inState(SendingSuperState))) {
		qInfo("received ACK %u in wrong state", blockId);
		return;
	}

	// reset the timeout counter
	m_timeoutCounter = 0;

	//
	const int fwDataSize = static_cast<int>(m_fwFile->size());

	// check if the ACK is for the last block
	if ((blockId * FIRMWARE_PACKET_MTU) > fwDataSize) {

		// stop the timeout
		m_timeoutTimer.stop();

		// set progress at 100% and emit the final progress change
		m_progress = 100;
		emit progressChanged(m_progress);

		// emit an upgrade complete signal (used to notify other services,
		// notably device info, that an upgrade has taken place)
		emit upgradeComplete();

		// post a complete event
		m_stateMachine.postEvent(CompleteEvent);

	} else if (blockId > m_lastAckBlockId) {

		// update the confirmation of the last block ACKed
		m_lastAckBlockId = blockId;

		// if this is the first ack then post a message to the state machine
		// so we move into the sending data state
		if (blockId == 0)
			m_stateMachine.postEvent(PacketAckEvent);

		// emit a signal for the progress update
		int progress = (blockId * FIRMWARE_PACKET_MTU * 100) / fwDataSize;
		if (progress != m_progress) {
			m_progress = progress;
			emit progressChanged(progress);
		}

		// send the next window of packets
		sendDATA();

		// (re)start the timeout timer
		m_timeoutTimer.start();
	}
}

// -----------------------------------------------------------------------------
/*!
	\internal


 */
void GattUpgradeService::onERRORPacket(const quint8 data[2])
{
	// sanity check the bits that should be zero are zero
	if (Q_UNLIKELY(data[0] != OPCODE_ERROR))
		qWarning("malformed error packet");

	qWarning("received ERROR 0x%02hhx", data[1]);

	// if not in the sending super state ignore the ERROR
	if (Q_UNLIKELY(!m_stateMachine.isRunning() ||
	               !m_stateMachine.inState(SendingSuperState)))
		return;

	// set the error string based on the code
	switch (data[1]) {
		case 0x01:
			m_lastError = QStringLiteral("CRC mismatch error from RCU");
			break;
		case 0x02:
			m_lastError = QStringLiteral("Invalid size error from RCU");
			break;
		case 0x03:
			m_lastError = QStringLiteral("Size mismatch error from RCU");
			break;
		case 0x04:
			m_lastError = QStringLiteral("Battery too low");
			break;
		case 0x05:
			m_lastError = QStringLiteral("Invalid opcode error from RCU");
			break;
		case 0x06:
			m_lastError = QStringLiteral("Internal error from RCU");
			break;
		case 0x07:
			m_lastError = QStringLiteral("Invalid hash error from RCU");
			break;
		default:
			m_lastError = QString_asprintf("Received unknown error (0x%02hhx) from RCU",
			                                data[1]);
			break;
	}

	//
	m_stateMachine.postEvent(PacketErrorEvent);
}
