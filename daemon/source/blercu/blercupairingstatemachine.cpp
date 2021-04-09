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
//  blercupairingstatemachine.cpp
//  SkyBluetoothRcu
//

#include "blercupairingstatemachine.h"
#include "blercuadapter.h"

#include "configsettings/configsettings.h"
#include "utils/logging.h"

#include <QEvent>



BleRcuPairingStateMachine::BleRcuPairingStateMachine(const QSharedPointer<const ConfigSettings> &config,
                                                     const QSharedPointer<BleRcuAdapter> &adapter,
                                                     QObject *parent)
	: QObject(parent)
	, m_adapter(adapter)
	, m_pairingCode(-1)
	, m_pairingMacHash(-1)
	, m_pairingAttempts(0)
	, m_pairingSuccesses(0)
	, m_pairingSucceeded(false)
{

	// constructs a map of name printf style formats for searching for device
	// names that match
	const QList<ConfigModelSettings> models = config->modelSettings();
	for (const ConfigModelSettings &model : models) {
		if (!model.disabled())
			m_pairingPrefixFormats[model.oui()] = model.pairingNameFormat();
	}


	// setup (but don't start) the state machine
	setupStateMachine();

	// connect up the events from the manager
	QObject::connect(m_adapter.data(), &BleRcuAdapter::discoveryChanged,
	                 this, &BleRcuPairingStateMachine::onDiscoveryChanged);
	QObject::connect(m_adapter.data(), &BleRcuAdapter::pairableChanged,
	                 this, &BleRcuPairingStateMachine::onPairableChanged);

	QObject::connect(m_adapter.data(), &BleRcuAdapter::deviceFound,
	                 this, &BleRcuPairingStateMachine::onDeviceFound);
	QObject::connect(m_adapter.data(), &BleRcuAdapter::deviceRemoved,
	                 this, &BleRcuPairingStateMachine::onDeviceRemoved);
	QObject::connect(m_adapter.data(), &BleRcuAdapter::deviceNameChanged,
	                 this, &BleRcuPairingStateMachine::onDeviceNameChanged);
	QObject::connect(m_adapter.data(), &BleRcuAdapter::deviceReadyChanged,
	                 this, &BleRcuPairingStateMachine::onDeviceReadyChanged);
	QObject::connect(m_adapter.data(), &BleRcuAdapter::devicePairingChanged,
	                 this, &BleRcuPairingStateMachine::onDevicePairingChanged);

	QObject::connect(m_adapter.data(), &BleRcuAdapter::poweredChanged,
	                 this, &BleRcuPairingStateMachine::onAdapterPoweredChanged);


	// setup and connect up the timeout the timers
	m_discoveryTimer.setSingleShot(true);
	m_discoveryTimer.setInterval(config->discoveryTimeout());

	m_pairingTimer.setSingleShot(true);
	m_pairingTimer.setInterval(config->pairingTimeout());

	m_setupTimer.setSingleShot(true);
	m_setupTimer.setInterval(config->setupTimeout());

	m_unpairingTimer.setSingleShot(true);
	m_unpairingTimer.setInterval(config->upairingTimeout());

	QObject::connect(&m_discoveryTimer, &QTimer::timeout,
	                 this, &BleRcuPairingStateMachine::onDiscoveryTimeout);
	QObject::connect(&m_pairingTimer, &QTimer::timeout,
	                 this, &BleRcuPairingStateMachine::onPairingTimeout);
	QObject::connect(&m_setupTimer, &QTimer::timeout,
	                 this, &BleRcuPairingStateMachine::onSetupTimeout);
	QObject::connect(&m_unpairingTimer, &QTimer::timeout,
	                 this, &BleRcuPairingStateMachine::onUnpairingTimeout);

}

BleRcuPairingStateMachine::~BleRcuPairingStateMachine()
{
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Configures the internal state machine object.

 */
void BleRcuPairingStateMachine::setupStateMachine()
{
	// set the name of the statemachine for logging
	m_stateMachine.setObjectName(QStringLiteral("PairingStateMachine"));

	// log the transitions at milestone level
	m_stateMachine.setTransistionLogLevel(QtInfoMsg, &milestone());

	// add all the states
	m_stateMachine.addState(RunningSuperState, QStringLiteral("RunningSuperState"));

	m_stateMachine.addState(RunningSuperState, DiscoverySuperState, QStringLiteral("DiscoverySuperState"));
	m_stateMachine.addState(DiscoverySuperState, StartingDiscoveryState, QStringLiteral("StartingDiscoveryState"));
	m_stateMachine.addState(DiscoverySuperState, DiscoveringState, QStringLiteral("DiscoveringState"));

	m_stateMachine.addState(RunningSuperState, StoppingDiscoveryState, QStringLiteral("StoppingDiscoveryState"));
	m_stateMachine.addState(RunningSuperState, PairingSuperState, QStringLiteral("PairingSuperState"));
	m_stateMachine.addState(PairingSuperState, EnablePairableState, QStringLiteral("EnablePairableState"));
	m_stateMachine.addState(PairingSuperState, PairingState, QStringLiteral("PairingState"));
	m_stateMachine.addState(PairingSuperState, SetupState, QStringLiteral("SetupState"));

	m_stateMachine.addState(RunningSuperState, UnpairingState, QStringLiteral("UnpairingState"));
	m_stateMachine.addState(FinishedState, QStringLiteral("FinishedState"));

	m_stateMachine.addState(RunningSuperState, StoppingDiscoveryStartedExternally, QStringLiteral("StoppingDiscoveryStartedExternally"));

	// add the transitions       From State           ->    Event                  ->   To State
	m_stateMachine.addTransition(RunningSuperState,         AdapterPoweredOffEvent,     FinishedState);

	m_stateMachine.addTransition(StartingDiscoveryState,    DiscoveryStartedEvent,      DiscoveringState);
	m_stateMachine.addTransition(DiscoverySuperState,       DeviceFoundEvent,           StoppingDiscoveryState);
	m_stateMachine.addTransition(DiscoverySuperState,       DiscoveryStartTimeoutEvent, FinishedState);
	m_stateMachine.addTransition(DiscoverySuperState,       DiscoveryStoppedEvent,      FinishedState);

	m_stateMachine.addTransition(StoppingDiscoveryState,    DiscoveryStoppedEvent,      EnablePairableState);
	m_stateMachine.addTransition(StoppingDiscoveryState,    DiscoveryStopTimeoutEvent,  FinishedState);

	m_stateMachine.addTransition(EnablePairableState,       PairableEnabledEvent,       PairingState);
	m_stateMachine.addTransition(PairingState,              PairableDisabledEvent,      UnpairingState);
	m_stateMachine.addTransition(PairingState,              DevicePairedEvent,          SetupState);
	m_stateMachine.addTransition(PairingSuperState,         DeviceReadyEvent,           FinishedState);
	m_stateMachine.addTransition(PairingSuperState,         DeviceUnpairedEvent,        FinishedState);
	m_stateMachine.addTransition(PairingSuperState,         DeviceRemovedEvent,         FinishedState);
	m_stateMachine.addTransition(PairingSuperState,         PairingTimeoutEvent,        UnpairingState);
	m_stateMachine.addTransition(PairingSuperState,         SetupTimeoutEvent,          UnpairingState);

	m_stateMachine.addTransition(UnpairingState,            DeviceUnpairedEvent,        FinishedState);
	m_stateMachine.addTransition(UnpairingState,            DeviceRemovedEvent,         FinishedState);
	m_stateMachine.addTransition(UnpairingState,            UnpairingTimeoutEvent,      FinishedState);

	m_stateMachine.addTransition(StoppingDiscoveryStartedExternally,            DiscoveryStoppedEvent,      StartingDiscoveryState);

	// connect to the state entry and exit signals
	QObject::connect(&m_stateMachine, &StateMachine::entered,
	                 this, &BleRcuPairingStateMachine::onStateEntry);
	QObject::connect(&m_stateMachine, &StateMachine::exited,
	                 this, &BleRcuPairingStateMachine::onStateExit);


	// set the initial state
	m_stateMachine.setInitialState(StartingDiscoveryState);
	m_stateMachine.setFinalState(FinishedState);
}

// -----------------------------------------------------------------------------
/*!
	Debugging function that dumps out the current state of the pairing state
	machine.

 */
void BleRcuPairingStateMachine::dump(Dumper out) const
{
	out.printLine("Pairing state machine:");
	out.pushIndent(2);

	out.printBoolean("running:", m_stateMachine.isRunning());
	if (m_stateMachine.isRunning()) {
		out.printLine("pairing code: %03d", m_pairingCode);
		out.printLine("state: %s", qPrintable(m_stateMachine.stateName()));
	}

	out.printLine("stats:");
	out.pushIndent(2);
	out.printLine("pairing attempts: %d", m_pairingAttempts);
	out.printLine("pairing failures: %d", (m_pairingAttempts - m_pairingSuccesses));
	out.popIndent();

	out.popIndent();
}

// -----------------------------------------------------------------------------
/*!
	Returns the current or last pairing code used by this state machine.

	If the state machine hasn't been run 0 is returned.

 */
int BleRcuPairingStateMachine::pairingCode() const
{
	return m_pairingCode;
}

// -----------------------------------------------------------------------------
/*!
	Returns \c true if the state machine is currently running.

 */
bool BleRcuPairingStateMachine::isRunning() const
{
	return m_stateMachine.isRunning();
}

// -----------------------------------------------------------------------------
/*!
	Starts the state machine using the supplied \a pairingCode and
	\a namePrefixes.

 */
void BleRcuPairingStateMachine::start(quint8 filterByte, quint8 pairingCode)
{
	// FIXME: use the filterByte to narrow the search to a certain RCU model
	Q_UNUSED(filterByte);

	// sanity check the statemachine is not already running
	if (Q_UNLIKELY(m_stateMachine.isRunning())) {
		qWarning("state machine already running");
		return;
	}

	// clear the target device
	m_targetAddress.clear();

	// store the pairing code
	m_pairingCode = pairingCode;
	m_pairingMacHash = -1;

	// create a map of OUI (first 3 bytes of mac address) to a regex to match to
	// the name of the device
	m_pairingPrefixes.clear();
	m_supportedPairingNames.clear();
	QMap<quint32, QByteArray>::const_iterator it = m_pairingPrefixFormats.begin();
	for (; it != m_pairingPrefixFormats.end(); ++it) {

		// construct the wildcard match
		QRegExp regEx(QString_asprintf(it.value().constData(), pairingCode));
		regEx.setPatternSyntax(QRegExp::WildcardUnix);

		qInfo("pairing regex for %02hhX:%02hhX:%02hhX:xx:xx:xx is '%s'",
		      quint8(it.key() >> 16), quint8(it.key() >> 8), quint8(it.key() >> 0),
		      regEx.pattern().toLatin1().constData());

		// add to the map to use for compare when a device is found
		m_pairingPrefixes.insert(it.key(), std::move(regEx));
		m_supportedPairingNames.push_back(std::move(regEx));
	}

	// start the state machine
	m_stateMachine.start();

	m_pairingAttempts++;
	m_pairingSucceeded = false;
	qMilestone("started pairing using code %03d", m_pairingCode);
}

// -----------------------------------------------------------------------------
/*!
	Starts the state machine using the supplied \a pairingCode and
	\a namePrefixes.

 */
void BleRcuPairingStateMachine::startMacHash(quint8 filterByte, quint8 macHash)
{
	// FIXME: use the filterByte to narrow the search to a certain RCU model
	Q_UNUSED(filterByte);

	// sanity check the statemachine is not already running
	if (Q_UNLIKELY(m_stateMachine.isRunning())) {
		qWarning("state machine already running");
		return;
	}

	// clear the target device
	m_targetAddress.clear();

	// clear the pairing code
	m_pairingCode = -1;

	// store the MAC hash
	m_pairingMacHash = macHash;

	// clear the maps, we are trying to pair to a specific device using a hash
	// of the MAC address instead
	m_pairingPrefixes.clear();
	m_supportedPairingNames.clear();

	// start the state machine
	m_stateMachine.start();

	m_pairingAttempts++;
	m_pairingSucceeded = false;
	qMilestone("started pairing, searching for device with MAC hash 0x%02X", m_pairingMacHash);
}

// -----------------------------------------------------------------------------
/*!
	Starts the pairing state machine, but skips the discovery phase as we
	already have a \a target device.

 */
void BleRcuPairingStateMachine::start(const BleAddress &target, const QString &name)
{
	// sanity check the state machine is not already running
	if (Q_UNLIKELY(m_stateMachine.isRunning())) {
		qWarning("state machine already running");
		return;
	}

	// set the target device
	m_targetAddress = target;

	// clear the pairing code
	m_pairingCode = -1;
	m_pairingMacHash = -1;

	// set the pairing prefix map to contain just the one name match
	m_pairingPrefixes.clear();
	m_supportedPairingNames.clear();

	m_pairingPrefixes.insert(target.oui(),
	                         QRegExp(name, Qt::CaseInsensitive, QRegExp::FixedString));
	m_supportedPairingNames.push_back(QRegExp(name, Qt::CaseInsensitive, QRegExp::FixedString));

	// start the state machine
	m_stateMachine.start();

	m_pairingAttempts++;
	m_pairingSucceeded = false;
	qMilestone("started pairing targeting %s", qPrintable(target.toString()));
}

// -----------------------------------------------------------------------------
/*!
	Stops the state machine by posting a cancel message to it.

	The stop may be asynchronous, you can either poll on the isRunning() call or
	wait for either the failed() or succeeded() signals.

 */
void BleRcuPairingStateMachine::stop()
{
	// TODO: implement
	qError("cancel pairing not implemented");
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Slot called when entering a new state.

 */
void BleRcuPairingStateMachine::onStateEntry(int state)
{
	switch (state) {
		case StartingDiscoveryState:
			onEnteredStartDiscoveryState();
			break;
		case DiscoveringState:
			onEnteredDiscoveringState();
			break;
		case StoppingDiscoveryState:
			onEnteredStoppingDiscoveryState();
			break;
		case EnablePairableState:
			onEnteredEnablePairableState();
			break;
		case PairingState:
			onEnteredPairingState();
			break;
		case SetupState:
			onEnteredSetupState();
			break;
		case UnpairingState:
			onEnteredUnpairingState();
			break;
		case FinishedState:
			onEnteredFinishedState();
			break;
		case StoppingDiscoveryStartedExternally:
			onEnteredStoppingDiscoveryStartedExternally();
		default:
			break;
	}
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Slot called when existing a state.

 */
void BleRcuPairingStateMachine::onStateExit(int state)
{
	switch (state) {
		case DiscoverySuperState:
			onExitedDiscoverySuperState();
			break;
		case PairingSuperState:
			onExitedPairingSuperState();
			break;
		case UnpairingState:
			onExitedUnpairingState();
			break;

		default:
			break;
	}
}

void BleRcuPairingStateMachine::onEnteredStoppingDiscoveryStartedExternally()
{
	lastOperationType = m_btrMgrAdapter.stopDiscovery();
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Called when entering the 'starting discovery' state.  We hook this point
	to post a delayed message to the state machine to handle the discovery
	timeout.

	This method also emits the started() signal.

 */
void BleRcuPairingStateMachine::onEnteredStartDiscoveryState()
{
	// start a timer for timing out the discovery
	m_discoveryTimer.start();

	// tell anyone who cares that pairing has started
	emit started();

	// check if we're already in discovery mode (we shouldn't be) and if so then
	// post a message to move off the initial state
	if (m_adapter->isDiscovering()) {
		qWarning("adapter was already in discovery mode, this is unusual but "
		         "shouldn't be a problem");

		// even though the adapter is telling us we're in discovery mode, sometimes
		// it lies, so issue another start request anyway, it doesn't hurt
		// if it arrives twice
		m_adapter->startDiscovery(m_pairingCode);

		// trigger a move to the discovering state
		m_stateMachine.postEvent(DiscoveryStartedEvent);

	} else {
		// otherwise ask the manager (to ask bluez) to start the scan
		m_adapter->startDiscovery(m_pairingCode);
	}
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Slot called when the discovery status of the bluetooth adapter changes.

 */
void BleRcuPairingStateMachine::onDiscoveryChanged(bool discovering)
{
	if (Q_UNLIKELY(!m_stateMachine.isRunning()))
	{
		qDebug("running onDiscoveryChanged when state machine is not running, let's store current discovery status = %s", discovering ? "true" : "false");
		discoveryStartedExternally = discovering;
		if (discoveryStartedExternally) {
			m_stateMachine.setInitialState(StoppingDiscoveryStartedExternally);
		}
		else {
			m_stateMachine.setInitialState(StartingDiscoveryState);
		}
		return;
	}

	if (discovering)
		m_stateMachine.postEvent(DiscoveryStartedEvent);
	else
		m_stateMachine.postEvent(DiscoveryStoppedEvent);
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Obviously called when the singleshot \l{m_discoveryTimer} timer expires.

 */
void BleRcuPairingStateMachine::onDiscoveryTimeout()
{
	if (Q_UNLIKELY(!m_stateMachine.isRunning()))
		return;

	if (m_stateMachine.inState(DiscoverySuperState)) {

		if (m_stateMachine.inState(StartingDiscoveryState))
			qError("timed-out waiting for discovery started signal");
		else
			qWarning("timed-out in discovery phase (didn't find target rcu"
			         " device to pair to)");

		m_stateMachine.postEvent(DiscoveryStartTimeoutEvent);
	}
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Called when entering the 'discovering' state.  At this point we query the
	manager for the current list of devices and their names.  We use this to
	determine if any existing devices match the pairing prefix.

 */
void BleRcuPairingStateMachine::onEnteredDiscoveringState()
{
	// get the current list of devices
	const QMap<BleAddress, QString> deviceNames = m_adapter->deviceNames();

	// process each existing device
	QMap<BleAddress, QString>::const_iterator it = deviceNames.begin();
	for (; it != deviceNames.end(); ++it)
		processDevice(it.key(), it.value());
}

void BleRcuPairingStateMachine::onExitedDiscoverySuperState()
{
	// stop the discovery timeout timer
	m_discoveryTimer.stop();

	// and stop the actually discovery
	m_adapter->stopDiscovery();
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Called upon entry to the 'Stopping Discovery' state, the request to stop has
	already been sent (on the exit of the 'Discovering' super state), so we
	just need to check that discovery is not already stopped.

 */
void BleRcuPairingStateMachine::onEnteredStoppingDiscoveryState()
{
	// start the pairing timeout timer
	m_pairingTimer.start();

	// if we've got to this state it means we have a target device
	Q_ASSERT(!m_targetAddress.isNull());

	// if entered this state and discovery is not running then post a discovery
	// stopped to get out of this event
	if (Q_UNLIKELY(!m_adapter->isDiscovering()))
		m_stateMachine.postEvent(DiscoveryStoppedEvent);
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Called when the 'pairable' state of the adaptor changes, the pairable state
	is important because if you pair to an RCU when not in the pairable state
	then bluez doesn't set the correct secure pairing flags.

	\note Pairability only applies to the bluez backend, on android the system
	is always pairable when the bonding request is sent.

 */
void BleRcuPairingStateMachine::onPairableChanged(bool pairable)
{
	if (Q_UNLIKELY(!m_stateMachine.isRunning()))
		return;

	// if pairable is disabled while in the pairing state it causes the state
	// machine to (correctly) abort the process, however the following is
	// here to log it for debugging
	//
	// this is caused by NGDEV-83943, fusion is meddling where it shouldn't
	if (m_stateMachine.inState(PairingSuperState) && (pairable == false))
		qWarning("adaptor 'pairable' disabled before target device became ready");

	if (pairable)
		m_stateMachine.postEvent(PairableEnabledEvent);
	else
		m_stateMachine.postEvent(PairableDisabledEvent);
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Called when entering the 'enable pairable' state.  We hook this point to
	cancel the discovery timeout event, start a new timer for the pairing timeout.

 */
void BleRcuPairingStateMachine::onEnteredEnablePairableState()
{
	// if we've got to this state it means we have a target device
	Q_ASSERT(!m_targetAddress.isNull());

	// check if the adapter is pairable
	if (m_adapter->isPairable()) {

		// is already pairable so just post the 'enabled' event
		m_stateMachine.postEvent(PairableEnabledEvent);

	} else {

		// otherwise set pairable state to true with a timeout, the timeout is
		// set to 5 seconds past the overall time we've given the state machine
		// to pair with the rcu.
		m_adapter->enablePairable(m_pairingTimer.interval() + 5000);

		// and wait for the notification that pairable was enabled ...
	}
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Called when entering the 'pairing' state.  We hook this point to request
	the adapter to add (pair/bond to) the target device.

 */
void BleRcuPairingStateMachine::onEnteredPairingState()
{
	// if we've got to this state it means we have a target device
	Q_ASSERT(!m_targetAddress.isNull());

	// request the manager to pair with the device
	m_adapter->addDevice(m_targetAddress);
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Obviously called when the singleshot \l{m_pairingTimer} timer expires.

 */
void BleRcuPairingStateMachine::onPairingTimeout()
{
	if (Q_UNLIKELY(!m_stateMachine.isRunning()))
		return;

	if (m_stateMachine.inState(StoppingDiscoveryState)) {
		qError("timed-out waiting for discovery to stop (suggesting something"
		       " has gone wrong inside bluez)");
		m_stateMachine.postEvent(DiscoveryStopTimeoutEvent);

	} else if (m_stateMachine.inState( { EnablePairableState, PairingState } )) {
		qWarning("timed-out in pairing phase (rcu device didn't pair within %dms)",
		         m_pairingTimer.interval());
		m_stateMachine.postEvent(PairingTimeoutEvent);

	}
}

// -----------------------------------------------------------------------------
/*!
	\internal


 */
void BleRcuPairingStateMachine::onEnteredSetupState()
{
	// start the setup timeout timer
	m_setupTimer.start();
	qDebug("starting setup timeout timer for %dms", m_setupTimer.interval());

	// nothing else needed to do
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Obviously called when the singleshot \l{m_setupTimer} timer expires.

 */
void BleRcuPairingStateMachine::onSetupTimeout()
{
	if (Q_UNLIKELY(!m_stateMachine.isRunning()))
		return;

	if (m_stateMachine.inState(SetupState)) {
		qWarning("timed-out in setup phase (rcu didn't response to all requests"
		         " within %dms)", m_setupTimer.interval());
		m_stateMachine.postEvent(SetupTimeoutEvent);
	}
}

// -----------------------------------------------------------------------------
/*!
	\internal


 */
void BleRcuPairingStateMachine::onExitedPairingSuperState()
{
	// stop the pairing and setup timeout timers
	m_pairingTimer.stop();
	m_setupTimer.stop();

	// clear the pairable state of the adaptor
	m_adapter->disablePairable();
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Called when entering the 'pairing' state.  We hook this point to cancel the
	discovery timeout event, start a new timer for the pairing timeout and then
	emit the enteredPairingState() signal with the BDADDR of the device we're
	trying to pair to.

 */
void BleRcuPairingStateMachine::onEnteredUnpairingState()
{
	// start the unpairing timeout timer
	m_unpairingTimer.start();

	// if we've got to this state it means we have a target device
	Q_ASSERT(!m_targetAddress.isNull());

	// remove (unpair) the target device because we've failed :-(
	if (m_adapter->removeDevice(m_targetAddress) == false)
		m_stateMachine.postEvent(DeviceUnpairedEvent);
}

void BleRcuPairingStateMachine::onUnpairingTimeout()
{
	if (Q_UNLIKELY(!m_stateMachine.isRunning()))
		return;

	if (m_stateMachine.inState(UnpairingState)) {
		qWarning("timed-out in un-pairing phase (failed rcu may be left paired)");
		m_stateMachine.postEvent(UnpairingTimeoutEvent);
	}
}

void BleRcuPairingStateMachine::onExitedUnpairingState()
{
	// stop the unpairing timeout timer
	m_unpairingTimer.stop();
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Called when entering the 'finished' state.  We hook this point to cancel to
	clean up any posted delayed events and then emit a finished() signal.

 */
void BleRcuPairingStateMachine::onEnteredFinishedState()
{
	Q_ASSERT(!m_discoveryTimer.isActive());
	Q_ASSERT(!m_pairingTimer.isActive());
	Q_ASSERT(!m_setupTimer.isActive());
	Q_ASSERT(!m_unpairingTimer.isActive());

	if (discoveryStartedExternally) {
		qDebug("discovery has been started externally and then stopped, so let's resume it");
		m_btrMgrAdapter.startDiscovery(lastOperationType);
		discoveryStartedExternally = false;
		lastOperationType = BtrMgrAdapter::unknownOperation;
	}

	// finally just emit a finished signal to the BleRcuManagerImpl object
	(m_pairingSucceeded ? emit finished() : emit failed());
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Called when the an outside object has called either deviceAdded() or
	deviceNameChanged(), it then checks if the \a name matches our expected
	pairing prefix.  If it does then we check if we already have a pairing
	target, if not we use this new device.

 */
void BleRcuPairingStateMachine::processDevice(const BleAddress &address,
                                              const QString &name)
{
	// get the pairing name prefix to use for the given device (based on the
	// OUI of the device's MAC address)
	QMap<quint32, QRegExp>::const_iterator it = m_pairingPrefixes.find(address.oui());
	if (it != m_pairingPrefixes.end()) {
		if (!it->exactMatch(name)) {
			return;
		}
	} else {
		// didn't find it based on OUI, so iterate through and compare names
		QVector<QRegExp>::const_iterator it_name = m_supportedPairingNames.begin();
		for (; it_name != m_supportedPairingNames.end(); ++it_name) {
			if (it_name->exactMatch(name)) {
				qInfo() << "OUI not found, but matched name successfully, name: " << name << ", address: " << address;
				break;
			}
		}

		if (it_name == m_supportedPairingNames.end()) {
			// Device not found through conventional means, see if we are pairing based on MAC hash
			if (m_pairingMacHash != -1) {
				// Check if MAC hash matches
				int macHash = 0;
				for (int i = 0; i < 6; ++i) {
					macHash += (int)address[i];
				}
				macHash &= 0xFF;
				qInfo() << "Validating device based on MAC hash, requested MAC hash = " << m_pairingMacHash 
						<< ", MAC hash of this device = " << macHash << ", name: " << name << ", address: " << address;
				if (m_pairingMacHash != macHash) {
					return;
				}
			} else {
				// log an error if we don't already have a target device (and
				// therefore have a restricted m_pairingPrefixes set)
				if (m_targetAddress.isNull()) {
					qWarning() << "odd, don't have a name prefix for device" << address;
				}
				return;
			}
		}
	}


	// if we don't already have a target address then store this now
	if (m_targetAddress.isNull()) {

		// is the device currently paired? if so we have to remove (unpair)
		// it and then remain in the current state
		if (m_adapter->isDevicePaired(address)) {

			qInfo() << "found target device" << address
					<< "but it's currently paired, will unpair and "
						"wait till it shows up in a scan again";

			m_adapter->removeDevice(address);
			return;
		}

		qInfo() << "found target device" << address;

		// store the target address
		m_targetAddress = address;

	} else if (Q_UNLIKELY(m_targetAddress != address)) {

		// this may happen if two remotes have the same pairing prefix,
		// in such situations we stick with the first one we found, there
		// is no way to know which is the correct device to pair to
		qWarning().nospace() << "device added with correct pairing prefix, "
								<< "however it's address doesn't match previous "
								<< "found device (prev:" << m_targetAddress
								<< " new:" << address << name << ")";
		return;
	}

	// post the event to move the state machine
	m_stateMachine.postEvent(DeviceFoundEvent);
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Slot expected to be called from outside this object to indicate that
	a new device has been detected by the bluetooth adapter.

	The \a address and \a name of the added device should be supplied, these
	are checked to see if they match the pairing target.

 */
void BleRcuPairingStateMachine::onDeviceFound(const BleAddress &address,
                                              const QString &name)
{
	if (Q_UNLIKELY(!m_stateMachine.isRunning()))
		return;

	qDebug() << "device added" << address << name
	         << "(target" << m_targetAddress << ")";

	processDevice(address, name);
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Called by the bluez code when a remote device has been removed.  This means
	the device has been unpaired and has disconnected.

 */
void BleRcuPairingStateMachine::onDeviceRemoved(const BleAddress &address)
{
	if (Q_UNLIKELY(!m_stateMachine.isRunning()))
		return;

	qDebug() << "device removed" << address
	         << "(target" << m_targetAddress << ")";

	// check if the device removed is the one we're targeting
	if (!m_targetAddress.isNull() && (m_targetAddress == address)) {
		m_stateMachine.postEvent(DeviceRemovedEvent);
	}
}

// -----------------------------------------------------------------------------
/*!
	Slot expected to be called from outside this object to indicate that
	a new device has changed it's name.

	The \a address and new \a name of the device should be supplied, these
	are checked to see if they match the pairing target.

 */
void BleRcuPairingStateMachine::onDeviceNameChanged(const BleAddress &address,
                                                    const QString &name)
{
	if (Q_UNLIKELY(!m_stateMachine.isRunning()))
		return;

	qDebug() << "device name changed" << address << name
	         << "(target" << m_targetAddress << ")";

	processDevice(address, name);
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Slot called when a device's \a paired state changes.

	We check if the device who's paired state has changed is our target device
	and if it is now paired then we send a \l{DevicePairedEvent} event to the
	state machine.

	\note There is deliberately no event emitted for an unpaired event, we leave
	it to the timeouts to handle that case.
 */
void BleRcuPairingStateMachine::onDevicePairingChanged(const BleAddress &address,
                                                       bool paired)
{
	if (Q_UNLIKELY(!m_stateMachine.isRunning()))
		return;

	// check if the device whos pairing has changed is the one we're trying to
	// pair to
	if (!m_targetAddress.isNull() && (m_targetAddress == address)) {
		if (paired)
			m_stateMachine.postEvent(DevicePairedEvent);
		else
			m_stateMachine.postEvent(DeviceUnpairedEvent);
	}
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Slot called when the given device has become 'ready'.  Ready is the state
	where the device is bonded and connected and all the GATT services have been
	resolved and lastly our GATT service objects have run through their
	initialisation code (successfully).

 */
void BleRcuPairingStateMachine::onDeviceReadyChanged(const BleAddress &address,
                                                     bool ready)
{
	if (Q_UNLIKELY(!m_stateMachine.isRunning()))
		return;

	// check if the device now ready is the one we're trying to pair to
	if (!m_targetAddress.isNull() && (m_targetAddress == address)) {
		if (ready) {
			m_pairingSuccesses++;
			m_pairingSucceeded = true;
			m_stateMachine.postEvent(DeviceReadyEvent);
		}
	}
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Slot called when lost connection to the BLE adaptor, this should never
	really happen and means we've lost connection to the bluetoothd daemon. The
	only sensible thing we can do is abort that pairing.

 */
void BleRcuPairingStateMachine::onAdapterPoweredChanged(bool powered)
{
	if (Q_UNLIKELY(!m_stateMachine.isRunning()))
		return;

	if (!powered)
		m_stateMachine.postEvent(AdapterPoweredOffEvent);
}


