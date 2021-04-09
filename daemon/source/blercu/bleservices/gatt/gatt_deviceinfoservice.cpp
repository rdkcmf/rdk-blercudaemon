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
//  gatt_deviceinfoservice.cpp
//  SkyBluetoothRcu
//

#include "gatt_deviceinfoservice.h"
#include "blercu/blegattservice.h"
#include "blercu/blegattcharacteristic.h"
#include "blercu/blercuerror.h"

#include "utils/logging.h"

#include <QDebug>



const BleUuid GattDeviceInfoService::m_serviceUuid(BleUuid::DeviceInformation);




// -----------------------------------------------------------------------------
/*!
	\class GattDeviceInfoService
	\brief

 */



// this table describes what command to send in each state and also the next
// state to move too plus the handler to call to process the reply message sent
// by the device through the tunnel
const QMap<GattDeviceInfoService::InfoField, GattDeviceInfoService::StateHandler> GattDeviceInfoService::m_stateHandler = {

	//  flag                   characteristic uuid              characteristic handler
	{   ManufacturerName,    { BleUuid::ManufacturerNameString, &GattDeviceInfoService::setManufacturerName  }   },
	{   ModelNumber,         { BleUuid::ModelNumberString,      &GattDeviceInfoService::setModelNumber       }   },
	{   SerialNumber,        { BleUuid::SerialNumberString,     &GattDeviceInfoService::setSerialNumber      }   },
	{   HardwareRevision,    { BleUuid::HardwareRevisionString, &GattDeviceInfoService::setHardwareRevision  }   },
	{   FirmwareVersion,     { BleUuid::FirmwareRevisionString, &GattDeviceInfoService::setFirmwareVersion   }   },
	{   SoftwareVersion,     { BleUuid::SoftwareRevisionString, &GattDeviceInfoService::setSoftwareVersion   }   },
	{   SystemId,            { BleUuid::SystemID,               &GattDeviceInfoService::setSystemId          }   },
	{   PnPId,               { BleUuid::PnPID,                  &GattDeviceInfoService::setPnPId             }   },

};


// -----------------------------------------------------------------------------
/*!
	Constructs the device info service which queries the info over the bluez
	GATT interface.

 */
GattDeviceInfoService::GattDeviceInfoService()
	: BleRcuDeviceInfoService(nullptr)
	, m_forceRefresh(false)
	, m_infoFlags(0)
	, m_systemId(0)
	, m_vendorIdSource(Invalid)
	, m_vendorId(0)
	, m_productId(0)
	, m_productVersion(0)
{
	// setup the basic statemachine
	init();
}

GattDeviceInfoService::~GattDeviceInfoService()
{
	stop();
}

// -----------------------------------------------------------------------------
/*!
	Returns the gatt uuid of this service.

 */
BleUuid GattDeviceInfoService::uuid()
{
	return m_serviceUuid;
}

// -----------------------------------------------------------------------------
/*!
	\internal
 
	Initailises the state machine used internally by the class.

 */
void GattDeviceInfoService::init()
{
	m_stateMachine.setObjectName(QStringLiteral("GattDeviceInfoService"));

	// add all the states and the super state groupings
	m_stateMachine.addState(IdleState, QStringLiteral("Idle"));
	m_stateMachine.addState(InitialisingState, QStringLiteral("Initialising"));
	m_stateMachine.addState(RunningState, QStringLiteral("Running"));
	m_stateMachine.addState(StoppedState, QStringLiteral("Stopped"));


	// add the transitions:      From State        ->   Event                               ->  To State
	m_stateMachine.addTransition(IdleState,             StartServiceRequestEvent,               InitialisingState);
	m_stateMachine.addTransition(IdleState,             StartServiceForceRefreshRequestEvent,   InitialisingState);

	m_stateMachine.addTransition(InitialisingState,     StopServiceRequestEvent,                IdleState);
	m_stateMachine.addTransition(InitialisingState,     InitialisedEvent,                       RunningState);

	m_stateMachine.addTransition(RunningState,          StopServiceRequestEvent,                StoppedState);
	m_stateMachine.addTransition(StoppedState,          StartServiceRequestEvent,               RunningState);
	m_stateMachine.addTransition(StoppedState,          StartServiceForceRefreshRequestEvent,   InitialisingState);


	// connect to the state entry signal
	QObject::connect(&m_stateMachine, &StateMachine::entered,
	                 this, &GattDeviceInfoService::onEnteredState);
	QObject::connect(&m_stateMachine, &StateMachine::exited,
	                 this, &GattDeviceInfoService::onExitedState);


	// set the initial state of the state machine and start it
	m_stateMachine.setInitialState(IdleState);
	m_stateMachine.start();
}


// -----------------------------------------------------------------------------
/*!
	Starts the service by setting the initial state and sending off the first
	gatt characteristic read requests.  When the service has finished it's setup,
	a \a ready() signal will be emitted.


 */
bool GattDeviceInfoService::start(const QSharedPointer<const BleGattService> &gattService)
{
	// check we're not already started
	if (!m_stateMachine.inState( { IdleState, StoppedState } )) {
		qWarning() << "service is already started";
		return true;
	}

	// unlike the other services, the device information only contains static
	// data, so we don't create and store multiple dbus proxies to the actual
	// characteristics, instead we just send one off requests to read the
	// values
	m_gattService = gattService;

	// if the force flag is set then start or restart the service forcing it
	// to rescan the device info (this is typically set after f/w upgrade)
	if (m_forceRefresh) {
		m_forceRefresh = false;
		m_stateMachine.postEvent(StartServiceForceRefreshRequestEvent);

	} else {
		// just an ordinary service start
		m_stateMachine.postEvent(StartServiceRequestEvent);
	}

	return true;
}

// -----------------------------------------------------------------------------
/*!


 */
void GattDeviceInfoService::stop()
{
	m_gattService.clear();
	m_stateMachine.postEvent(StopServiceRequestEvent);
}

// -----------------------------------------------------------------------------
/*!
	Slot typically called after the completion of a firmware upgrade, it causes
	the device info fields to be re-read the next time the service is started.

 */
void GattDeviceInfoService::forceRefresh()
{
	m_forceRefresh = true;
}

// -----------------------------------------------------------------------------
/*!
	\internal


 */
void GattDeviceInfoService::onEnteredState(int state)
{
	switch (state) {
		case InitialisingState:
			// clear the bitmask of recceived fields and go and request them
			// all in one big lump
			m_infoFlags = 0;
			sendCharacteristicReadRequest(ManufacturerName);
			sendCharacteristicReadRequest(ModelNumber);
			sendCharacteristicReadRequest(SerialNumber);
			sendCharacteristicReadRequest(HardwareRevision);
			sendCharacteristicReadRequest(FirmwareVersion);
			sendCharacteristicReadRequest(SoftwareVersion);
			sendCharacteristicReadRequest(PnPId);
			sendCharacteristicReadRequest(SystemId);
			break;

		case RunningState:
			emit ready();
			break;
	}
}

// -----------------------------------------------------------------------------
/*!
	\internal


 */
void GattDeviceInfoService::onExitedState(int state)
{
	// on exiting the initialising state we should have all the required
	// device info fields, so at this point log a milestone message with all
	// the details
	if (state == InitialisingState) {
		qProdLog("bluetooth rcu device info [ %s / %s / hw:%s / fw:%s / sw:%s ]",
		         qPrintable(m_manufacturerName), qPrintable(m_modelNumber),
		         qPrintable(m_hardwareRevision), qPrintable(m_firmwareVersion),
		         qPrintable(m_softwareVersion));
	}
}

// -----------------------------------------------------------------------------
/*!
	\internal


 */
void GattDeviceInfoService::sendCharacteristicReadRequest(InfoField field)
{
	// sanity checks
	if (Q_UNLIKELY(!m_stateHandler.contains(field))) {
		qError() << "trying to send command for unknown info field" << field;
		return;
	}

	if (Q_UNLIKELY(!m_gattService || !m_gattService->isValid())) {
		qError("gatt service info is not valid");
		return;
	}


	// get the uuid of characteristic whos value we want to retrieve
	const BleUuid &uuid = m_stateHandler[field].uuid;

	// try and get the dbus object path to the characteristic
	QSharedPointer<BleGattCharacteristic> characteristic =
		m_gattService->characteristic(uuid);
	if (!characteristic || !characteristic->isValid()) {

		// systemID is optional so don't log an error if not present
		if (uuid != BleUuid(BleUuid::SystemID)) {
			qWarning() << "missing or invalid gatt characteristic with uuid"
			           << uuid << "skipping device info characteristic";
		}

		return;
	}

	// request a read on the characteristic
	Future<QByteArray> result = characteristic->readValue();
	if (!result.isValid() || result.isError()) {
		onCharacteristicReadError(result.errorName(), result.errorMessage(), field);
		return;
	} else if (result.isFinished()) {
		onCharacteristicReadSuccess(result.result(), field);
		return;
	}

	// bind the field the request is for as well
	const std::function<void(const QByteArray&)> successFunctor =
		std::bind(&GattDeviceInfoService::onCharacteristicReadSuccess,
		          this, std::placeholders::_1, field);

	const std::function<void(const QString&, const QString&)> errorFunctor =
		std::bind(&GattDeviceInfoService::onCharacteristicReadError,
		          this, std::placeholders::_1, std::placeholders::_2, field);

	// connect functors to the future async completion
	result.connectErrored(this, errorFunctor);
	result.connectFinished(this, successFunctor);
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Slot called when the system replies after successifully reading the
	characteristic value from the remote device.

 */
void GattDeviceInfoService::onCharacteristicReadSuccess(const QByteArray &value,
                                                        InfoField field)
{
	// check we're in a state where a command is expected
	if (!m_stateHandler.contains(field)) {
		qWarning() << "received gatt char reply we weren't expecting for field"
		           << field << "- ignoring the reply";
		return;
	}

	// if we have a processor for the field, call it
	const StateHandler &handler = m_stateHandler[field];
	if (handler.handler != nullptr)
		(this->*(handler.handler))(value);


	// add the field to our bitmask of received fields, if we've got them all
	// then can signal we're initialised
	m_infoFlags |= field;


	// check if we now have all the required fields
	static const InfoFieldFlags requiredFields = ManufacturerName
	                                           | ModelNumber
	                                           | SerialNumber
	                                           | HardwareRevision
	                                           | FirmwareVersion
	                                           | SoftwareVersion
	                                           | PnPId;

	if ((requiredFields & m_infoFlags) == requiredFields)
		m_stateMachine.postEvent(InitialisedEvent);
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Slot called when the system replies after failing to read the characteristic
	value from the remote device.

 */
void GattDeviceInfoService::onCharacteristicReadError(const QString &error,
                                                      const QString &message,
                                                      InfoField field)
{
	// check we're in a state where a command is expected
	if (!m_stateHandler.contains(field)) {
		qWarning() << "received gatt char reply we weren't expecting for field"
		           << field << "- ignoring the reply";
		return;
	}

	const StateHandler &handler = m_stateHandler[field];
	qWarning() << "failed to read value for characteristic with uuid"
	           << handler.uuid << "due to" << error << message;
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Called when we receive a reply from the GATT \c manufacturer_name_string
	characteristic.

	\see https://www.bluetooth.com/specifications/gatt/viewer?attributeXmlFile=org.bluetooth.characteristic.manufacturer_name_string.xml
 */
void GattDeviceInfoService::setManufacturerName(const QByteArray &value)
{
	const QString name = QString::fromUtf8(value);

	if (name != m_manufacturerName) {
		m_manufacturerName = name;
		qInfo() << "manufacturer name:" << m_manufacturerName;

		emit manufacturerNameChanged(m_manufacturerName);
	}
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Called when we receive a reply from the GATT \c model_number_string
	characteristic.

	\see https://www.bluetooth.com/specifications/gatt/viewer?attributeXmlFile=org.bluetooth.characteristic.model_number_string.xml
 */
void GattDeviceInfoService::setModelNumber(const QByteArray &value)
{
	const QString model = QString::fromUtf8(value);

	if (model != m_modelNumber) {
		m_modelNumber = model;
		qInfo() << "model number:" << m_modelNumber;

		emit modelNumberChanged(m_modelNumber);
	}
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Called when we receive a reply from the GATT \c serial_number_string
	characteristic.

	\see https://www.bluetooth.com/specifications/gatt/viewer?attributeXmlFile=org.bluetooth.characteristic.serial_number_string.xml
 */
void GattDeviceInfoService::setSerialNumber(const QByteArray &value)
{
	const QString serial = QString::fromUtf8(value);

	if (serial != m_serialNumber) {
		m_serialNumber = serial;
		qInfo() << "serial number:" << m_serialNumber;

		emit serialNumberChanged(m_serialNumber);
	}
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Called when we receive a reply from the GATT \c hardware_revision_string
	characteristic.

	\see https://www.bluetooth.com/specifications/gatt/viewer?attributeXmlFile=org.bluetooth.characteristic.hardware_revision_string.xml
 */
void GattDeviceInfoService::setHardwareRevision(const QByteArray &value)
{
	const QString hwVersion = QString::fromUtf8(value);

	if (hwVersion != m_hardwareRevision) {
		m_hardwareRevision = hwVersion;
		qInfo() << "hardware revision:" << m_hardwareRevision;

		emit hardwareRevisionChanged(m_hardwareRevision);
	}
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Called when we receive a reply from the GATT \c firmware_revision_string
	characteristic.

	\see https://www.bluetooth.com/specifications/gatt/viewer?attributeXmlFile=org.bluetooth.characteristic.firmware_revision_string.xml
 */
void GattDeviceInfoService::setFirmwareVersion(const QByteArray &value)
{
	const QString fwVersion = QString::fromUtf8(value);

	if (fwVersion != m_firmwareVersion) {

		m_firmwareVersion = fwVersion;
		qInfo() << "firmware version:" << m_firmwareVersion;

		emit firmwareVersionChanged(m_firmwareVersion);
	}
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Called when we receive a reply from the GATT \c software_revision_string
	characteristic.

	\see https://www.bluetooth.com/specifications/gatt/viewer?attributeXmlFile=org.bluetooth.characteristic.software_revision_string.xml
 */
void GattDeviceInfoService::setSoftwareVersion(const QByteArray &value)
{
	const QString swVersion = QString::fromUtf8(value);

	if (swVersion != m_softwareVersion) {

		m_softwareVersion = swVersion;
		qInfo() << "software version:" << m_softwareVersion;

		emit softwareVersionChanged(m_softwareVersion);
	}
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Called when we receive a reply from the GATT \c system_id characteristic.

	\see https://www.bluetooth.com/specifications/gatt/viewer?attributeXmlFile=org.bluetooth.characteristic.system_id.xml
 */
void GattDeviceInfoService::setSystemId(const QByteArray &value)
{
	// sanity check that the received data is 64-bit / 8 bytes
	if (value.length() < 8) {
		qError("received invalid length for system id (%d bytes)", value.length());
		return;
	}

	m_systemId = (quint64(value[0]) << 32) |
	             (quint64(value[1]) << 24) |
	             (quint64(value[2]) << 16) |
	             (quint64(value[3]) <<  8) |
	             (quint64(value[4]) <<  0) |
	             (quint64(value[5]) << 40) |
	             (quint64(value[6]) << 48) |
	             (quint64(value[7]) << 56);

	qInfo("system id: 0x%016llx", m_systemId);
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Called when we receive a reply from the GATT \c pnp_id characteristic.

	The value of PnP is the same as in the bluetooth LE DIS profile;
	https://www.bluetooth.com/specifications/gatt/viewer?attributeXmlFile=org.bluetooth.characteristic.pnp_id.xml

 */
void GattDeviceInfoService::setPnPId(const QByteArray &value)
{
	// sanity check that the received data is at least 7 bytes
	if (value.length() < 7) {
		qError("received invalid length for pnp id (%d bytes)", value.length());
		return;
	}

	// store the pnp data
	m_vendorIdSource =  quint8(value[0]);
	m_vendorId =       (quint16(value[1]) << 0) | (quint16(value[2]) << 8);
	m_productId =      (quint16(value[3]) << 0) | (quint16(value[4]) << 8);
	m_productVersion = (quint16(value[5]) << 0) | (quint16(value[6]) << 8);

	qInfo("pnp id (%s) 0x%04x:0x%04x:0x%04x",
	      (m_vendorIdSource == Bluetooth) ? "bluetooth" :
	      (m_vendorIdSource == USB)       ? "usb" : "?",
	      m_vendorId, m_productId, m_productVersion);
}

// -----------------------------------------------------------------------------
/*!
	\overload

	Returns \c true if the service is ready and all the info fields have been
	populated.

 */
bool GattDeviceInfoService::isReady() const
{
	return m_stateMachine.inState(RunningState);
}

// -----------------------------------------------------------------------------
/*!
	\overload

	This information is not available on RCUs that only implement GATT.

 */
Future<qint16> GattDeviceInfoService::rssi() const
{
	return Future<qint16>::createErrored(BleRcuError::errorString(BleRcuError::Rejected),
	                                     QStringLiteral("Not supported"));
}

// -----------------------------------------------------------------------------
/*!
	\overload

	Returns the manufacturer name string, this is only valid if the service is
	in the ready state.

 */
QString GattDeviceInfoService::manufacturerName() const
{
	return m_manufacturerName;
}

// -----------------------------------------------------------------------------
/*!
	\overload

	Returns the model number string, this is only valid if the service is
	in the ready state.

 */
QString GattDeviceInfoService::modelNumber() const
{
	return m_modelNumber;
}

// -----------------------------------------------------------------------------
/*!
	\overload

	Returns the serial number string, this is only valid if the service is
	in the ready state.

 */
QString GattDeviceInfoService::serialNumber() const
{
	return m_serialNumber;
}

// -----------------------------------------------------------------------------
/*!
	\overload

	Returns the hardware revision string, this is only valid if the service is
	in the ready state.

 */
QString GattDeviceInfoService::hardwareRevision() const
{
	return m_hardwareRevision;
}

// -----------------------------------------------------------------------------
/*!
	\overload

	Returns the firmware version string, this is only valid if the service is
	in the ready state.

 */
QString GattDeviceInfoService::firmwareVersion() const
{
	return m_firmwareVersion;
}

// -----------------------------------------------------------------------------
/*!
	\overload

	Returns the software version string, this is only valid if the service is
	in the ready state.

 */
QString GattDeviceInfoService::softwareVersion() const
{
	return m_softwareVersion;
}

// -----------------------------------------------------------------------------
/*!
	\overload

	Returns the system id value, this is only valid if the service is in the
	ready state.

 */
quint64 GattDeviceInfoService::systemId() const
{
	return m_systemId;
}

// -----------------------------------------------------------------------------
/*!
	\overload

	Returns the PnP vendor id source, i.e. if the vendor id is registered as a
	usb or bluetooth id. This is only valid if the service is in the ready state.

 */
BleRcuDeviceInfoService::PnPVendorSource GattDeviceInfoService::pnpVendorIdSource() const
{
	switch (m_vendorIdSource) {
		case 0x01:     return PnPVendorSource::Bluetooth;
		case 0x02:     return PnPVendorSource::USB;
		default:       return PnPVendorSource::Invalid;
	}
}

// -----------------------------------------------------------------------------
/*!
	\overload

	Returns the PnP vendor id, this is only valid if the service is in the ready
	state.

 */
quint16 GattDeviceInfoService::pnpVendorId() const
{
	return m_vendorId;
}

// -----------------------------------------------------------------------------
/*!
	\overload

	Returns the PnP product id, this is only valid if the service is in the
	ready state.

 */
quint16 GattDeviceInfoService::pnpProductId() const
{
	return m_productId;
}

// -----------------------------------------------------------------------------
/*!
	\overload

	Returns the PnP product version, this is only valid if the service is in the
	ready state.

 */
quint16 GattDeviceInfoService::pnpProductVersion() const
{
	return m_productVersion;
}

