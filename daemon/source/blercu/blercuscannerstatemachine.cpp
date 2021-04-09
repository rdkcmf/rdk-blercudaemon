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
//  blercucscannerstatemachine.cpp
//  SkyBluetoothRcu
//

#include "blercuscannerstatemachine.h"
#include "blercuadapter.h"

#include "configsettings/configsettings.h"
#include "utils/logging.h"

#include <QTime>

#include <cinttypes>



BleRcuScannerStateMachine::BleRcuScannerStateMachine(const QSharedPointer<const ConfigSettings> &config,
                                                     const QSharedPointer<BleRcuAdapter> &adapter,
                                                     QObject *parent)
	: QObject(parent)
	, m_adapter(adapter)
	, m_scanTimeoutMs(-1)
{

	// constructs a map of name printf style formats for searching for device
	// names that match
	const QList<ConfigModelSettings> models = config->modelSettings();
	for (const ConfigModelSettings &model : models) {
		if (!model.disabled())
			m_deviceNameMatchers.insert(model.oui(), model.scanNameMatcher());
			m_supportedPairingNames.push_back(model.scanNameMatcher());
	}


	// setup (but don't start) the state machine
	setupStateMachine();

	// connect up the events from the manager
	QObject::connect(m_adapter.data(), &BleRcuAdapter::discoveryChanged,
	                 this, &BleRcuScannerStateMachine::onDiscoveryChanged);

	QObject::connect(m_adapter.data(), &BleRcuAdapter::deviceFound,
	                 this, &BleRcuScannerStateMachine::onDeviceFound);
	QObject::connect(m_adapter.data(), &BleRcuAdapter::deviceNameChanged,
	                 this, &BleRcuScannerStateMachine::onDeviceNameChanged);

	QObject::connect(m_adapter.data(), &BleRcuAdapter::poweredChanged,
	                 this, &BleRcuScannerStateMachine::onAdapterPoweredChanged);


}

BleRcuScannerStateMachine::~BleRcuScannerStateMachine()
{
	stop();
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Configures the internal state machine object.

 */
void BleRcuScannerStateMachine::setupStateMachine()
{
	// set the name of the state machine for logging
	m_stateMachine.setObjectName(QStringLiteral("ScannerStateMachine"));

	// log the transitions at milestone level
	m_stateMachine.setTransistionLogLevel(QtInfoMsg);

	// add all the states
	m_stateMachine.addState(RunningSuperState, QStringLiteral("RunningSuperState"));
	m_stateMachine.addState(RunningSuperState, StartingDiscoveryState, QStringLiteral("StartingDiscoveryState"));
	m_stateMachine.addState(RunningSuperState, DiscoveringState, QStringLiteral("DiscoveringState"));
	m_stateMachine.addState(RunningSuperState, StoppingDiscoveryState, QStringLiteral("StoppingDiscoveryState"));
	m_stateMachine.addState(FinishedState, QStringLiteral("FinishedState"));


	// add the transitions       From State           ->    Event                  ->   To State
	m_stateMachine.addTransition(RunningSuperState,         AdapterPoweredOffEvent,     FinishedState);

	m_stateMachine.addTransition(StartingDiscoveryState,    DiscoveryStartedEvent,      DiscoveringState);
	m_stateMachine.addTransition(StartingDiscoveryState,    CancelRequestEvent,         StoppingDiscoveryState);
	m_stateMachine.addTransition(StartingDiscoveryState,    DiscoveryStartTimeoutEvent, FinishedState);

	m_stateMachine.addTransition(DiscoveringState,          DeviceFoundEvent,           StoppingDiscoveryState);
	m_stateMachine.addTransition(DiscoveringState,          CancelRequestEvent,         StoppingDiscoveryState);
	m_stateMachine.addTransition(DiscoveringState,          DiscoveryTimeoutEvent,      StoppingDiscoveryState);
	m_stateMachine.addTransition(DiscoveringState,          DiscoveryStoppedEvent,      FinishedState);

	m_stateMachine.addTransition(StoppingDiscoveryState,    DiscoveryStoppedEvent,      FinishedState);
	m_stateMachine.addTransition(StoppingDiscoveryState,    DiscoveryStopTimeoutEvent,  FinishedState);


	// connect to the state entry and exit signals
	QObject::connect(&m_stateMachine, &StateMachine::entered,
	                 this, &BleRcuScannerStateMachine::onStateEntry);
	QObject::connect(&m_stateMachine, &StateMachine::exited,
	                 this, &BleRcuScannerStateMachine::onStateExit);


	// set the initial state
	m_stateMachine.setInitialState(StartingDiscoveryState);
	m_stateMachine.setFinalState(FinishedState);
}

// -----------------------------------------------------------------------------
/*!
	Converts a millisecond value to days, hours, seconds, etc

 */
QString BleRcuScannerStateMachine::formatTimeDuration(qint64 millis) const
{
	if (millis < 0)
		return QString("-");

	constexpr qint64 millisPerDay = (24 * 60 * 60 * 1000);
	const qint64 days = millis / millisPerDay;

	QTime t = QTime(0, 0)
			.addMSecs(static_cast<int>(millis - (days * millisPerDay)));

	return QString("%1d %2")
			.arg(days)
			.arg(t.toString("hh:mm:ss.zzz"));
}


// -----------------------------------------------------------------------------
/*!
	Debugging function that dumps out the current state of the pairing state
	machine.

 */
void BleRcuScannerStateMachine::dump(Dumper out) const
{
	out.printLine("Scanning state machine:");
	out.pushIndent(2);

	out.printBoolean("running:   ", m_stateMachine.isRunning());
	if (m_stateMachine.isRunning()) {

		out.printLine("state:     %s", qPrintable(m_stateMachine.stateName()));

		if (m_scanTimeoutMs < 0)
			out.printLine("remaining: -");
		else if (!m_scanElapsedTime.isValid())
			out.printLine("remaining: %dms", m_scanTimeoutMs);
		else
			out.printLine("remaining: %dms",
			              (m_scanTimeoutMs - int(m_scanElapsedTime.elapsed())));
	}

	out.popIndent();
}

// -----------------------------------------------------------------------------
/*!
	Returns \c true if the state machine is currently running.

 */
bool BleRcuScannerStateMachine::isRunning() const
{
	return m_stateMachine.isRunning();
}

// -----------------------------------------------------------------------------
/*!
	Starts the state machine for the scanner.  The scan will run for the given
	\c timeoutMs or until an RCU is found in pairing mode or cancelled.

 */
void BleRcuScannerStateMachine::start(int timeoutMs)
{
	// sanity check
	if (Q_UNLIKELY(m_stateMachine.isRunning())) {
		qError("scanner already running");
		return;
	}

	qInfo("starting scanner with timeout %dms", timeoutMs);

	// ensure the found device is cleared
	m_foundDevice.clear();

	// set the discovery time-out
	m_scanTimeoutMs = timeoutMs;

	// clear the elapsed timer, it is only started when discovery is started
	m_scanElapsedTime.invalidate();

	// start the state machine
	m_stateMachine.start();
}

// -----------------------------------------------------------------------------
/*!
	Cancels the scanning by injecting a cancel event into the state machine
	should clean up the discovery.

	Note this is an async call, you should listen for the finished signal which
	indicates the scanning is finished.

 */
void BleRcuScannerStateMachine::stop()
{
	// sanity check
	if (!m_stateMachine.isRunning()) {
		qInfo("scanner not running");
		return;
	}

	qInfo("cancelling scanner");

	// post a cancel event and let the state-machine clean up
	m_stateMachine.postEvent(CancelRequestEvent);
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Slot called when lost connection to the BLE adaptor, this should never
	really happen and means we've lost connection to the bluetoothd daemon. The
	only sensible thing we can do is abort the scanning.

 */
void BleRcuScannerStateMachine::onAdapterPoweredChanged(bool powered)
{
	if (!m_stateMachine.isRunning())
		return;

	if (!powered)
		m_stateMachine.postEvent(AdapterPoweredOffEvent);
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Called when the discovery state of the adapter changed.

 */
void BleRcuScannerStateMachine::onDiscoveryChanged(bool discovering)
{
	// ignore if not running
	if (!m_stateMachine.isRunning())
		return;

	if (discovering)
		m_stateMachine.postEvent(DiscoveryStartedEvent);
	else
		m_stateMachine.postEvent(DiscoveryStoppedEvent);
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Called when the discovery state of the adapter changed.

 */
void BleRcuScannerStateMachine::onDiscoveryStartTimeout()
{
	if (m_stateMachine.isRunning())
		m_stateMachine.postEvent(DiscoveryStartTimeoutEvent);
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Called when the discovery state of the adapter changed.

 */
void BleRcuScannerStateMachine::onDiscoveryTimeout()
{
	if (m_stateMachine.isRunning())
		m_stateMachine.postEvent(DiscoveryTimeoutEvent);
}


// -----------------------------------------------------------------------------
/*!
	\internal

	Called when a new device is found by the bluetooth adapter.

 */
void BleRcuScannerStateMachine::onDeviceFound(const BleAddress &address,
                                              const QString &name)
{
	// ignore if not running or not in the discovery phase
	if (!m_stateMachine.isRunning() || !m_stateMachine.inState(DiscoveringState))
		return;

	// process the new device
	processDevice(address, name);
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Called when a device's name has changed.

 */
void BleRcuScannerStateMachine::onDeviceNameChanged(const BleAddress &address,
                                                    const QString &name)
{
	// ignore if not running or not in the discovery phase
	if (!m_stateMachine.isRunning() || !m_stateMachine.inState(DiscoveringState))
		return;

	// process the new device name
	processDevice(address, name);
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Called when a device name has changed or a new device is found.  Here we
	check if the device name indicates it's an RCU in pairing mode.

 */
void BleRcuScannerStateMachine::processDevice(const BleAddress &address,
                                              const QString &name)
{
	// if we've already found a target then skip out early
	if (!m_foundDevice.isNull())
		return;

	// check if the name is a match for one of our RCU types
	QMap<quint32, QRegExp>::const_iterator it =
			m_deviceNameMatchers.find(address.oui());

	// check if we have the device's OUI in the map
	if (it != m_deviceNameMatchers.end()) {
		if (!it->exactMatch(name))
			return;
	} else {
		// didn't find it based on OUI, so iterate through and compare names
		QVector<QRegExp>::const_iterator it_name = m_supportedPairingNames.begin();
		for (; it_name != m_supportedPairingNames.end(); ++it_name) {
			if (it_name->exactMatch(name)) {
				qInfo() << "OUI not found, but matched name successfully, name: " << name << ", address: " << address;
				break;
			}
		}

		if (it_name == m_supportedPairingNames.end())
			return;
	}

	if (m_adapter->isDevicePaired(address)) {
		qInfo() << "device is currently paired - address: " << address << ", name: " << name << ", so ignoring....";
		return;
	}

	qInfo() << "found pairable device" << address
			<< "with name" << name;

	// store the address
	m_foundDevice.address = address;
	m_foundDevice.name = name;

	// update the state machine
	m_stateMachine.postEvent(DeviceFoundEvent);
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Slot called when entering a new state.

 */
void BleRcuScannerStateMachine::onStateEntry(int state)
{
	switch (state) {
		case StartingDiscoveryState:
			onEnteredStartDiscoveryState();
			break;
		case DiscoveringState:
			onEnteredDiscoveringState();
			break;
		case StoppingDiscoveryState:
			onEnteredStopDiscoveryState();
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

	Slot called when existing a state.

 */
void BleRcuScannerStateMachine::onStateExit(int state)
{
	Q_UNUSED(state);
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Called when entering the 'start discovering' state.  This is where we
	request the adapter to start discovery.

	This method also emits the started() signal.

 */
void BleRcuScannerStateMachine::onEnteredStartDiscoveryState()
{
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
		m_adapter->startDiscovery(-1);

		// trigger a move to the discovering state
		m_stateMachine.postEvent(DiscoveryStartedEvent);

	} else {
		// otherwise ask the manager (to ask bluez) to start the scan
		m_adapter->startDiscovery(-1);

		// post a timed-out delay message, if we're still in the 'start
		// discovery' phase when the timeout event arrives we cancel it
		m_stateMachine.postDelayedEvent(DiscoveryStartTimeoutEvent, 5000);
	}
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Called when entering the 'discovering' state.  At this point we query the
	manager for the current list of devices and their names.  We use this to
	determine if any existing devices match the name of a pairable RCU.

 */
void BleRcuScannerStateMachine::onEnteredDiscoveringState()
{
	// store the time we started discovery, only used for debugging
	m_scanElapsedTime.start();

	// if the scanner was started with a timeout then add a delayed event
	// to the state machine that'll stop the scanner after x number of milliseconds
	if (m_scanTimeoutMs >= 0)
		m_stateMachine.postDelayedEvent(DiscoveryTimeoutEvent, m_scanTimeoutMs);


	// get the current list of devices
	const QMap<BleAddress, QString> deviceNames = m_adapter->deviceNames();

	// process each existing device
	QMap<BleAddress, QString>::const_iterator it = deviceNames.begin();
	for (; it != deviceNames.end(); ++it)
		processDevice(it.key(), it.value());
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Called upon entry to the 'Stopping Discovery' state, here we send the request
	to stop the discovery.

 */
void BleRcuScannerStateMachine::onEnteredStopDiscoveryState()
{
	// send the request to stop discovery
	m_adapter->stopDiscovery();

	// check if already stopped
	if (!m_adapter->isDiscovering()) {
		m_stateMachine.postEvent(DiscoveryStoppedEvent);

	} else {
		// post a timed-out delay message, if we're still in the 'stop
		// discovery' phase when the event arrives we assuem something
		// has gone wrong and give up
		m_stateMachine.postDelayedEvent(DiscoveryStopTimeoutEvent, 3000);
	}
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Called when entering the 'finished' state.  We can enter this state if
	we've failed, been cancelled or on success.  Regardless we emit the finished
	signal.

	If we did manage to find a target device then we also emit the
	foundPairableDevice() signal.

 */
void BleRcuScannerStateMachine::onEnteredFinishedState()
{
	// if we found a device then tell any clients
	if (!m_foundDevice.address.isNull()) {
		emit foundPairableDevice(m_foundDevice.address, m_foundDevice.name);
		m_foundDevice.clear();
	} else {
		emit failed();
	}

	// and we're done
	emit finished();
}






