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
//  blercuadapter.cpp
//  SkyBluetoothRcu
//

#include "blercuadapter_p.h"
#include "blercudevice_p.h"
#include "blercurecovery.h"
#include "blercu/bleservices/blercuservicesfactory.h"

#include "dbus/dbusobjectmanager.h"
#include "interfaces/bluezadapterinterface.h"
#include "interfaces/bluezdeviceinterface.h"

#include "configsettings/configsettings.h"
#include "utils/logging.h"



// -----------------------------------------------------------------------------
/*!
	\class BleRcuAdapterBluez
	\brief The BleRcuAdapterBluez is essentially a wrapper around the bluez
	adapter interface, however it runs it's own state machine and also stores
	the BleRcuDeviceBluez objects for any known device.


	State Machine
	This class implements the following state machine, it tries at all times
	to ensure that the adapter is available and powered.

	\image BleRcuManager.svg

	The service registered / unregistered refer to the dbus service that the
	bluetooth daemon exposes.  It should always be registered, however if
	for whatever reason the daemon crashes and is restarted, then the service
	will be unregistered and then re-registered, this object attempts to handle
	those cases gracefully.

	The adapter availability events should also also never really happen on a
	stable running system, an adapter refers to the bluetooth device on the
	host.  On Sky STB's this is always present, there is no hotplugging of
	bluetooth adapters.  However the state changes are handled in this object
	in case the adapter had to be reset for any reason.



 */





// -----------------------------------------------------------------------------
/*!
	\internal

	Static function used at construction time to create a set of supported
	device OUIs from the \l{ConfigSettings} vendor details list.

 */
QSet<quint32> BleRcuAdapterBluez::getSupportedOuis(const QList<ConfigModelSettings> &modelDetails)
{
	QSet<quint32> ouis;

	for (const ConfigModelSettings &model : modelDetails) {
		if (!model.disabled())
			ouis.insert(model.oui());
	}

	return ouis;
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Static function used at construction time to create a set of supported
	device names from the \l{ConfigSettings} vendor details list.

 */
QVector<QRegExp> BleRcuAdapterBluez::getSupportedPairingNames(const QList<ConfigModelSettings> &modelDetails)
{
	QVector<QRegExp> names;

	for (const ConfigModelSettings &model : modelDetails) {
		if (!model.disabled()) {
			names.push_back(model.scanNameMatcher());
		}
	}

	return names;
}



BleRcuAdapterBluez::BleRcuAdapterBluez(const QSharedPointer<const ConfigSettings> &config,
                                       const QSharedPointer<BleRcuServicesFactory> &servicesFactory,
                                       const QDBusConnection &bluezBusConn,
                                       QObject *parent)
	: BleRcuAdapter(parent)
	, m_servicesFactory(servicesFactory)
	, m_bluezDBusConn(bluezBusConn)
	, m_bluezService("org.bluez")
	, m_discovering(false)
	, m_pairable(false)
	, m_discoveryRequests(0)
	, m_discoveryRequested(StopDiscovery)
	, m_supportedOuis(getSupportedOuis(config->modelSettings()))
	, m_supportedPairingNames(getSupportedPairingNames(config->modelSettings()))
	, m_retryEventId(-1)
{

	// we always want monotonic elapsed timers, however we carry on if we don't
	// but just log it as a warning
	if (!QElapsedTimer::isMonotonic())
		qWarning("qt elapsed timers aren't monotonic, boo");

	qInfo("Create HciSocket");
	m_hciSocket = HciSocket::create(0, -1);
	if (!m_hciSocket || !m_hciSocket->isValid()) {
		qError("failed to setup hci socket to hci%u", 0);
	}

	// create a dbus service watcher object so we can detect if the bluez daemon
	// (bluetoothd) falls off the bus or arrives back on the bus
	m_bluezServiceWatcher =
			QSharedPointer<QDBusServiceWatcher>::create(m_bluezService,
			                                            m_bluezDBusConn);


	// connect to the added / remove service signals
	QObject::connect(m_bluezServiceWatcher.data(), &QDBusServiceWatcher::serviceRegistered,
	                 this, &BleRcuAdapterBluez::onBluezServiceRegistered,
	                 Qt::QueuedConnection);
	QObject::connect(m_bluezServiceWatcher.data(), &QDBusServiceWatcher::serviceUnregistered,
	                 this, &BleRcuAdapterBluez::onBluezServiceUnregistered,
	                 Qt::QueuedConnection);


	// initialise and start the state machine
	initStateMachine();


	// also listen for any recovery events, these are just requests that any of
	// the code can trigger and should be last resort events when we think
	// that something is broken and needs resetting.
	QObject::connect(bleRcuRecovery, &BleRcuRecovery::powerCycleAdapter,
	                 this, &BleRcuAdapterBluez::onPowerCycleAdapter,
	                 Qt::QueuedConnection);
	QObject::connect(bleRcuRecovery, &BleRcuRecovery::reconnectDevice,
	                 this, &BleRcuAdapterBluez::onDisconnectReconnectDevice,
	                 Qt::QueuedConnection);


	// the bluetoothd daemon can get commands backed up during pairing which
	// means that Discovery Start / Stop are not acted on for up to 30 seconds
	// which causes things like scans to start well after the pairing process
	// has finished.  This time is used to cancel discovery that may have
	// been left running
	m_discoveryWatchdog.setSingleShot(false);
	m_discoveryWatchdog.setInterval(5000);
	QObject::connect(&m_discoveryWatchdog, &QTimer::timeout,
	                 this, &BleRcuAdapterBluez::onDiscoveryWatchdog);
}

BleRcuAdapterBluez::~BleRcuAdapterBluez()
{
	if (m_stateMachine.isRunning()) {
		m_stateMachine.postEvent(ShutdownEvent);
		m_stateMachine.stop();
	}

	qInfo("BleRcuAdapterBluez shut down");
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Intialises and starts the state machine for managing the bluez service and
	adapter proxy.

 */
void BleRcuAdapterBluez::initStateMachine()
{
	// set the name of the statemachine for logging
	m_stateMachine.setObjectName(QStringLiteral("BleRcuAdapterBluez"));

	// add all the states
	m_stateMachine.addState(ServiceUnavailableState, QStringLiteral("ServiceUnavailableState"));
	m_stateMachine.addState(ServiceAvailableSuperState, QStringLiteral("ServiceAvailableSuperState"));

	m_stateMachine.addState(ServiceAvailableSuperState, AdapterUnavailableState, QStringLiteral("AdapterUnavailableState"));
	m_stateMachine.addState(ServiceAvailableSuperState, AdapterAvailableSuperState, QStringLiteral("AdapterAvailableSuperState"));

	m_stateMachine.addState(AdapterAvailableSuperState, AdapterPoweredOffState, QStringLiteral("AdapterPoweredOffState"));
	m_stateMachine.addState(AdapterAvailableSuperState, AdapterPoweredOnState, QStringLiteral("AdapterPoweredOnState"));

	m_stateMachine.addState(ShutdownState, QStringLiteral("ShutdownState"));


	// add the transitions       From State	              ->    Event                  ->  To State
	m_stateMachine.addTransition(ServiceUnavailableState,       ServiceAvailableEvent,      AdapterUnavailableState);
	m_stateMachine.addTransition(ServiceUnavailableState,       ServiceRetryEvent,          ServiceUnavailableState);
	m_stateMachine.addTransition(ServiceAvailableSuperState,    ServiceUnavailableEvent,    ServiceUnavailableState);
	m_stateMachine.addTransition(ServiceAvailableSuperState,    ShutdownEvent,              ShutdownState);

	m_stateMachine.addTransition(AdapterUnavailableState,       AdapterAvailableEvent,      AdapterPoweredOffState);
	m_stateMachine.addTransition(AdapterUnavailableState,       AdapterRetryAttachEvent,    AdapterUnavailableState);
	m_stateMachine.addTransition(AdapterAvailableSuperState,    AdapterUnavailableEvent,    AdapterUnavailableState);

	m_stateMachine.addTransition(AdapterPoweredOffState,        AdapterPoweredOnEvent,      AdapterPoweredOnState);
	m_stateMachine.addTransition(AdapterPoweredOffState,        AdapterRetryPowerOnEvent,   AdapterPoweredOffState);
	m_stateMachine.addTransition(AdapterPoweredOnState,         AdapterPoweredOffEvent,     AdapterPoweredOffState);


	// connect to the state entry and exit signals
	QObject::connect(&m_stateMachine, &StateMachine::entered,
	                 this, &BleRcuAdapterBluez::onStateEntry);
	QObject::connect(&m_stateMachine, &StateMachine::exited,
	                 this, &BleRcuAdapterBluez::onStateExit);


	// set the initial state
	m_stateMachine.setInitialState(ServiceUnavailableState);
	m_stateMachine.start();
}

// -----------------------------------------------------------------------------
/*!
	\internal

 */
void BleRcuAdapterBluez::onStateEntry(int state)
{
	switch (State(state)) {
		case ServiceUnavailableState:
			onEnteredServiceUnavailableState();
			break;
		case AdapterUnavailableState:
			onEnteredAdapterUnavailableState();
			break;
		case AdapterPoweredOffState:
			onEnteredAdapterPoweredOffState();
			break;
		case AdapterPoweredOnState:
			onEnteredAdapterPoweredOnState();
			break;
		default:
			break;
	}
}

// -----------------------------------------------------------------------------
/*!
	\internal

 */
void BleRcuAdapterBluez::onStateExit(int state)
{
	switch (State(state)) {
		case ServiceAvailableSuperState:
			onExitedServiceAvailableSuperState();
			break;
		case AdapterAvailableSuperState:
			onExitedAdapterAvailableSuperState();
			break;
		case AdapterPoweredOnState:
			onExitedAdapterPoweredOnState();
			break;
		default:
			break;
	}
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Called on entry (or re-entry) to the 'Service Unavailable State', we just
	check if the service is still unavailable.

 */
void BleRcuAdapterBluez::onEnteredServiceUnavailableState()
{
	// cancel any pending delayed retry event
	if (m_retryEventId >= 0) {
		m_stateMachine.cancelDelayedEvent(m_retryEventId);
		m_retryEventId = -1;
	}


	// check if the service proxy object is valid
	if (!m_bluezObjectMgr || !m_bluezObjectMgr->isValid()) {

		// check if the dbus service is currently registered
		QDBusConnectionInterface *interface = m_bluezDBusConn.interface();
		if (interface == nullptr) {

			// the interface will be null if we aren't connected to the dbus
			// daemon, i.e. using QDBusServer locally, which is the case for
			// unit tests
			m_bluezObjectMgr =
				QSharedPointer<DBusObjectManagerInterface>::create(m_bluezService, "/", m_bluezDBusConn);
			if (!m_bluezObjectMgr->isValid()) {
				qError() << "failed to create adapter object manager proxy,"
				         << " due to" << m_bluezObjectMgr->lastError();
				m_bluezObjectMgr.reset();
			}

		} else {

			// we have an interface to the server so check if the bluez adapter
			// is registered
			QDBusReply<bool> reply = interface->isServiceRegistered(m_bluezService);
			if (!reply.isValid()) {
				qCritical() << "error reply in request to check bluez service availability"
				            << reply.error();

			} else if (reply.value() == false) {
				qWarning() << m_bluezService << "is still not registered on the bus";

			} else {

				// create a proxy to the 'org.freedesktop.DBus.ObjectManager'
				// interface on the bluez service / object
				m_bluezObjectMgr =
					QSharedPointer<DBusObjectManagerInterface>::create(m_bluezService, "/", m_bluezDBusConn);
				if (!m_bluezObjectMgr->isValid()) {
					qError() << "failed to create adapter object manager proxy,"
					         << " due to" << m_bluezObjectMgr->lastError();
					m_bluezObjectMgr.reset();
				}

			}
		}


		// install handlers for interfaces added / removed notifications
		if (m_bluezObjectMgr) {
			QObject::connect(m_bluezObjectMgr.data(), &DBusObjectManagerInterface::InterfacesAdded,
			                 this, &BleRcuAdapterBluez::onBluezInterfacesAdded);
			QObject::connect(m_bluezObjectMgr.data(), &DBusObjectManagerInterface::InterfacesRemoved,
			                 this, &BleRcuAdapterBluez::onBluezInterfacesRemoved);
		}

	}


	// check once again if the proxy is valid
	if (m_bluezObjectMgr && m_bluezObjectMgr->isValid())
		m_stateMachine.postEvent(ServiceAvailableEvent);
	else
		m_retryEventId = m_stateMachine.postDelayedEvent(ServiceRetryEvent, 1000);
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Called on exit from the 'Service Available Super State', this means that the
	bluez daemon has fallen off the bus and therefore we need to clear the
	dbus proxy object we are using to talk to the daemon as it's now defunct.

 */
void BleRcuAdapterBluez::onExitedServiceAvailableSuperState()
{
	m_bluezObjectMgr.reset();
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Called on entry (or re-entry) to the 'Adapter Unavailable State', we just
	check if the adapter is still unavailable.

	We may re-enter this state when the retry timer expires.

 */
void BleRcuAdapterBluez::onEnteredAdapterUnavailableState()
{
	// cancel any pending delayed retry event
	if (m_retryEventId >= 0) {
		m_stateMachine.cancelDelayedEvent(m_retryEventId);
		m_retryEventId = -1;
	}


	// try and find the first adapter (there should only be one)
	if (m_adapterObjectPath.path().isEmpty()) {

		m_adapterObjectPath = findAdapter();
		if (m_adapterObjectPath.path().isEmpty()) {
			qError("failed to find the bluez adapter object, is the bluetoothd "
			       "daemon running?");

			m_retryEventId = m_stateMachine.postDelayedEvent(AdapterRetryAttachEvent, 1000);
			return;
		}
	}

	// we need to attach two dbus proxy interfaces to the adapter object;
	//   org.bluez.Adapter1                  - for the bluez events
	//   org.freedesktop.DBus.ObjectManager  - for notification on device add / remove
	if (!m_adapterProxy) {

		if (!attachAdapter(m_adapterObjectPath)) {
			qError("failed to create proxies to the bluez adapter object");

			m_retryEventId = m_stateMachine.postDelayedEvent(AdapterRetryAttachEvent, 1000);
			return;
		}
	}

	m_stateMachine.postEvent(AdapterAvailableEvent);
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Called on exit from the 'Adapter Available Super State', this should only
	happen at shutdown or if the daemon falls of the bus.  In theory it could
	happen if the bluetooth adapter attached to the STB disappears, but since
	it is fixed to the board this shouldn't ever happen.

	Anyway when exiting this state for whatever reason we clear the list of
	ble device objects where holding and reset our dbus proxy interface to the
	adapter as it's now defunct.

 */
void BleRcuAdapterBluez::onExitedAdapterAvailableSuperState()
{
	// remove all the devices and emit events from them
	QMap<BleAddress, QSharedPointer<BleRcuDeviceBluez>>::iterator it = m_devices.begin();
	while (it != m_devices.end()) {

		// get the BDADDR of the device we're removing
		const BleAddress bdaddr = it.key();

		// remove the device from the map and send a signal saying the device
		// has disappeared
		it = m_devices.erase(it);

		// TODO: check replacement
		// emit deviceRemoved(bdaddr);
		emit devicePairingChanged(bdaddr, false, BleRcuAdapter::privateSignal());
	}

	m_adapterObjectPath = QDBusObjectPath();
	m_adapterProxy.reset();
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Called on entry (or re-entry) to the 'Powered Off State', we just check if
	the adapter is still un-powered and if so send a request to the daemon to
	power it on.

	The power on request is asynchronous; we wait for the power property change
	notification to get out of this state.

	We may re-enter this state when the retry timer expires.

 */
void BleRcuAdapterBluez::onEnteredAdapterPoweredOffState()
{
	// cancel any pending delayed retry event
	if (m_retryEventId >= 0) {
		m_stateMachine.cancelDelayedEvent(m_retryEventId);
		m_retryEventId = -1;
	}

	// skip out early if already powered
	if (m_adapterProxy->powered()) {
		m_stateMachine.postEvent(AdapterPoweredOnEvent);
		return;
	}

	qMilestone("adapter is not powered, attempting to power on now");


	// nb: we don't call setPowered() directly as that can block for up to 10
	// seconds, it is one of the few properties that do - normally it's only
	// methods that block
	QDBusPendingReply<> reply =
		m_adapterProxy->asyncSetProperty("Powered", QVariant::fromValue<bool>(true));

	// connect up the reply to a slot just so we can log any errors
	QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(reply, this);

	// install a slot on the completion of the request, we only do this to
	// catch errors and abort the coupling process
	QObject::connect(watcher, &QDBusPendingCallWatcher::finished,
	                 this, &BleRcuAdapterBluez::onPowerOnReply);


	// post an event so we retry power on again in 10 seconds time if we don't
	// get an acknowledgement
	m_retryEventId = m_stateMachine.postDelayedEvent(AdapterRetryPowerOnEvent, 10000);

}

// -----------------------------------------------------------------------------
/*!
	\internal

	Slot called with the reply to the request to change the 'Powered' property
	to true. This slot is installed just for logging errors it doesn't affect
	the state machine.

 */
void BleRcuAdapterBluez::onPowerOnReply(QDBusPendingCallWatcher *call)
{
	// sanity check
	if (Q_UNLIKELY(call == nullptr)) {
		qWarning("missing dbus watcher call in slot");
		return;
	}

	// check for error
	QDBusPendingReply<> reply = *call;
	if (Q_UNLIKELY(reply.isError())) {
		QDBusError error = reply.error();
		qError() << "power on request failed with error" << error;
	} else {
		qDebug("power on request successful");
	}

	// clean up the pending call
	call->deleteLater();
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Called on entry to the 'Powered On State', this is the final setup state and
	it's expected to be the steady state.

	When entering this state we reset the scan filter for just BLE devices,
	then disable pairable mode (this may affect BT audio device pairing if
	active, but very slight corner case), before finally get a list of devices
	that are currently attached to the adapter.

 */
void BleRcuAdapterBluez::onEnteredAdapterPoweredOnState()
{
	// cancel any pending delayed retry event
	if (m_retryEventId >= 0) {
		m_stateMachine.cancelDelayedEvent(m_retryEventId);
		m_retryEventId = -1;
	}


	// check if the adapter is already in discovery mode (really shouldn't be)
	// and stop it if so and then sets the discovery filter for BT LE
	if (!setAdapterDiscoveryFilter()) {
		qError("failed to configure discovery filter");
		// not fatal
	}

	// disable the pairable flag on the adapter
	disablePairable();


	// signal the power state change before iterating and adding any devices
	emit poweredChanged(true, BleRcuAdapter::privateSignal());


	// finally get a list of currently registered devices (RCUs)
	getRegisteredDevices();

	// signal that the adapter is powered and we got the list of paired devices
	emit poweredInitialised(BleRcuAdapter::privateSignal());
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Called when leaving the 'Powered On State', implemented to simply emit a
	signal on the power change.

 */
void BleRcuAdapterBluez::onExitedAdapterPoweredOnState()
{
	emit poweredChanged(false, BleRcuAdapter::privateSignal());
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Slot called when dbus tells us that the bluez service (org.bluez) has
	been registered once again on the bus.  We don't immediately try and
	connect as typically you have a race where the service is registered
	first followed by the objects and interfaces.  So instead we trigger the
	\l{onBluezServiceRetryTimer} to check again in one second time.

 */
void BleRcuAdapterBluez::onBluezServiceRegistered(const QString &serviceName)
{
	if (Q_UNLIKELY(serviceName != m_bluezService))
		return;

	qMilestone("detected bluez service registration, will retry connecting in 1s");

	m_stateMachine.postDelayedEvent(ServiceRetryEvent, 1000);
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Slot called when dbus tells us that the bluez service (org.bluez) has
	been un-registered from the bus.  This triggers us to clear the availablity
	flag and remove all devices.

 */
void BleRcuAdapterBluez::onBluezServiceUnregistered(const QString &serviceName)
{
	if (Q_UNLIKELY(serviceName != m_bluezService))
		return;

	qError("detected bluez service has dropped off the dbus, has it crashed?");

	m_stateMachine.postEvent(ServiceUnavailableEvent);
}

// -----------------------------------------------------------------------------
/*!
	Returns \c true if the manager has been correctly constructed and managed
	to connect to the blue interface.

 */
bool BleRcuAdapterBluez::isValid() const
{
	return true;
}

// -----------------------------------------------------------------------------
/*!
	Returns \c true if the manager has been correctly constructed and managed
	to connect to the blue interface.

 */
bool BleRcuAdapterBluez::isAvailable() const
{
	return m_stateMachine.inState(AdapterAvailableSuperState);
}

// -----------------------------------------------------------------------------
/*!
	Returns \c true if the manager has been correctly constructed and managed
	to power it on.

 */
bool BleRcuAdapterBluez::isPowered() const
{
	return m_stateMachine.inState(AdapterPoweredOnState);
}

// -----------------------------------------------------------------------------
/*!
	Called by the system when someone invokes dumpsys on this service.

 */
void BleRcuAdapterBluez::dump(Dumper out) const
{
	out.printLine("stack:     bluez");
	out.printLine("address:   %s", qPrintable(m_address.toString()));
	out.printBoolean("available: ", isAvailable());
	out.printBoolean("powered:   ", isPowered());
	out.printBoolean("scanning:  ", m_discovering);
	out.printBoolean("pairable:  ", m_pairable);
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Attempts to find the first bluetooth (HCI) adapter

	This first gets a list of all the managed objects on the 'org.bluez'
	service. Then iterates through the objects trying to find an object that
	has the 'org.bluez.Adapter1' service.  We assume there is only one bluetooth
	adapter so we use the first one found.

	Nb: I found it helps to run dbus-monitor while running the bluez example
	python scripts to figure out what to expect

 */
QDBusObjectPath BleRcuAdapterBluez::findAdapter(int timeout)
{
	// sanity check the dbus connection
	if (!m_bluezDBusConn.isConnected()) {
		qWarning() << "dbus connection not valid";
		return QDBusObjectPath();
	}

	// temporarily set a shorter timeout
	m_bluezObjectMgr->setTimeout(timeout);

	// get all the managed objects on the remote 'org.bluez' object
	QDBusPendingReply<DBusManagedObjectList> reply = m_bluezObjectMgr->GetManagedObjects();
	reply.waitForFinished();

	// restore the default timeout
	m_bluezObjectMgr->setTimeout(-1);

	// check for an error
	if (reply.isError()) {
		qWarning() << "failed to get managed object due to" << reply.error();
		return QDBusObjectPath();
	}

	const DBusManagedObjectList objects = reply.value();

	DBusManagedObjectList::const_iterator object = objects.begin();
	for (; object != objects.end(); ++object) {

		// get the object path and interfaces
		const QDBusObjectPath &path = object.key();
		const DBusInterfaceList &interfaces = object.value();

		DBusInterfaceList::const_iterator interface = interfaces.begin();
		for (; interface != interfaces.end(); ++interface) {

			// get the interface name and properties
			const QString &name = interface.key();
			const QVariantMap &properties = interface.value();

			// if this object has an adapter interface then it's one for us
			if (name == BluezAdapterInterface::staticInterfaceName()) {

				const QVariant addressValue = properties.value("Address");
				m_address = BleAddress(addressValue.toString());

				qMilestone() << "found bluez adapter at" << path.path()
				             << "with address" << m_address;

				// use the supplied properties to set the initial discovery
				// and pairable states
				if (properties.contains("Discovering"))
					m_discovering = properties["Discovering"].toBool();

				if (properties.contains("Pairable"))
					m_pairable = properties["Pairable"].toBool();

				return path;
			}
		}
	}

	return QDBusObjectPath();
}

// -----------------------------------------------------------------------------
/*!
	\internal

	This creates dbus proxy interface objects to communicate with the
	adapter interface on the bluez daemon.

	We attach to two interfaces on the daemon, the first is 'org.bluez.Adapter1'
	which is the one used to control / monitor the adapter (i.e. things like
	dicovery start/stop, power on/off, etc).

	The second interface is 'org.freedesktop.DBus.ObjectManager' this is used
	to notify us when devices are added and removed from the adapter.

	Once the proxy objects are created, then we attach all the signals to our
	internal slots.

 */
bool BleRcuAdapterBluez::attachAdapter(const QDBusObjectPath &adapterPath)
{
	// create a proxy to the 'org.bluez.Adapter1' interface on the adapter object
	m_adapterProxy = QSharedPointer<BluezAdapterInterface>::create(m_bluezService,
	                                                               adapterPath.path(),
	                                                               m_bluezDBusConn);
	if (!m_adapterProxy || !m_adapterProxy->isValid()) {
		qWarning() << "failed to create adapter proxy";
		return false;
	}

	// install handlers for the interesting adapter notifications
	QObject::connect(m_adapterProxy.data(), &BluezAdapterInterface::discoveringChanged,
	                 this, &BleRcuAdapterBluez::onAdapterDiscoveringChanged);
	QObject::connect(m_adapterProxy.data(), &BluezAdapterInterface::pairableChanged,
	                 this, &BleRcuAdapterBluez::onAdapterPairableChanged);
	QObject::connect(m_adapterProxy.data(), &BluezAdapterInterface::poweredChanged,
	                 this, &BleRcuAdapterBluez::onAdapterPowerChanged);

	return true;
}

// -----------------------------------------------------------------------------
/*!
	First checks if the adapter is already in discovery mode, if so it is
	cancelled, then it attempts to set the discovery filter so we only get
	bluetooth LE devices in the scan results.

 */
bool BleRcuAdapterBluez::setAdapterDiscoveryFilter()
{
	// get and store the current discovery state
	m_discovering = m_adapterProxy->discovering();

	// check if the adapter is currently in discovery mode, stop it if so
	if (Q_UNLIKELY(m_discovering)) {

		QDBusPendingReply<> reply = m_adapterProxy->StopDiscovery();
		reply.waitForFinished();

		if (reply.isError()) {
			qError() << "failed to stop discovery due to" << reply.error();
			// not fatal, fall through
		}
	}

	// attempt to set the filter to LE
	QVariantMap properties;
	properties[QStringLiteral("Transport")] = QStringLiteral("le");

	QDBusPendingReply<> reply = m_adapterProxy->SetDiscoveryFilter(properties);
	reply.waitForFinished();

	if (reply.isError()) {
		qError() << "failed to set discovery filter due to" << reply.error();
		return false;
	}

	return true;
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Called at start-up to get the list of devices already available in the
	bluez daemon.

	For each device found we call addDevice().

 */
void BleRcuAdapterBluez::getRegisteredDevices()
{
	// get all the managed objects on the remote 'org.bluez' object
	QDBusPendingReply<DBusManagedObjectList> reply = m_bluezObjectMgr->GetManagedObjects();
	reply.waitForFinished();

	// check for an error
	if (reply.isError()) {
		qWarning() << "failed to get managed object due to" << reply.error();
		return;
	}

	const DBusManagedObjectList objects = reply.value();

	// find an object with all the following interfaces
	DBusManagedObjectList::const_iterator object = objects.begin();
	for (; object != objects.end(); ++object) {

		// get the object path and interfaces
		const QDBusObjectPath &path = object.key();
		const DBusInterfaceList &interfaces = object.value();

		DBusInterfaceList::const_iterator interface = interfaces.begin();
		for (; interface != interfaces.end(); ++interface) {

			// get the interface name and properties
			const QString &name = interface.key();
			const QVariantMap &properties = interface.value();

			// if this object has an 'org.bluez.Device1' interface then attempt
			// to add the device
			if (name == BluezDeviceInterface::staticInterfaceName()) {

				// add the device to our internal map
				onDeviceAdded(path, properties);
			}
		}
	}

}

// -----------------------------------------------------------------------------
/*!
	\internal

	Slot called when the power changed property on the adapter is signalled. We
	hook this notification to detect if anyone has powered down the adapter,
	which would be fatal for the RCU.

 */
void BleRcuAdapterBluez::onAdapterPowerChanged(bool powered)
{
	if (powered) {
		qMilestone("BT adapter powered on");
		m_stateMachine.postDelayedEvent(AdapterPoweredOnEvent, 10);

	} else {
		qMilestone("odd, someone has powered down the BT adapter unexpectedly");
		m_stateMachine.postDelayedEvent(AdapterPoweredOffEvent, 100);

	}
}

// -----------------------------------------------------------------------------
/*!
	\fn bool BleRcuManager::isDiscovering()

	Returns \c true if the bluetooth adapter currently has discovery (scanning)
	enabled.

	\sa startDiscovery(), stopDiscovery(), discoveryChanged()
 */
bool BleRcuAdapterBluez::isDiscovering() const
{
	return m_discovering;
}

// -----------------------------------------------------------------------------
/*!
	\fn bool BleRcuManager::startDiscovery()

	Sends a dbus request to the bluez daemon to start discovery (aka scanning).
	The request is sent regardless of internal cached state, therefore an
	'operation already progress' error may be logged if trying to start discovery
	whilst already running.

	This request is asynchronous, to check it succeeded you should monitor the
	discoveryChanged() signal.

	\sa isDiscovering(), stopDiscovery(), discoveryChanged()
 */
bool BleRcuAdapterBluez::startDiscovery(int pairingCode)
{
	// on bluez we don't use the pairing code to filter the scan results, so
	// ignore the value
	Q_UNUSED(pairingCode);

	if (Q_UNLIKELY(!m_stateMachine.inState(AdapterPoweredOnState))) {
		qError("adapter not powered, can't start discovery");
		return false;
	}

	// set the expected discovery state for the watchdog
	m_discoveryRequested = StartDiscovery;

	// if not discovery don't send a request
	if (m_discovering)
		return true;

	// reset the discovery watchdog and increment the discovery pending count
	m_discoveryRequests++;
	qDebug("starting discoveryWatchdog, m_discoveryRequests = %d", m_discoveryRequests);
	m_discoveryWatchdog.start();

	// otherwise send the request to start discovery
	QDBusPendingReply<> reply = m_adapterProxy->StartDiscovery();
	QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(reply, this);

	// install a slot on the completion of the request, we only do this to
	// catch errors and abort the coupling process
	QObject::connect(watcher, &QDBusPendingCallWatcher::finished,
	                 this, &BleRcuAdapterBluez::onStartDiscoveryReply);

	return true;
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Slot called with the reply to the request to start discovery. This slot is
	installed just for logging errors.

	\sa startDiscovery()
 */
void BleRcuAdapterBluez::onStartDiscoveryReply(QDBusPendingCallWatcher *call)
{
	// sanity check
	if (Q_UNLIKELY(call == nullptr)) {
		qWarning("missing dbus watcher call in slot");
		return;
	}

	// reset the discovery watchdog and decrement the discovery pending count
	m_discoveryWatchdog.start();
	m_discoveryRequests--;
	qDebug("starting discoveryWatchdog, m_discoveryRequests = %d", m_discoveryRequests);
	if(m_discoveryRequests == 0) {
		qDebug("there is no outstanding discovery requests, let's stop discoveryWatchdog");
		m_discoveryWatchdog.stop();
	}

	// check for error
	QDBusPendingReply<> reply = *call;
	if (Q_UNLIKELY(reply.isError())) {
		QDBusError error = reply.error();
		qError() << "discovery start request failed with error" << error;
	} else {
		qDebug("discovery start request successful");
	}

	// clean up the pending call
	call->deleteLater();
}

// -----------------------------------------------------------------------------
/*!
	\fn bool BleRcuAdapter::stopDiscovery()

	Sends a dbus request to the bluez daemon to stop discovery (aka scanning).
	The request is sent regardless of internal cached state, therefore an
	'operation not running' error may be logged if trying to stop discovery
	when it's not already running.

	This request is asynchronous, to check it succeeded you should monitor the
	discoveryChanged() signal.

	\sa startDiscovery(), isDiscovering(), discoveryChanged()
 */
bool BleRcuAdapterBluez::stopDiscovery()
{
	if (Q_UNLIKELY(!m_stateMachine.inState(AdapterPoweredOnState)))
		return false;

	// set the expected discovery state for the watchdog
	m_discoveryRequested = StopDiscovery;

	// regardless of whether we think we are in the discovery mode or not
	// send the request to stop, this is a workaround for a bluetoothd issue
	// where it gets stuck in the 'starting' phase

	// reset the discovery watchdog and increment the discovery pending count
	m_discoveryRequests++;
	qDebug("starting discoveryWatchdog, m_discoveryRequests = %d", m_discoveryRequests);
	m_discoveryWatchdog.start();

	// send the request to stop discovery
	QDBusPendingReply<> reply = m_adapterProxy->StopDiscovery();
	QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(reply, this);

	// install a slot on the completion of the request, we only do this to
	// catch errors and abort the coupling process
	QObject::connect(watcher, &QDBusPendingCallWatcher::finished,
	                 this, &BleRcuAdapterBluez::onStopDiscoveryReply);

	return true;
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Slot called with the reply to the request to stop discovery. This slot is
	installed just for logging errors.

	\sa stopDiscovery()
 */
void BleRcuAdapterBluez::onStopDiscoveryReply(QDBusPendingCallWatcher *call)
{
	// sanity check
	if (Q_UNLIKELY(call == nullptr)) {
		qWarning("missing dbus watcher call in slot");
		return;
	}

	// reset the discovery watchdog and decrement the discovery pending count
	m_discoveryWatchdog.start();
	m_discoveryRequests--;
	qDebug("starting discoveryWatchdog, m_discoveryRequests = %d", m_discoveryRequests);
	if(m_discoveryRequests == 0) {
		qDebug("there is no outstanding discovery requests, let's stop discoveryWatchdog");
		m_discoveryWatchdog.stop();
	}

	// check for error
	QDBusPendingReply<> reply = *call;
	if (Q_UNLIKELY(reply.isError())) {
		QDBusError error = reply.error();
		qError() << "discovery stop request failed with error" << error;
	} else {
		qDebug("discovery stop request successful");
	}

	// clean up the pending call
	call->deleteLater();
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Slot called periodically to check that the current discovery / scan state
	matches what has been requested.  This is to work around issues in the
	bluetoothd daemon that causes Discovery Start / Stop requests to be delayed
	for a long time.

 */
void BleRcuAdapterBluez::onDiscoveryWatchdog()
{
	// wait for any outstanding requests to finish
	if (m_discoveryRequests > 0)
		return;

	// check if the current discovery mode is in the correct state
	const bool requestedMode = (m_discoveryRequested == StartDiscovery);
	if (m_discovering != requestedMode) {

		qError("detected discovery in the wrong state (expected:%s actual:%s)",
		       (m_discoveryRequested == StartDiscovery) ? "on" : "off",
		       m_discovering ? "on" : "off");

		// in the wrong state so Start / Stop discovery
		if (m_discoveryRequested == StartDiscovery)
			startDiscovery(-1);
		else
			stopDiscovery();
	}
}

// -----------------------------------------------------------------------------
/*!
	\fn bool BleRcuManager::isPairable()

	Returns \c true if the bluetooth adapter currently has pairable enabled.

	\sa disablePairable(), enablePairable(), pairableChanged()
 */
bool BleRcuAdapterBluez::isPairable() const
{
	return m_pairable;
}

// -----------------------------------------------------------------------------
/*!
	\fn bool BleRcuManager::enablePairable()

	Enables the pairable flag on the adapter with the given \a timeout in
	milliseconds.

	This method sends two dbus requests to the bluez daemon to first set the
	pairable timeout and then to set the pairable property to \c true.

	\sa disablePairable(), isPairable(), pairableChanged()
 */
bool BleRcuAdapterBluez::enablePairable(int timeout)
{
	if (Q_UNLIKELY(!m_stateMachine.inState(AdapterPoweredOnState)))
		return false;

	qInfo("enabling pairable mode for %d seconds", timeout / 1000);

	// TODO: should we switch to non-blocking propery write ?
	m_adapterProxy->setPairableTimeout(static_cast<quint32>(timeout / 1000));
	m_adapterProxy->setPairable(true);

	return true;
}

// -----------------------------------------------------------------------------
/*!
	\fn bool BleRcuManager::disablePairable()

	Disables the pairable flag on the adapter.

	This method sends a dbus request to the bluez daemon to set the pairable
	property to \c false.

	\sa enablePairable(), isPairable(), pairableChanged()
 */
bool BleRcuAdapterBluez::disablePairable()
{
	if (Q_UNLIKELY(!m_stateMachine.inState(AdapterPoweredOnState)))
		return false;

	// if any of our devices are in the pairing state then cancel it
	QMap<BleAddress, QSharedPointer<BleRcuDeviceBluez>>::const_iterator it =
			m_devices.begin();
	for (; it != m_devices.end(); ++it) {
		const QSharedPointer<BleRcuDeviceBluez> &device = it.value();
		if (device->isPairing())
			device->cancelPairing();
	}

	qInfo("disabling pairable mode");

	// TODO: should we switch to non-blocking property write ?
	m_adapterProxy->setPairable(false);

	return true;
}

// -----------------------------------------------------------------------------
/*!
	\fn bool BleRcuManager::getDevice()

	Returns a shared pointer to the device with the given \a address. If the
	device is unknown then an empty / invalid shared pointer is returned.

	\sa deviceNames()
 */
QSharedPointer<BleRcuDevice> BleRcuAdapterBluez::getDevice(const BleAddress &address) const
{
	const QSharedPointer<BleRcuDeviceBluez> device = m_devices.value(address);
	if (!device || !device->isValid()) {
		qInfo() << "failed to find device with address" << address;
		return QSharedPointer<BleRcuDevice>();
	}

	return qSharedPointerCast<BleRcuDevice>(device);
}

// -----------------------------------------------------------------------------
/*!
	\fn bool BleRcuManager::pairedDevices()

	Returns the set of all currently paired devices.

	\sa deviceNames(), getDevice()
 */
QSet<BleAddress> BleRcuAdapterBluez::pairedDevices() const
{
	QSet<BleAddress> paired;

	QMap<BleAddress, QSharedPointer<BleRcuDeviceBluez>>::const_iterator it = m_devices.begin();
	for (; it != m_devices.end(); ++it) {

		const QSharedPointer<BleRcuDeviceBluez> &device = it.value();
		if (device && device->isValid() && device->isPaired())
			paired.insert(it.key());

	}

	return paired;
}

// -----------------------------------------------------------------------------
/*!
	\fn bool BleRcuManager::deviceNames()

	Returns map of device names stored against their address.

	\sa pairedDevices(), getDevice()
 */
QMap<BleAddress, QString> BleRcuAdapterBluez::deviceNames() const
{
	QMap<BleAddress, QString> namesMap;

	QMap<BleAddress, QSharedPointer<BleRcuDeviceBluez>>::const_iterator it = m_devices.begin();
	for (; it != m_devices.end(); ++it) {

		const QSharedPointer<BleRcuDeviceBluez> &device = it.value();
		if (device && device->isValid()) {
			namesMap.insert(it.key(), device->name());
		}
	}

	return namesMap;
}

// -----------------------------------------------------------------------------
/*!
	\fn void BleRcuManager::isDevicePaired(const BleAddress &address)

	Returns \c true if the device with the given \a address is paired.  This
	request works on cached values, it's possible it may not be accurate if
	an unpair notification is yet to arrive over dbus.

	\sa pairDevice(), removeDevice()
 */
bool BleRcuAdapterBluez::isDevicePaired(const BleAddress &address) const
{
	const QSharedPointer<BleRcuDeviceBluez> device = m_devices.value(address);
	if (!device || !device->isValid()) {
		qInfo() << "failed to find device with address" << address
		        << "to query paired status";
		return false;
	}

	return device->isPaired();
}

// -----------------------------------------------------------------------------
/*!
	\fn void BleRcuManager::addDevice(const BleAddress &address)

	Sends a request to the bluez daemon to pair the device with the given
	\a address.  The request is sent even if the device is already paired,
	this is to handle the case where a pending unpair notification is sitting
	in the dbus queue but not yet processed.

	This request is asynchronous.

	\sa isDevicePaired(), removeDevice()
 */
bool BleRcuAdapterBluez::addDevice(const BleAddress &address)
{
	if (Q_UNLIKELY(!m_stateMachine.inState(AdapterPoweredOnState)))
		return false;

	const QSharedPointer<BleRcuDeviceBluez> device = m_devices.value(address);
	if (!device || !device->isValid()) {
		qInfo() << "failed to find device with address" << address << "to pair";
		return false;
	}

	qInfo() << "requesting bluez pair" << device->address();

	device->pair(0);

	return true;
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Called whenever the 'discovering' state changes on the bluetooth adapter.
	This is used to move the state machine on, or report an error if this
	occurs in the wrong state.

	This is a callback from the dbus notification of the discovering change.

 */
void BleRcuAdapterBluez::onAdapterDiscoveringChanged(bool discovering)
{
	qInfo() << "adapter" << (discovering ? "started" : "stopped") << "discovering";

	// skip out early if nothings actually changed
	if (Q_UNLIKELY(m_discovering == discovering))
		return;

	// set the new state then emit a signal
	m_discovering = discovering;
	emit discoveryChanged(m_discovering, BleRcuAdapter::privateSignal());
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Called whenever the 'pairable' state changes on the bluetooth adapter.

	This is a callback from the dbus notification of the pairable property
	change.

 */
void BleRcuAdapterBluez::onAdapterPairableChanged(bool pairable)
{
	qInfo() << "adapter pairable state changed to" << pairable;

	// skip out early if nothings actually changed
	if (Q_UNLIKELY(m_pairable == pairable))
		return;

	// set the new state then emit a signal
	m_pairable = pairable;
	emit pairableChanged(m_pairable, BleRcuAdapter::privateSignal());
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Called when a device is added to the bluez adapter interface.

	A device is typically added once discovery is started, when a device is
	added it is typically not paired or connected.

	The properties supplied should match all the values that can be queried /
	modified on the 'org.bluez.Device1' interface. For example the following
	is the properties dump taken from a dbus-monitor session.

	\code
		array [ dict entry( string "Address"
		                    variant string "60:03:08:CE:49:F3")
		        dict entry( string "Alias"
		                    variant string "60-03-08-CE-49-F3" )
		        dict entry( string "Paired"
		                    variant boolean false )
		        dict entry( string "Trusted"
		                    variant boolean false )
		        dict entry( string "Blocked"
		                    variant boolean false )
		        dict entry( string "LegacyPairing"
		                    variant boolean false )
		        dict entry( string "RSSI"
		                    variant int16 -77 )
		        dict entry( string "Connected"
		                    variant boolean false )
		        dict entry( string "UUIDs"
		                    variant array [ ] )
		        dict entry( string "Adapter"
		                    variant object path "/org/bluez/hci0" )
		 ]
	\endcode

	This function is called for all manor of devices, so to filter out only
	RCUs we use the BDADDR to match only ruwido remotes.

 */
void BleRcuAdapterBluez::onDeviceAdded(const QDBusObjectPath &path,
                                       const QVariantMap &properties)
{
	// it's unlikely but possible that we already have this device stored, as
	// this function may be called at start-up when we've queried the bluez
	// daemon but the signal handlers are also installed.  Anyway it just means
	// we should ignore this call, it's not an error
	QMap<BleAddress, QSharedPointer<BleRcuDeviceBluez>>::const_iterator it = m_devices.begin();
	for (; it != m_devices.end(); ++it) {
		const QSharedPointer<BleRcuDeviceBluez> &device = it.value();
		if (Q_UNLIKELY(device->bluezObjectPath() == path))
			return;
	}


	// we only require the "Adapter" and "Address" properties from the
	// notification, the rest of the details (like whether it's paired /
	// connected) are read by the newly created BleRcuDevice.


	// get the adapter path and verify the device is attached to our adapter
	// (in reality there should only be one adapter, but doesn't hurt to check)
	QVariantMap::const_iterator property = properties.find(QStringLiteral("Adapter"));
	if ((property == properties.end()) || !property.value().canConvert<QDBusObjectPath>()) {
		qWarning() << "property =" << property.value().type();
		qWarning() << "device 'Adapter' property is missing or invalid";
		return;
	}
	const QDBusObjectPath adapterPath = qvariant_cast<QDBusObjectPath>(property.value());

	if (adapterPath != m_adapterObjectPath) {
		qWarning() << "odd, the device added doesn't belong to our adapter";
		return;
	}


	// get the device address
	property = properties.find(QStringLiteral("Address"));
	if ((property == properties.end()) || (property.value().type() != QVariant::String)) {
		qWarning("device 'Address' property is missing or invalid");
		return;
	}

	// convert the address to a BDADDR object
	const BleAddress bdaddr(property.value().toString());
	if (bdaddr.isNull()) {
		qWarning() << "failed to parse the device address" << property.value();
		return;
	}

	// get the device name
	QString name;
	property = properties.find(QStringLiteral("Name"));
	if ((property == properties.end()) || (property.value().type() != QVariant::String))
		qInfo("device 'Name' property is missing or invalid");
	else
		name = property.value().toString();

	QVector<QRegExp>::const_iterator it_name = m_supportedPairingNames.begin();
	for (; it_name != m_supportedPairingNames.end(); ++it_name) {
		if (it_name->exactMatch(name)) {
			qInfo() << "found pairable device" << bdaddr
					<< "with name" << name;
			break;
		}
	}
	if (it_name == m_supportedPairingNames.end()) {
		// Name not found, see if OUI matches an entry in the map.
		if (!m_supportedOuis.contains(bdaddr.oui())) {
			// qDebug() << "device with address " << bdaddr << ", and name: " << name << " is not an RCU, so ignoring.";
			return;
		}
	}


	// get the connected and paired properties
	bool connected = false;
	property = properties.find(QStringLiteral("Connected"));
	if ((property == properties.end()) || (property.value().type() != QVariant::Bool))
		qInfo("device 'Connected' property is missing or invalid");
	else
		connected = property.value().toBool();

	bool paired = false;
	property = properties.find(QStringLiteral("Paired"));
	if ((property == properties.end()) || (property.value().type() != QVariant::Bool))
		qInfo("device 'Paired' property is missing or invalid");
	else
		paired = property.value().toBool();




	// so we now have all the params we need, create a device object to manage
	// the (RCU) device
	QSharedPointer<BleRcuDeviceBluez> device =
			QSharedPointer<BleRcuDeviceBluez>::create(bdaddr, name,
			                                          m_bluezDBusConn, path,
			                                          m_servicesFactory);
	if (!device || !device->isValid()) {
		qWarning() << "failed to create device with bdaddr" << bdaddr;
		return;
	}

	// connect up the signals from the device, we use functors to bind the
	// device bdaddr in with the slot callback
	std::function<void(const QString&)> nameChangedFunctor =
		std::bind(&BleRcuAdapterBluez::onDeviceNameChanged, this,
		          bdaddr, std::placeholders::_1);

	QObject::connect(device.data(), &BleRcuDevice::nameChanged,
	                 this, nameChangedFunctor);

	std::function<void(bool)> pairedChangedFunctor =
		std::bind(&BleRcuAdapterBluez::onDevicePairedChanged, this,
		          bdaddr, std::placeholders::_1);

	QObject::connect(device.data(), &BleRcuDevice::pairedChanged,
	                 this, pairedChangedFunctor);

	std::function<void(bool)> readyChangedFunctor =
		std::bind(&BleRcuAdapterBluez::onDeviceReadyChanged, this,
		          bdaddr, std::placeholders::_1);

	QObject::connect(device.data(), &BleRcuDevice::readyChanged,
	                 this, readyChangedFunctor);


	// add the device to the list
	m_devices.insert(bdaddr, device);

	qMilestone().nospace() << "added device " << bdaddr
	                       << " named " << name
	                       << " (connected: " << connected
	                       << " paired: " << paired << ")";


	emit deviceFound(device->address(), device->name(),
	                 BleRcuAdapter::privateSignal());
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Called when an object managed by bluez was added.

	The function checks that one of the interfaces added for the object is
	'org.bluez.Device1' and if so then it calls onDevicedAdded() to add the
	device to our internal map.

 */
void BleRcuAdapterBluez::onBluezInterfacesAdded(const QDBusObjectPath &objectPath,
                                                const DBusInterfaceList &interfacesAndProperties)
{
	// loop through the interfaces added
	DBusInterfaceList::const_iterator it = interfacesAndProperties.begin();
	for (; it != interfacesAndProperties.end(); ++it) {

		const QString &interface = it.key();
		const QVariantMap &properties = it.value();

		qDebug("received interface %ss added event", qPrintable(interface));

		// if the interface is 'org.bluez.Device1' add the device
		if (interface == BluezDeviceInterface::staticInterfaceName())
			onDeviceAdded(objectPath, properties);

		// if the interface is 'org.bluez.Adapter1' trigger a retry
		else if (interface == BluezAdapterInterface::staticInterfaceName())
			m_stateMachine.postDelayedEvent(AdapterRetryAttachEvent, 10);

	}
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Called when an object with interface org.bluez.Device1 was removed.

	We use the object path to find the device to remove, it's not an error if
	we don't have a device stored for the given path as we only store devices
	that have a BDADDR that matches an (ruwido) RCU.

 */
void BleRcuAdapterBluez::onDeviceRemoved(const QDBusObjectPath &objectPath)
{
	// check if we have an RCU device at the given dbus path
	QMap<BleAddress, QSharedPointer<BleRcuDeviceBluez>>::iterator it = m_devices.begin();
	for (; it != m_devices.end(); ++it) {
		const QSharedPointer<BleRcuDeviceBluez> &device = it.value();
		if (device->bluezObjectPath() == objectPath)
			break;
	}

	// if is not an error if the removed device is not in our map
	if (it == m_devices.end())
		return;


	// get the BDADDR of the device we're removing
	const BleAddress bdaddr = it.key();

	// check if it was paired
	const bool wasPaired = it.value()->isPaired();

	qMilestone() << "removed device" << bdaddr;

	// remove the device from the map and send a signal saying the device
	// has disappeared
	m_devices.erase(it);

	// if was paired then we clearly no longer are so emit a signal
	if (wasPaired)
		emit devicePairingChanged(bdaddr, false, BleRcuAdapter::privateSignal());

	emit deviceRemoved(bdaddr, BleRcuAdapter::privateSignal());
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Called when an object managed by bluez is removed.

	The function checks that one of the interfaces for the removed object is
	'org.bluez.Device1' and if so then it calls onDeviceRemoved() to remove the
	device from our internal map.
 */
void BleRcuAdapterBluez::onBluezInterfacesRemoved(const QDBusObjectPath &objectPath,
                                                  const QStringList &interfaces)
{
	qDebug() << "received interface(s) removed event";

	// check if it's the adapter that was removed (this should never really
	// happen) otherwise check if one of the interfaces being removed is
	// 'org.bluez.Device1'
	if (objectPath == m_adapterObjectPath)
		m_stateMachine.postDelayedEvent(AdapterUnavailableEvent, 10);

	else if (interfaces.contains(BluezDeviceInterface::staticInterfaceName()))
		onDeviceRemoved(objectPath);
}

// -----------------------------------------------------------------------------
/*!
	\fn void BleRcuManager::removeDevice(const BleAddress &address)

	Sends a request over dbus to the bluez daemon to remove the device, this
	has the effect of disconnecting and unpairing any device (if it was
	connected and paired).

	This request is asynchronous.

	\sa pairDevice(), isDevicePaired()
 */
bool BleRcuAdapterBluez::removeDevice(const BleAddress &address)
{
	if (Q_UNLIKELY(!m_stateMachine.inState(AdapterAvailableSuperState)))
		return false;

	// find the device with the address in our map, this is so we can cancel
	// pairing if currently in the pairing procedure
	const QSharedPointer<BleRcuDeviceBluez> device = m_devices.value(address);
	if (!device || !device->isValid()) {
		qInfo() << "failed to find device with address" << address << "to remove";
		return false;
	}

	qInfo() << "requesting bluez remove" << device->address();

	// if currently pairing then cancel it before removing the device
	if (device->isPairing())
		device->cancelPairing();


	// ask the adapter to remove this device since if failed pairing,
	// we add a listener on the result just so we can log any errors,
	// there is no
	QDBusPendingReply<> reply = m_adapterProxy->RemoveDevice(device->bluezObjectPath());
	QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(reply, this);

	QObject::connect(watcher, &QDBusPendingCallWatcher::finished,
	                 this, &BleRcuAdapterBluez::onRemoveDeviceReply);

	return true;
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Called when we get a response to our asynchronous request to remove a device
	from the adapter.  We only hook this point so we can log any errors.

	\see removeDevice()
 */
void BleRcuAdapterBluez::onRemoveDeviceReply(QDBusPendingCallWatcher *call)
{
	// check for error
	QDBusPendingReply<> reply = *call;
	if (Q_UNLIKELY(reply.isError())) {

		QDBusError error = reply.error();
		qError() << "remove device request failed with error" << error;

	} else {

		qDebug("remove device request successful");

	}

	// clean up the pending call
	call->deleteLater();
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Event signalled by an \l{BleRcuDevice} when it detects it's name has changed.

	We need this info for pairing as an already known device may change it's
	name when it enters pairing mode.
 */
void BleRcuAdapterBluez::onDeviceNameChanged(const BleAddress &address,
                                             const QString &name)
{
	qMilestone() << "renamed device" << address << "to" << name;

	emit deviceNameChanged(address, name, BleRcuAdapter::privateSignal());
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Event signalled by an \l{BleRcuDevice} when it detects it's 'paired' state
	has changed.

	Although this is not a necessity but helpful for the pairing state machine.
 */
void BleRcuAdapterBluez::onDevicePairedChanged(const BleAddress &address,
                                              bool paired)
{
	// nb: already logged as milestone in BleRcuDeviceImpl, don't log again

	emit devicePairingChanged(address, paired, BleRcuAdapter::privateSignal());
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Event signalled by a \t{BleRcuDevice} object when it's ready state has
	changed. The 'ready' state implies that the device is paired, connected, and
	has gone through the initial setup such that it is now ready.

	This notification is passed on to the coupling state machine if it's running
	and the device is now 'ready'.

	This is also the point were we check the number of devices we have in the
	'paired' state, if it exceeds the maximum allowed then we remove the last
	device to enter the 'ready' state.

 */
void BleRcuAdapterBluez::onDeviceReadyChanged(const BleAddress &address,
                                             bool ready)
{
	qMilestone() << "device with address" << address << "is"
	             << (ready ? "" : "not") << "ready";

	if (ready && m_devices.contains(address)) {
		if (m_devices[address]->name() == "Platco PR1") {
			// We have a Platco RCU, find the HCI handle and send VSC to BT/Wifi chip
			const QList<HciSocket::ConnectedDeviceInfo> deviceInfos = m_hciSocket->getConnectedDevices();
			for (const HciSocket::ConnectedDeviceInfo &deviceInfo : deviceInfos) {
				qInfo() << "found connected device" << deviceInfo;

				if (address == deviceInfo.address) {
					if (m_hciSocket) {
						qMilestone() << "HCI connection handle:" << deviceInfo.handle << ", device:" << deviceInfo.address
								<< "is a Platco remote, sending VSC to increase BT data capability in the chip BT/WIFI coexistence engine.";
						m_hciSocket->sendIncreaseDataCapability(deviceInfo.handle);
					}
				}
			}
		}
	}

	emit deviceReadyChanged(address, ready, BleRcuAdapter::privateSignal());
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Recovery method, not expected to be called unless something has gone
	seriously wrong.

	This is called whenever someone calls the following
	\code
		emit bleRcuRecovery->powerCycleAdapter();
	\endcode

 */
void BleRcuAdapterBluez::onPowerCycleAdapter()
{
	qMilestone("deliberately power cycling the adapter to try and recover from"
	           " error state");

	if (!m_adapterProxy) {
		qError("bluez not available so can't power cycle the adapter");
		return;
	}

	m_adapterProxy->setPowered(false);

	// if everything is not completely hosed, this will trigger a power event
	// from bluetoothd daemon that will detected by onAdapterPowerChanged()
	// which will in turn schedule a re-power event
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Recovery method, not expected to be called unless something has gone
	seriously wrong.

	This is called whenever someone calls the following
	\code
		emit bleRcuRecovery->reconnectDevice(m_address);
	\endcode

 */
void BleRcuAdapterBluez::onDisconnectReconnectDevice(const BleAddress &device)
{
	// TODO: implement if needed, it is tricky and power cycling is generally
	// the better option
#if 1

	Q_UNUSED(device);
	qError() << "recovery method not implemented, use power cycle instead";

#else
	qMilestone() << "deliberately disconnecting / reconnecting to device"
	             << device << "to try and recover from error state";

	if (!m_devices.contains(device)) {
		qError() << "failed to find device" << device << "to try and recover";
		return;
	}

	const QSharedPointer<BleRcuDeviceBluez> device_ = m_devices[device];
	if (!device_ || !device_->isValid()) {
		qError() << "failed to find device" << device << "to try and recover";
		return;
	}

	// TODO

#endif
}

