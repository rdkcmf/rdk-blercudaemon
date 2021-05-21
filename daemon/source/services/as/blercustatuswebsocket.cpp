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
//  blercustatuswebsocket.cpp
//  SkyBluetoothRcu
//

#include "blercustatuswebsocket.h"

#include "blercu/blercucontroller.h"
#include "blercu/blercudevice.h"
#include "blercu/bleservices/blercubatteryservice.h"
#include "blercu/bleservices/blercudeviceinfoservice.h"
#include "blercu/bleservices/blercufindmeservice.h"
#include "blercu/bleservices/blercuaudioservice.h"
#include "blercu/bleservices/blercuinfraredservice.h"
#include "blercu/bleservices/blercutouchservice.h"
#include "blercu/bleservices/blercuupgradeservice.h"

#include "utils/inputdevicemanager.h"
#include "utils/logging.h"

#include <QJsonArray>
#include <QJsonDocument>



// -----------------------------------------------------------------------------
/*!
	\class BleRcuStatusWebSocket
	\brief Implements the status notification code for the as
	'/as/peripherals/btremotes/status' websocket.

	Despite it's name this doesn't actually implement a websocket, rather it
	listens for notifications that would change the contents of the status message
	and then sent out the messages to any listeners.

	This class is thread safe and designed to be called from both the Qt main
	event loop thread and any Android / Binder threads.


 */




BleRcuStatusWebSocket::BleRcuStatusWebSocket(int asVersion, QObject *parent)
	: QObject(parent)
	, m_asVersion(asVersion)
	, m_controllerState(BleRcuController::Initialising)
	, m_pairingInProgress(false)
	, JSON {
			QStringLiteral("status"),
			QStringLiteral("pairinginprogress"),
			QStringLiteral("remotes"),
			QStringLiteral("bdaddr"),
			QStringLiteral("connected"),
			QStringLiteral("name"),
			QStringLiteral("deviceid"),
			QStringLiteral("make"),
			QStringLiteral("model"),
			QStringLiteral("hwrev"),
			QStringLiteral("serialno"),
			QStringLiteral("rcuswver"),
			QStringLiteral("btlswver"),
			QStringLiteral("batterylevel")
	  }
{

	// set the initial status json which doesn't contain any devices
	// just pairing status
	QJsonObject root;
	root[JSON.status] = controllerStateString(m_controllerState);
	if (m_asVersion < 109)
		root[JSON.pairinginprogress] = m_pairingInProgress;
	root[JSON.remotes] = QJsonArray();

	// create the json string
	m_status = root;

	// invalidate the ws, will cause an update signal with the current state
	invalidateWebSocket();
}


BleRcuStatusWebSocket::~BleRcuStatusWebSocket()
{

}

// -----------------------------------------------------------------------------
/*!
	\internal

	Converts the controller state to AS string representation.

 */
QString BleRcuStatusWebSocket::controllerStateString(BleRcuController::State state) const
{
	static const QMap<BleRcuController::State, QString> stateNames = {
		{  BleRcuController::Initialising, "INITIALISING" },
		{  BleRcuController::Idle,         "IDLE" },
		{  BleRcuController::Searching,    "SEARCHING" },
		{  BleRcuController::Pairing,      "PAIRING" },
		{  BleRcuController::Complete,     "COMPLETE" },
		{  BleRcuController::Failed,       "FAILED" },
	};

	return stateNames.value(state, QStringLiteral("UNKNOWN"));
}

// -----------------------------------------------------------------------------
/*!
	Sets the controller which supplies us with the notifications and details
	about paired devices.

 */
void BleRcuStatusWebSocket::setController(const QSharedPointer<BleRcuController> &controller)
{
	// setup the input device manager if haven't already
	if (!m_inputDeviceManager) {

		// create the input device manager
		m_inputDeviceManager = InputDeviceManager::create();

		// connect to the events from the InputDevice manager
		QObject::connect(m_inputDeviceManager.data(), &InputDeviceManager::deviceAdded,
		                 this, &BleRcuStatusWebSocket::onInputDeviceAdded);
		QObject::connect(m_inputDeviceManager.data(), &InputDeviceManager::deviceRemoved,
		                 this, &BleRcuStatusWebSocket::onInputDeviceRemoved);
	}

	// sanity check we haven't already set the controller
	if (m_controller) {
		qError("already have a controller object, ignoring");
		return;
	}

	// sanity check the controller object
	if (!controller || !controller->isValid()) {
		qError("invalid controller object");
		return;
	}

	// store the controller object
	m_controller = controller;

	// connect to the controller signals
	QObject::connect(controller.data(), &BleRcuController::managedDeviceAdded,
	                 this, &BleRcuStatusWebSocket::onDeviceAdded);
	QObject::connect(controller.data(), &BleRcuController::managedDeviceRemoved,
	                 this, &BleRcuStatusWebSocket::onDeviceRemoved);
	QObject::connect(controller.data(), &BleRcuController::stateChanged,
	                 this, &BleRcuStatusWebSocket::onControllerStateChanged);
	QObject::connect(controller.data(), &BleRcuController::pairingStateChanged,
	                 this, &BleRcuStatusWebSocket::onPairingStateChanged);


	// add any existing devices to the json status map
	const QSet<BleAddress> devices = m_controller->managedDevices();
	for (const BleAddress &bdaddr : devices) {

		QSharedPointer<BleRcuDevice> device = m_controller->managedDevice(bdaddr);
		if (device && device->isValid()) {
			addDeviceToStatus(device);
		}
	}

	bool updateWs = false;

	// set the initial pairing state
	if (m_pairingInProgress != m_controller->isPairing()) {
		m_pairingInProgress = m_controller->isPairing();
		updateWs = true;
	}

	// set the initial controller state
	if (m_controllerState != m_controller->state()) {
		m_controllerState = m_controller->state();
		updateWs = true;
	}

	if (updateWs) {
		invalidateWebSocket();
	}
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Called when the state of the controller changes.  This is part of the
	new AS API used to support pairing via scanning.

 */
void BleRcuStatusWebSocket::onControllerStateChanged(BleRcuController::State state)
{
	qDebug() << "on controller state change" << state;

	// take the lock and update the json
	QMutexLocker locker(&m_lock);

	// update the controller state
	if (m_controllerState != state) {
		m_controllerState = state;

		// update any listeners for the new device added
		invalidateWebSocket();
	}
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Called when the pairing state of the controller changes.  This slot just
	passes the event onto the Java BleRcuService object.

 */
void BleRcuStatusWebSocket::onPairingStateChanged(bool pairing)
{
	qDebug() << "on pairing state change" << pairing;

	// take the lock and update the json
	QMutexLocker locker(&m_lock);

	// update the pairing status
	if (m_pairingInProgress != pairing) {
		m_pairingInProgress = pairing;

		// update any listeners for the new device added
		invalidateWebSocket();
	}
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Called when the Android InputDevice manager signals that a new InputDevice
	has been created.  We hook this event so that we can upgrade the deviceId
	field of any managed BleRcuDevice.

 */
void BleRcuStatusWebSocket::onInputDeviceAdded(const InputDeviceInfo &info)
{
	// sanity check we have a controller
	if (Q_UNLIKELY(!m_controller)) {
		qError("input device added without controller object");
		return;
	}

	// get the current set of managed devices
	const QSet<BleAddress> devices = m_controller->managedDevices();

	// check if this input device matches one of those
	for (const BleAddress &address : devices) {
		if (info.matches(address)) {
			updateDeviceIdMap(address, info.id());
			break;
		}
	}

}

// -----------------------------------------------------------------------------
/*!
	\internal

	Called when the Android InputDevice manager signals that an InputDevice
	has been removed.  We hook this event so that we can set the deviceId to
	-1.

 */
void BleRcuStatusWebSocket::onInputDeviceRemoved(const InputDeviceInfo &info)
{
	// sanity check we have a controller
	if (Q_UNLIKELY(!m_controller)) {
		qError("input device removed without controller object");
		return;
	}

	// get the current set of managed devices
	const QSet<BleAddress> devices = m_controller->managedDevices();

	// check if this input device matches one of those
	for (const BleAddress &address : devices) {
		if (info.matches(address)) {
			updateDeviceIdMap(address, -1);
			break;
		}
	}
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Used to update the internal BLE address to deviceId map and send out any
	notifications to the AS service on changes.

 */
void BleRcuStatusWebSocket::updateDeviceIdMap(const BleAddress &address,
                                              int deviceId)
{
	qInfo() << "device" << address << "deviceId has changed to" << deviceId;

	if (deviceId < 0) {

		// if we don't have a mapping then nothing to do
		if (!m_deviceIdMap.contains(address))
			return;

		// remove the mapping
		m_deviceIdMap.remove(address);

		// call the java class to post the binder notification
		onDeviceIdChanged(address, -1);

	} else {

		// check if we already have a mapping (and it matches) if so nothing
		// to do
		if (m_deviceIdMap.value(address, -1) == deviceId)
			return;

		// [debugging] check we don't already have a mapping for this device id
		// (this should never happen)
		QMap<BleAddress, int>::iterator it = m_deviceIdMap.begin();
		while (it != m_deviceIdMap.end()) {
			if (it.value() == deviceId) {
				qError() << "deviceId" << deviceId << "is assigned to"
				         << it.key() << "whereas it should be assigned to"
				         << address;

				// send a change event for the old mapping
				onDeviceIdChanged(it.key(), -1);

				// and then remove it from the map
				it = m_deviceIdMap.erase(it);
			} else {
				++it;
			}
		}

		// add the mapping
		m_deviceIdMap.insert(address, deviceId);

		// call the java class to post the binder notification
		onDeviceIdChanged(address, deviceId);
	}
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Creates a new JSON object storing information for the given \a device. The
	device is then added to the internal m_remotes map and the ws status message
	is updated and any listeners informed of the change.

 */
void BleRcuStatusWebSocket::addDeviceToStatus(const QSharedPointer<BleRcuDevice> &device)
{
	const BleAddress bdaddr = device->address();

	// create the initial json object
	QJsonObject remote;
	remote[JSON.bdaddr] = bdaddr.toString();
	remote[JSON.connected] = device->isReady();
	remote[JSON.name] = device->name();

	const int deviceId = m_deviceIdMap.value(bdaddr, device->deviceId());
	if (deviceId >= 0)
		remote[JSON.deviceid] = deviceId;

	// populate the device info fields
	updateDeviceInfo(device, &remote);


	// take the lock and update the json
	QMutexLocker locker(&m_lock);

	m_remotes[bdaddr] = remote;

	// update any listeners for the new device added
	invalidateWebSocket();
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Populates the json object with device info and the current battery level.

 */
void BleRcuStatusWebSocket::updateDeviceInfo(const QSharedPointer<BleRcuDevice> &device,
                                             QJsonObject *remote)
{
	struct DeviceInfoField {
		QString jsonName;
		std::function<QString(BleRcuDeviceInfoService*)> getter;
	};
	static const QVector<DeviceInfoField> infoFields = {
		{ JSON.make,        &BleRcuDeviceInfoService::manufacturerName  },
		{ JSON.model,       &BleRcuDeviceInfoService::modelNumber       },
		{ JSON.hwrev,       &BleRcuDeviceInfoService::hardwareRevision  },
		{ JSON.serialno,    &BleRcuDeviceInfoService::serialNumber      },
		{ JSON.rcuswver,    &BleRcuDeviceInfoService::softwareVersion   },
		{ JSON.btlswver,    &BleRcuDeviceInfoService::firmwareVersion   },
	};

	// populate the device info fields
	QSharedPointer<BleRcuDeviceInfoService> infoService = device->deviceInfoService();
	if (!infoService) {
		qWarning() << "failed to get device info service for" << device->address();
	} else {
		// add the fields if contain valid values
		for (const DeviceInfoField &field : infoFields) {
			const QString value = field.getter(infoService.get());
			if (!value.isNull())
				remote->insert(field.jsonName, value);
		}
	}

	// and the battery level
	QSharedPointer<BleRcuBatteryService> battService = device->batteryService();
	if (!battService) {
		qWarning() << "failed to get device battery service for" << device->address();
	} else {
		// only reply with the battery level if valid
		int battLevel = battService->level();
		if ((battLevel >= 0) && (battLevel <= 100))
			remote->insert(JSON.batterylevel, battLevel);
	}
}

// -----------------------------------------------------------------------------
/*!
	\internal


 */
void BleRcuStatusWebSocket::onDeviceAdded(const BleAddress &address)
{
	qDebug() << "device" << address << "added";

	// get the device added and install a listener on it's state change(s)
	QSharedPointer<BleRcuDevice> device = m_controller->managedDevice(address);
	if (!device || !device->isValid()) {
		qWarning() << "failed to get device wrapper for" << address;
		return;
	}

	// we have to use functors for the events so that the device address is also
	// captured in the slot callback
	QObject::connect(device.data(), &BleRcuDevice::readyChanged,
	                 this, std::bind(&BleRcuStatusWebSocket::onDeviceReadyChanged,
	                                 this, address, std::placeholders::_1),
	                 Qt::UniqueConnection);

	QObject::connect(device.data(), &BleRcuDevice::nameChanged,
	                 this, std::bind(&BleRcuStatusWebSocket::onDeviceNameChanged,
	                                 this, address, std::placeholders::_1),
	                 Qt::UniqueConnection);


	// for the other notifications required of the service we have to drill
	// down into it's individual services
	QSharedPointer<BleRcuBatteryService> battService = device->batteryService();
	if (!battService) {
		qWarning() << "failed to get device battery service for" << address;
	} else {
		QObject::connect(battService.data(), &BleRcuBatteryService::levelChanged,
		                 this, std::bind(&BleRcuStatusWebSocket::onDeviceBatteryLevelChanged,
		                                 this, address, std::placeholders::_1),
		                 Qt::UniqueConnection);
	}

	QSharedPointer<BleRcuDeviceInfoService> infoService = device->deviceInfoService();
	if (!infoService) {
		qWarning() << "failed to get device info service for" << address;
	} else {
		QObject::connect(infoService.data(), &BleRcuDeviceInfoService::softwareVersionChanged,
		                 this, std::bind(&BleRcuStatusWebSocket::onDeviceSoftwareVersionChanged,
		                                 this, address, std::placeholders::_1),
		                 Qt::UniqueConnection);
	}

	// before notifying of the new device check if we already have an input
	// device id for it and add to the internal map
	const InputDeviceInfo info = m_inputDeviceManager->findInputDevice(address);
	if (!info.isNull() && (info.id() != -1))
		updateDeviceIdMap(address, info.id());
	else
		qInfo() << "failed to find input device for" << address;


	// finally add the device to the json and notify any listeners
	addDeviceToStatus(device);
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Called when a device has been 'removed', basically it has been unpaired.
	This only happens when a new device is paired and the we need to remove a
	device when we exceed the maximum number of devices supported.

 */
void BleRcuStatusWebSocket::onDeviceRemoved(const BleAddress &address)
{
	qDebug() << "device" << address << "removed";

	// take the lock and update the json
	QMutexLocker locker(&m_lock);

	// remove from the status json map
	if (m_remotes.remove(address) > 0) {
		invalidateWebSocket();
	}

	locker.unlock();

	// remove any deviceId mapping
	updateDeviceIdMap(address, -1);
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Called when a device's input id has changed.

 */
void BleRcuStatusWebSocket::onDeviceIdChanged(const BleAddress &address, int deviceId)
{
	qDebug() << "device" << address << "input id has changed to" << deviceId;

	// this may be called before a device is added or after it's been removed
	// from the m_remotes map, so to avoid an incorrect warning about missing
	// device we check if the device is in the m_remotes map first
	QMutexLocker locker(&m_lock);

	if (!m_remotes.contains(address))
		return;

	locker.unlock();

	// the device is in the map so update it's value
	updateDeviceStatus(address, JSON.deviceid, deviceId);
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Called when a device's ready state has changed.  Ready means the device is
	paired, connected & all the services have been setup.

	From a remote client's POV they see the 'ready' state as the 'connected'
	state.

 */
void BleRcuStatusWebSocket::onDeviceReadyChanged(const BleAddress &address, bool ready)
{
	qDebug() << "device" << address << (ready ? "ready" : "not ready");


	// get the device pointer for the extra info fields
	QSharedPointer<BleRcuDevice> device = m_controller->managedDevice(address);


	// take the lock and update the json
	QMutexLocker locker(&m_lock);

	auto it = m_remotes.find(address);
	if (it == m_remotes.end()) {
		qWarning("received a device update from unknown device %s",
		         qPrintable(address.toString()));
		return;
	}

	QJsonObject &remote = it.value();

	// if the device is now read we need to re-get the device info
	if (ready && device && device->isValid())
		updateDeviceInfo(device, &remote);

	// and update the connected info
	remote[JSON.connected] = ready;

	// then tell the listeners of the change
	invalidateWebSocket();
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Called when a device's name changes.

 */
void BleRcuStatusWebSocket::onDeviceNameChanged(const BleAddress &address, const QString &name)
{
	qDebug() << "device" << address << "name changed to" << name;

	updateDeviceStatus(address, JSON.name, name);
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Called when a device's level has changed.

 */
void BleRcuStatusWebSocket::onDeviceBatteryLevelChanged(const BleAddress &address, int level)
{
	qDebug() << "device" << address << "battery level changed to" << level;

	updateDeviceStatus(address, JSON.batterylevel, level);
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Called when a device's software version changes.  This happens after the
	firmware has been update on the RCU.

 */
void BleRcuStatusWebSocket::onDeviceSoftwareVersionChanged(const BleAddress &address,
                                                           const QString &swVersion)
{
	qDebug() << "device" << address << "s/w version changed to" << swVersion;

	updateDeviceStatus(address, JSON.rcuswver, swVersion);
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Updates the json for the particular device with the given \a bdaddr and
	informs any ws listeners of the change.

 */
void BleRcuStatusWebSocket::updateDeviceStatus(const BleAddress &bdaddr,
                                               const QString &key,
                                               const QJsonValue &value)
{
	// take the lock and update the json
	QMutexLocker locker(&m_lock);

	auto it = m_remotes.find(bdaddr);
	if (it == m_remotes.end()) {
		qWarning("received a device update from unknown device %s",
		         qPrintable(bdaddr.toString()));

	} else {

		// check if the value has changed, if not then nothing to do
		const QJsonValue curValue = it->value(key);
		if (curValue != value) {

			// set the new value
			it->insert(key, value);

			// inform any listeners
			invalidateWebSocket();
		}
	}
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Connect a queue connection back to ourselves for notifying listeners of a
	change to the ws status ... this is to decouple the service IPC from the
	internal notifications.

 */
void BleRcuStatusWebSocket::invalidateWebSocket()
{
	QMetaObject::invokeMethod(this,
	                          &BleRcuStatusWebSocket::onInvalidatedWebSocket,
	                          Qt::QueuedConnection);
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Queued slot called from the main event loop when we detect that the status
	message needs to be updated.

	This is where we form the json and send of the message to any listeners over
	Binder IPC.

 */
void BleRcuStatusWebSocket::onInvalidatedWebSocket()
{
	QJsonArray remotes;

	// take the lock and update the json
	QMutexLocker locker(&m_lock);

	for (const auto &remote : qAsConst(m_remotes)) {
		remotes.append(remote);
	}

	m_status[JSON.status] = controllerStateString(m_controllerState);
	if (m_asVersion < 109)
		m_status[JSON.pairinginprogress] = m_pairingInProgress;
	m_status[JSON.remotes] = remotes;

	// check if it's changed from last time
	if (m_status == m_lastStatus)
		return;

	// tell all the registered listeners (send binder IPC to as proxy)
	emit updateWebSocket(m_status);

	// save for next time
	m_lastStatus = m_status;
}


