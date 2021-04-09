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
//  blercucontroller.cpp
//  SkyBluetoothRcu
//

#include "blercucontroller_p.h"
#include "blercuanalytics.h"
#include "blercuadapter.h"
#include "blercudevice.h"
#include "blercuerror.h"

#include "utils/logging.h"
#include "configsettings/configsettings.h"
#include "utils/inputdevicemanager.h"

#include <QCoreApplication>
#include <QTimer>

BleRcuControllerImpl::BleRcuControllerImpl(const QSharedPointer<const ConfigSettings> &config,
                                           const QSharedPointer<BleRcuAdapter> &adapter,
                                           QObject *parent)
	: BleRcuController(parent)
	, m_config(config)
	, m_adapter(adapter)
	, m_analytics(QSharedPointer<BleRcuAnalytics>::create(config))
	, m_pairingStateMachine(config, m_adapter)
	, m_scannerStateMachine(config, m_adapter)
	, m_lastError(BleRcuError::NoError)
	, m_maxManagedDevices(1)
	, m_state(Initialising)
{

	// state is sent in signals so register it
	qRegisterMetaType<BleRcuController::State>("BleRcuController::State");


	// build a set of IR pairing filter bytes that are supported according to
	// the json config file
	const QList<ConfigModelSettings> modelSettings = m_config->modelSettings();
	for (const ConfigModelSettings &modelSetting : modelSettings) {
		if (!modelSetting.disabled())
			m_supportedFilterBytes += modelSetting.irFilterBytes();
	}


	// connect to the finished signal of the pairing statemachine, use to update
	// our list of managed devices
	QObject::connect(&m_pairingStateMachine, &BleRcuPairingStateMachine::finished,
	                 this, &BleRcuControllerImpl::onFinishedPairing,
	                 Qt::QueuedConnection);

	// connect to the started signal so we can send pairing state notifications
	QObject::connect(&m_pairingStateMachine, &BleRcuPairingStateMachine::started,
	                 this, &BleRcuControllerImpl::onStartedPairing,
	                 Qt::QueuedConnection);

	// connect to the failed signal so we can send pairing state notifications
	QObject::connect(&m_pairingStateMachine, &BleRcuPairingStateMachine::failed,
	                 this, &BleRcuControllerImpl::onFailedPairing,
	                 Qt::QueuedConnection);

	// connect to the manager's device pairing change signals
	QObject::connect(adapter.data(), &BleRcuAdapter::devicePairingChanged,
	                 this, &BleRcuControllerImpl::onDevicePairingChanged,
	                 Qt::QueuedConnection);

	// connect to the manager's device ready signals
	QObject::connect(adapter.data(), &BleRcuAdapter::deviceReadyChanged,
	                 this, &BleRcuControllerImpl::onDeviceReadyChanged,
	                 Qt::QueuedConnection);

	// connect to the manager's initialised signal
	QObject::connect(m_adapter.data(), &BleRcuAdapter::poweredInitialised,
	                 this, &BleRcuControllerImpl::onInitialised,
	                 Qt::QueuedConnection);

	// NGDEV-146407: check if already powered and if so signal the initialised state
	if (m_adapter->isPowered()) {
#if QT_VERSION >= QT_VERSION_CHECK(5, 4, 0)
		QTimer::singleShot(1000, this, &BleRcuControllerImpl::onInitialised);
#else
		QTimer* timer = new QTimer(this);
		QObject::connect(timer, &QTimer::timeout, this, &BleRcuControllerImpl::onInitialised);
		QObject::connect(timer, &QTimer::timeout, timer, &QObject::deleteLater);
		timer->setSingleShot(true);
		timer->start(1000);
#endif
	}

	// connect the analytics to the signals we generate (does this make more
	// sense store the object outside this class?)
	QObject::connect(this, &BleRcuController::managedDeviceAdded,
	                 m_analytics.data(), &BleRcuAnalytics::logDeviceAdded,
	                 Qt::QueuedConnection);
	QObject::connect(this, &BleRcuController::managedDeviceRemoved,
	                 m_analytics.data(), &BleRcuAnalytics::logDeviceRemoved,
	                 Qt::QueuedConnection);
	QObject::connect(this, &BleRcuController::pairingStateChanged,
	                 m_analytics.data(), &BleRcuAnalytics::logPairingStateChange,
	                 Qt::QueuedConnection);


	// connect to the scanner signals
	QObject::connect(&m_scannerStateMachine, &BleRcuScannerStateMachine::started,
	                 this, &BleRcuControllerImpl::onStartedScanning,
	                 Qt::QueuedConnection);
	QObject::connect(&m_scannerStateMachine, &BleRcuScannerStateMachine::finished,
	                 this, &BleRcuControllerImpl::onFinishedScanning,
	                 Qt::QueuedConnection);
	QObject::connect(&m_scannerStateMachine, &BleRcuScannerStateMachine::failed,
	                 this, &BleRcuControllerImpl::onFailedScanning,
	                 Qt::QueuedConnection);

	// connect to the signal emitted when the scanner found an RCU device in
	// pairing mode
	QObject::connect(&m_scannerStateMachine, &BleRcuScannerStateMachine::foundPairableDevice,
	                 this, &BleRcuControllerImpl::onFoundPairableDevice,
	                 Qt::QueuedConnection);


	// schedule the controller to synchronise the list of managed devices at
	// start-up in the next idle time of the event loop
#if QT_VERSION >= QT_VERSION_CHECK(5, 4, 0)
	QTimer::singleShot(0, this, &BleRcuControllerImpl::syncManagedDevices);
#else
	{
		QTimer* timerX = new QTimer(this);
		QObject::connect(timerX, &QTimer::timeout, this, &BleRcuControllerImpl::syncManagedDevices);
		QObject::connect(timerX, &QTimer::timeout, timerX, &QObject::deleteLater);
		timerX->setSingleShot(true);
		timerX->start(0);
	}
#endif

}

BleRcuControllerImpl::~BleRcuControllerImpl()
{
	qInfo("BleRcuController shut down");
}


bool BleRcuControllerImpl::isValid() const
{
	return true;
}

// -----------------------------------------------------------------------------
/*!
	\fn void BleRcuController::state() const

 */
BleRcuController::State BleRcuControllerImpl::state() const
{
	return m_state;
}

// -----------------------------------------------------------------------------
/*!
	\fn void BleRcuController::dump(Dumper out) const

	Currently this is only used on Android when the dumpsys app is run, it
	simply prints out the state of the controller to the supplied Dumper object.

 */
void BleRcuControllerImpl::dump(Dumper out) const
{
	out.printLine("SkyBluetoothRcu Service");
	out.pushIndent(2);
	out.printLine("version: %s", qPrintable(qApp->applicationVersion()));
	out.printLine("enabled: true");
	out.popIndent();
	out.printNewline();

	// dump out the adapter details
	out.printLine("Adapter:");
	out.pushIndent(2);
	m_adapter->dump(out);
	out.popIndent();
	out.printNewline();

	// dump out the managed devices
	out.printLine("Managed devices:");
	out.pushIndent(2);
	for (const BleAddress &bdaddr : m_managedDevices) {
		out.printLine("%s", qPrintable(bdaddr.toString()));
		out.pushIndent(2);

		const QSharedPointer<BleRcuDevice> device = m_adapter->getDevice(bdaddr);
		if (!device || !device->isValid())
			out.printLine("Invalid");
		else
			device->dump(out);

		out.popIndent();
	}
	out.popIndent();

	// dump out the pairing status
	out.printNewline();
	m_pairingStateMachine.dump(out);

	// dump out the scanner status
	out.printNewline();
	m_scannerStateMachine.dump(out);
}

// -----------------------------------------------------------------------------
/*!
	\fn BleRcuError BleRcuController::lastError() const

	Returns the last error that occured when performing a pairing function.

 */
BleRcuError BleRcuControllerImpl::lastError() const
{
	return m_lastError;
}

// -----------------------------------------------------------------------------
/*!
	\fn bool BleRcuController::isPairing() const

	Returns \c true if pairing is currently in progress.

	\see startPairing()
 */
bool BleRcuControllerImpl::isPairing() const
{
	return m_pairingStateMachine.isRunning();
}

// -----------------------------------------------------------------------------
/*!
	\fn int BleRcuController::pairingCode()

	Returns the current or last 8-bit pairing code used.

	If startPairing() has never been called \c -1 will be returned. Or if
	pairing was started after a scan then \c -1 will also be returned.

	\see startPairing()
 */
int BleRcuControllerImpl::pairingCode() const
{
	return m_pairingStateMachine.pairingCode();
}

// -----------------------------------------------------------------------------
/*!
	\fn bool BleRcuController::startPairing(quint8 pairingCode)

	Attempts to start the pairing procedure looking for devices that identify
	with the given \a filterByte and \a pairingCode.  Both these byte values
	are sent in the IR pairing signal and are used to help identify the RCU
	model and unique name.

	If the controller is currently in pairing mode this method will fail and
	return \c false.  If the bluetooth adaptor is not available or not powered
	then this function will also fail and return \c false.

	If \c false is returned use BleRcuController::lastError() to get the failure
	reason.

	\note This object doesn't actually run the pairing procedure, instead it
	just starts and stops the \l{BleRcuPairingStateMachine} object.

	\see cancelPairing(), isPairing() & pairingCode()
 */
bool BleRcuControllerImpl::startPairing(quint8 filterByte, quint8 pairingCode)
{
	// if currently scanning then we have to cancel that first before processing
	// the IR pairing request (nb - pairing request can only come to this
	// function from an IR event)
	if (m_scannerStateMachine.isRunning()) {
		m_scannerStateMachine.stop();

		qWarning("received IR pairing request in scanning mode, disabling"
		         " scanner and when stopped will start IR pairing");
		return false;
	}

	// use the filter byte to check if the current RCU model is supported
	if ((filterByte != 0x00) && !m_supportedFilterBytes.contains(filterByte)) {
		qDebug("IR filter byte from RCU not supported");
		m_lastError = BleRcuError(BleRcuError::Rejected,
		                          QStringLiteral("Unsupported filter byte value"));
		return false;
	}

	if (m_pairingStateMachine.isRunning()) {
		qDebug("requested pairing in already pairing state, ignoring request");
		m_lastError = BleRcuError(BleRcuError::Busy,
		                          QStringLiteral("Already in pairing state"));
		return false;
	}

	// check that the manager has powered on the adapter, without this we
	// obviously can't scan / pair / etc. The only time the adaptor should
	// (legitimately) be unavailable is right at start-up
	if (!m_adapter->isAvailable() || !m_adapter->isPowered()) {
		m_lastError = BleRcuError(BleRcuError::General,
		                          QStringLiteral("Adaptor not available or not powered"));
		return false;
	}

	// start the pairing process
	m_pairingStateMachine.start(filterByte, pairingCode);
	return true;
}

// -----------------------------------------------------------------------------
/*!
	\fn bool BleRcuController::startPairingMacHash(quint8 macHash)

	Attempts to start the pairing procedure looking for a device that has
	a MAC address matching the supplied MAC hash.
	
	The MAC hash is calculated by adding all the bytes of the MAC address
	together, and masking with 0xFF.
	e.g., MAC = AA:BB:CC:DD:EE:FF, hash = (AA+BB+CC+DD+EE+FF) & 0xFF

	If the controller is currently in pairing mode this method will fail and
	return \c false.  If the bluetooth adaptor is not available or not powered
	then this function will also fail and return \c false.

	If \c false is returned use BleRcuController::lastError() to get the failure
	reason.

	\note This object doesn't actually run the pairing procedure, instead it
	just starts and stops the \l{BleRcuPairingStateMachine} object.

	\see cancelPairing(), isPairing() & pairingCode()
 */
bool BleRcuControllerImpl::startPairingMacHash(quint8 filterByte, quint8 macHash)
{
	// if currently scanning then we have to cancel that first before processing
	// the IR pairing request (nb - pairing request can only come to this
	// function from an IR event)
	if (m_scannerStateMachine.isRunning()) {
		m_scannerStateMachine.stop();

		qWarning("received IR pairing request in scanning mode, disabling"
		         " scanner and when stopped will start IR pairing");
		return false;
	}

	// use the filter byte to check if the current RCU model is supported
	if ((filterByte != 0x00) && !m_supportedFilterBytes.contains(filterByte)) {
		qDebug("IR filter byte from RCU not supported");
		m_lastError = BleRcuError(BleRcuError::Rejected,
		                          QStringLiteral("Unsupported filter byte value"));
		return false;
	}

	if (m_pairingStateMachine.isRunning()) {
		qDebug("requested pairing in already pairing state, ignoring request");
		m_lastError = BleRcuError(BleRcuError::Busy,
		                          QStringLiteral("Already in pairing state"));
		return false;
	}

	// check that the manager has powered on the adapter, without this we
	// obviously can't scan / pair / etc. The only time the adaptor should
	// (legitimately) be unavailable is right at start-up
	if (!m_adapter->isAvailable() || !m_adapter->isPowered()) {
		m_lastError = BleRcuError(BleRcuError::General,
		                          QStringLiteral("Adaptor not available or not powered"));
		return false;
	}

	// start the pairing process
	m_pairingStateMachine.startMacHash(filterByte, macHash);
	return true;
}

// -----------------------------------------------------------------------------
/*!
	\fn void BleRcuController::cancelPairing()

	Cancels the pairing procedure if running.

	\see startPairing(), isPairing() & pairingCode()
 */
bool BleRcuControllerImpl::cancelPairing()
{
	if (!m_pairingStateMachine.isRunning())
		return false;

	m_pairingStateMachine.stop();
	return true;
}

// -----------------------------------------------------------------------------
/*!
	\fn bool BleRcuController::isScanning() const

	Returns \c true if scanning is currently in progress.

	\see startScanning()
 */
bool BleRcuControllerImpl::isScanning() const
{
	return m_scannerStateMachine.isRunning();
}

// -----------------------------------------------------------------------------
/*!
	\fn void BleRcuController::startScanning()

	Starts the scanner looking for RCUs in pairing mode.  The scan will run for
	\a timeoutMs milliseconds, or until cancelled if less than zero.

	The scanner won't start if the pairing state machine is already running.

	\see cancelScanning() & isScanning()
 */
bool BleRcuControllerImpl::startScanning(int timeoutMs)
{
	// check we're not currently pairing
	if (m_pairingStateMachine.isRunning()) {
		qWarning("currently performing pairing, cannot start new scan");
		return false;
	}

	// check we're not already scanning
	if (m_scannerStateMachine.isRunning()) {
		qWarning("already scanning, new scan request aborted");
		return false;
	}

	// check that the manager has powered on the adapter, without this we
	// obviously can't scan. The only time the adaptor should (legitimately) be
	// unavailable is right at start-up
	if (!m_adapter->isAvailable() || !m_adapter->isPowered()) {
		m_lastError = BleRcuError(BleRcuError::General,
		                          QStringLiteral("Adaptor not available or not powered"));
		return false;
	}

	// start the scanning process
	if (m_state != Searching) {
		m_state = Searching;
		emit stateChanged(m_state);
	}

	m_scannerStateMachine.start(timeoutMs);
	return true;
}

// -----------------------------------------------------------------------------
/*!
	\fn void BleRcuController::cancelScanning()

	Cancels the current scanning process.

	\see startScanning()
 */
bool BleRcuControllerImpl::cancelScanning()
{
	if (!m_scannerStateMachine.isRunning())
		return false;

	m_scannerStateMachine.stop();
	return true;
}

// -----------------------------------------------------------------------------
/*!
	\fn QSet<BleAddress> BleRcuController::managedDevices() const

	Returns a set of all the RCU devices currently been managed.

	\see managedDevice()
 */
QSet<BleAddress> BleRcuControllerImpl::managedDevices() const
{
	return m_managedDevices;
}

// -----------------------------------------------------------------------------
/*!
	\fn QSharedPointer<BleRcuDevice> BleRcuController::managedDevice(const BleAddress &address) const

	Returns a shared pointer to the managed RCU device with the given \a address.
	If there is no managed RCU with the given address an empty shared pointer
	is returned.

	\see managedDevices()
 */
QSharedPointer<BleRcuDevice> BleRcuControllerImpl::managedDevice(const BleAddress &address) const
{
	if (Q_UNLIKELY(!m_managedDevices.contains(address)))
		return QSharedPointer<BleRcuDevice>();

	return m_adapter->getDevice(address);
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Called in the following scenarios:
		- At start-up time
		- A device has been added in non-pairing state
		- A device has been removed in non-pairing state
		- On pairing finished (success or failure)

	It gets a list of managed devices from the \l{BleRcuAdapter} object, if
	there are devices in the return list that are paired but not in our local
	managed set then they are added to the set and a signal emitted.  Conversely
	if there is a device in our managed set that doesn't match a paired device
	in the adapter's list then it is removed from the set and 'removed' signal
	is emitted.

 */
void BleRcuControllerImpl::syncManagedDevices()
{
	// get the set of currently paired devices
	const QSet<BleAddress> paired = m_adapter->pairedDevices();

	// calculate the set of removed devices first (if any)
	QSet<BleAddress> removed;
	for (const BleAddress &address : m_managedDevices) {

		if (!paired.contains(address))
			removed.insert(address);
	}

	qDebug() << "removed" << removed;
	m_managedDevices -= removed;
	for (const BleAddress &address : removed)
		emit managedDeviceRemoved(address);


	// calculate the set of added devices next (if any)
	QSet<BleAddress> added;
	for (const BleAddress &address : paired) {

		if (!m_managedDevices.contains(address))
			added.insert(address);
	}

	qDebug() << "added" << added;
	m_managedDevices += added;
	for (const BleAddress &address : added)
		emit managedDeviceAdded(address);


	// next check if the number of paired / managed device exceeds the
	// maximum allowed, if so we need to remove the device that was connected
	// the longest ago
	if (m_managedDevices.count() > m_maxManagedDevices) {

		// the following will push the call onto the event queue, didn't
		// want to do this option in the callback slot (although shouldn't
		// be an issue)
#if QT_VERSION >= QT_VERSION_CHECK(5, 4, 0)
		QTimer::singleShot(0, this, &BleRcuControllerImpl::removeLastConnectedDevice);
#else

		QTimer* timer = new QTimer(this);
		QObject::connect(timer, &QTimer::timeout, this, &BleRcuControllerImpl::removeLastConnectedDevice);
		QObject::connect(timer, &QTimer::timeout, timer, &QObject::deleteLater);
		timer->setSingleShot(true);
		timer->start(0);
#endif
	}
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Queued function call that iterates through the set of currently managed
	devices and removes the one(s) that were the last to go to the 'ready' state.

	This function should only be called after we detect that more than the
	maximum number of devices have entered the ready state, i.e. we have more
	than the maximum actively connected.

	\sa removeDevice(), onDeviceReadyChanged()
 */
void BleRcuControllerImpl::removeLastConnectedDevice()
{
	// because this is a queued callback we need to check once again the we
	// have exceeded the maximum number of devices, here we create an ordered
	// list of all the 'paired' devices, the oldest ones to enter the ready
	// state are at the front

	QList<QSharedPointer<const BleRcuDevice>> pairedDevices;
	for (const BleAddress &bdaddr : m_managedDevices) {

		QSharedPointer<BleRcuDevice> device = m_adapter->getDevice(bdaddr);
		if (device && device->isValid() && device->isPaired()) {

			// find the spot in the list to insert the item, devices with the
			// oldest ready transition at the front
			QList<QSharedPointer<const BleRcuDevice>>::iterator it = pairedDevices.begin();
			for (; it != pairedDevices.end(); ++it) {
				if ((*it)->msecsSinceReady() < device->msecsSinceReady())
					break;
			}

			pairedDevices.insert(it, device);
		}
	}

	// remove the first n number of paired devices
	while (pairedDevices.length() > m_maxManagedDevices) {

		// take the first device from the queue
		const QSharedPointer<const BleRcuDevice> device = pairedDevices.takeFirst();

		qMilestone() << "unpairing" << device->address()
		             << "because exceeded maximum number of managed devices";

		// ask bluez to remove it, this will disconnect and unpair the device
		m_adapter->removeDevice(device->address());
	}
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Queued slot called when the pairing state machine has started.
 */
void BleRcuControllerImpl::onStartedPairing()
{
	// a queued event so check the state
	const bool pairing = m_pairingStateMachine.isRunning();

	// tell clients that the pairing state has changed
	emit pairingStateChanged(pairing);

	m_state = Pairing;
	emit stateChanged(Pairing);
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Queued slot called when the pairing state machine has finished.  This
	doesn't necessarily mean it succeeded, this is called on failure as well.

 */
void BleRcuControllerImpl::onFinishedPairing()
{
	// a queued event so check the state
	const bool pairing = m_pairingStateMachine.isRunning();

	// (re)sync our list of managed devices now pairing has finished
	if (!pairing)
		syncManagedDevices();

	// tell clients that the pairing state is changed
	emit pairingStateChanged(pairing);

	m_state = Complete;
	emit stateChanged(Complete);
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Queued slot called when the pairing state machine has failed. This doesn't
	necessarily mean it succeeded, this is called on failure as well.

 */
void BleRcuControllerImpl::onFailedPairing()
{
	// a queued event so check the state
	const bool pairing = m_pairingStateMachine.isRunning();

	// (re)sync our list of managed devices now pairing has finished
	if (!pairing)
		syncManagedDevices();

	// tell clients that the pairing state is changed
	emit pairingStateChanged(pairing);

	m_state = Failed;
	emit stateChanged(Failed);
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Queued slot called when the adaptor is powered on 
 */
void BleRcuControllerImpl::onInitialised()
{
	if (m_state == Initialising) {
		m_state = Idle;
		emit stateChanged(Idle);
	}
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Queued slot called when the \l{BleRcuManager} has added a new device.
 */
void BleRcuControllerImpl::onDevicePairingChanged(const BleAddress &address,
                                                  bool paired)
{
	if (!paired) {
		// if the removed device is in our managed set then we remove it
		// immediately even if the pairing state machine is running.
		// Previously we didn't do this while in pairing mode, that meant if you
		// repaired the same device the client never got the removed / added
		// notifications.  So although this is not technically wrong, some
		// clients were expecting an added notification to indicate pairing
		// succeeded, when in fact all they got was an unchanged list of devices.
		if (m_managedDevices.contains(address)) {
			m_managedDevices.remove(address);
			emit managedDeviceRemoved(address);
		}
	}

	// a device has bonded / unbonded, so if the state-machine is not running
	// then re-sync the list of devices we are managing
	if (!m_pairingStateMachine.isRunning())
		syncManagedDevices();
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Event signalled by a \l{BleRcuDevice} object when it's ready state has
	changed. The 'ready' state implies that the device is paired, connected, and
	has gone through the initial setup such that it is now ready.

	This is also the point were we check the number of devices we have in the
	'paired' state, if it exceeds the maximum allowed then we remove the last
	device to enter the 'ready' state.

 */
void BleRcuControllerImpl::onDeviceReadyChanged(const BleAddress &address,
                                                bool ready)
{
	Q_UNUSED(address);

	if (ready && !m_pairingStateMachine.isRunning())
		syncManagedDevices();
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Queued slot called when the scanner state machine indicates it has started.
 */
void BleRcuControllerImpl::onStartedScanning()
{
	emit scanningStateChanged(true);
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Queued slot called when the scanner state machine indicates it has stopped.
	This may be because it was cancelled, found a target device or timed out.
 */
void BleRcuControllerImpl::onFinishedScanning()
{
	emit scanningStateChanged(false);
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Queued slot called when the scanner state machine indicates it has failed to
	find a device from scanning. This may be because it was cancelled, found a
	target device or timed out.
 */
void
BleRcuControllerImpl::onFailedScanning()
{
	m_state = Failed;
	emit stateChanged(Failed);
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Queued slot called when the scanner state machine found an RCU that was in
	'pairing' mode.

	This triggers us to start the pairing state machine targeting the device
	with the given address.

 */
void BleRcuControllerImpl::onFoundPairableDevice(const BleAddress &address,
                                                 const QString &name)
{
	qInfo() << "found" << address << "RCU device in pairing mode,"
	        << "kicking off the pairing state machine";

	// sanity check (needed?)
	if (m_pairingStateMachine.isRunning()) {
		qWarning("found target device in scan but pairing state machine "
		         "already running?");
		return;
	}

	// start pairing the device
	m_pairingStateMachine.start(address, name);
}
