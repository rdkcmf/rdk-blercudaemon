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
//  gatt_infraredservice.cpp
//  SkyBluetoothRcu
//

#include "gatt_infraredservice.h"
#include "gatt_infraredsignal.h"

#include "blercu/blegattservice.h"
#include "blercu/blegattcharacteristic.h"

#include "irdb/irdatabase.h"
#include "irdb/irsignalset.h"

#include "utils/logging.h"
#include "utils/edid.h"

#include <functional>


#if (QT_VERSION < QT_VERSION_CHECK(5, 7, 0))
	// this adds const to non-const objects (like std::as_const)
	template <typename T>
	Q_DECL_CONSTEXPR typename std::add_const<T>::type &qAsConst(T &t) Q_DECL_NOTHROW { return t; }
#endif


const BleUuid GattInfraredService::m_serviceUuid(BleUuid::SkyQInfrared);


// -----------------------------------------------------------------------------
/*!
	Constructs the infrared GATT service.

 */
GattInfraredService::GattInfraredService(const QSharedPointer<const IrDatabase> &irDatabase)
	: BleRcuInfraredService(nullptr)
	, m_irDatabase(irDatabase)
	, m_irStandbyMode(StandbyModeB)
	, m_codeId(-1)
{

	// get the model type, the following env variable will have one of the
	// following values for Amidala boxes
	//   ES160, ESi160 or ESd160
	QRegExp regex("ES*160", Qt::CaseInsensitive, QRegExp::Wildcard);

	const QByteArray stbModel = qgetenv("ETHAN_STB_MODEL");
	if (!stbModel.isEmpty() && regex.exactMatch(stbModel))
		m_irStandbyMode = StandbyModeC;
	else
		m_irStandbyMode = StandbyModeB;

	qInfo("using standby mode %c", (m_irStandbyMode == StandbyModeC) ? 'C' : 'B');


	// setup the state machine
	init();
}

// -----------------------------------------------------------------------------
/*!
	Desctructs the infrared GATT service, stopping it.

 */
GattInfraredService::~GattInfraredService()
{
	// stop the service if it's not already
	stop();
}

// -----------------------------------------------------------------------------
/*!
	Returns the constant gatt service uuid.

 */
BleUuid GattInfraredService::uuid()
{
	return m_serviceUuid;
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Configures and starts the state machine
 */
void GattInfraredService::init()
{
	m_stateMachine.setObjectName(QStringLiteral("GattInfraredService"));

	// add all the states and the super state groupings
	m_stateMachine.addState(IdleState, QStringLiteral("Idle"));
	m_stateMachine.addState(StartingSuperState, QStringLiteral("StartingSuperState"));
	m_stateMachine.addState(StartingSuperState, SetStandbyModeState, QStringLiteral("SetStandbyMode"));
	m_stateMachine.addState(StartingSuperState, GetCodeIdState, QStringLiteral("GetCodeId"));
	m_stateMachine.addState(StartingSuperState, GetIrSignalsState, QStringLiteral("GetIrSignals"));
	m_stateMachine.addState(RunningState, QStringLiteral("Running"));


	// add the transitions:      From State         ->  Event                   ->  To State
	m_stateMachine.addTransition(IdleState,             StartServiceRequestEvent,   SetStandbyModeState);

	m_stateMachine.addTransition(SetStandbyModeState,   SetIrStandbyModeEvent,      GetCodeIdState);
	m_stateMachine.addTransition(GetCodeIdState,        ReceivedCodeIdEvent,        GetIrSignalsState);
	m_stateMachine.addTransition(GetIrSignalsState,     IrSignalsReadyEvent,        RunningState);

	m_stateMachine.addTransition(StartingSuperState,    StopServiceRequestEvent,    IdleState);
	m_stateMachine.addTransition(RunningState,          StopServiceRequestEvent,    IdleState);


	// add a slot for state machine notifications
	QObject::connect(&m_stateMachine, &StateMachine::entered,
	                 this, &GattInfraredService::onEnteredState);


	// set the initial state of the state machine and start it
	m_stateMachine.setInitialState(IdleState);
	m_stateMachine.start();
}

// -----------------------------------------------------------------------------
/*!
	\overload

	Loops through all the IrSignal characteristics on this service and creates
	GattInfraredSignal objects to wrap them (if we don't already have a wrapper).

 */
void GattInfraredService::getSignalCharacteristics(const QSharedPointer<BleGattService> &gattService)
{
	// get all the characteristics
	const QList< QSharedPointer<BleGattCharacteristic> > characteristics =
		gattService->characteristics(BleUuid::InfraredSignal);


	// add a matching IrSignal if we don't already have one
	for (const QSharedPointer<BleGattCharacteristic> &characteristic : characteristics) {

		// check if we already have this characteristic wrapped
		bool haveSignal = false;
		for (const QSharedPointer<GattInfraredSignal> &irSignal : m_irSignals) {
			if (irSignal && irSignal->isValid() &&
			    (irSignal->instanceId() == characteristic->instanceId())) {
				haveSignal = true;
				break;
			}
		}

		if (haveSignal)
			continue;

		// create a wrapper around the char
		QSharedPointer<GattInfraredSignal> irSignal =
			QSharedPointer<GattInfraredSignal>::create(characteristic);
		if (irSignal && irSignal->isValid())
			m_irSignals.append(irSignal);
	}

}

// -----------------------------------------------------------------------------
/*!
	\overload

	Starts the service

 */
bool GattInfraredService::start(const QSharedPointer<BleGattService> &gattService)
{
	// sanity check the supplied info is valid
	if (!gattService || !gattService->isValid() ||
	    (gattService->uuid() != m_serviceUuid)) {
		qWarning("invalid infrared gatt service info");

#if defined(EC101_WORKAROUND_MISSING_IR_SERVICE)
		// this is here to support the initial version of the EC10x RCUs that
		// didn't support the infrared service
		m_stateMachine.postEvent(StartServiceRequestEvent);
		return true;
#else
		return false;
#endif
	}


	// create the bluez dbus proxy to the standby mode characteristic
	if (!m_standbyModeCharacteristic || !m_standbyModeCharacteristic->isValid()) {

		m_standbyModeCharacteristic = gattService->characteristic(BleUuid::InfraredStandby);
		if (!m_standbyModeCharacteristic || !m_standbyModeCharacteristic->isValid()) {
			qWarning("failed to create proxy to the ir standby mode characteristic");
			m_standbyModeCharacteristic.reset();
		}
	}

	// create the bluez dbus proxy to the code id characteristic
	if (!m_codeIdCharacteristic || !m_codeIdCharacteristic->isValid()) {

		m_codeIdCharacteristic = gattService->characteristic(BleUuid::InfraredCodeId);
		if (!m_codeIdCharacteristic || !m_codeIdCharacteristic->isValid()) {
			qWarning("failed to create proxy to the code id characteristic");
			m_codeIdCharacteristic.reset();
		}
	}

	// create the bluez dbus proxy to the code id characteristic
	if (!m_emitIrCharacteristic || !m_emitIrCharacteristic->isValid()) {

		m_emitIrCharacteristic = gattService->characteristic(BleUuid::EmitInfraredSignal);
		if (!m_emitIrCharacteristic || !m_emitIrCharacteristic->isValid()) {
			qWarning("failed to create proxy to the emit ir signal characteristic");
			m_emitIrCharacteristic.reset();
		}
	}

	// creates the GattInfraredSignal objects matched against the characteristics
	getSignalCharacteristics(gattService);


	// check we're not already started
	if (m_stateMachine.state() != IdleState) {
		qWarning("service already started");
		return true;
	}

	m_stateMachine.postEvent(StartServiceRequestEvent);
	return true;
}

// -----------------------------------------------------------------------------
/*!
	\overload

 */
void GattInfraredService::stop()
{
	m_stateMachine.postEvent(StopServiceRequestEvent);
}

// -----------------------------------------------------------------------------
/*!
	\overload

 */
bool GattInfraredService::isReady() const
{
	return m_stateMachine.inState(RunningState);
}

// -----------------------------------------------------------------------------
/*!
	\internal

 */
void GattInfraredService::onEnteredState(int state)
{
	switch (state) {

		case IdleState:
			onEnteredIdleState();
			break;

		case SetStandbyModeState:
			onEnteredSetStandbyModeState();
			break;
		case GetCodeIdState:
			onEnteredGetCodeIdState();
			break;
		case GetIrSignalsState:
			onEnteredGetIrSignalsState();
			break;

		case RunningState:
			// entered running state so emit the ready signal
			emit ready();
			break;

	}
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Called on entry to the 'idle' state, used to stop all the GattInfraredSignal
	objects.

 */
void GattInfraredService::onEnteredIdleState()
{
	// this doesn't do much, just aborts any outstanding operation on the
	// ir signal objects
	for (const QSharedPointer<GattInfraredSignal> &irSignal : qAsConst(m_irSignals)) {
		if (irSignal)
			irSignal->stop();
	}

}

// -----------------------------------------------------------------------------
/*!
	\internal

	Called at service start time, it sets the standby mode to match the current
	platform.  In theory we only need to do this once after pairing, however
	reading the value then writing is more hassle than just writing the value
	everytime.

 */
void GattInfraredService::onEnteredSetStandbyModeState()
{
	// sanity check we actually have a standby mode characterisitc
	if (!m_standbyModeCharacteristic || !m_standbyModeCharacteristic->isValid()) {
		qWarning("missing standby mode characterisitc");
		m_stateMachine.postEvent(SetIrStandbyModeEvent);
		return;
	}

	// for amidala boxes we use mode C
	const quint8 value = (m_irStandbyMode == StandbyModeC) ? 0x00 : 0x01;


	// lambda called if an error occurs enabling the notifications
	const std::function<void(const QString&, const QString&)> errorCallback =
		[this](const QString &errorName, const QString &errorMessage)
		{
			qError() << "failed to write standby mode due to" << errorName << errorMessage;

			// tell the state machine we are now ready (even though we have failed)
			m_stateMachine.postEvent(SetIrStandbyModeEvent);
		};

	// lamda called if notifications are successifully enabled
	const std::function<void()> successCallback =
		[this](void)
		{
			qInfo("set ir standby mode to 0x%02x",
			      (m_irStandbyMode == StandbyModeC) ? 0x00 : 0x01);

			// tell the state machine we are now ready
			m_stateMachine.postEvent(SetIrStandbyModeEvent);
		};


	// send a request to write the standby mode
	Future<> result = m_standbyModeCharacteristic->writeValue(QByteArray(1, value));
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

	Called when we receive a reply to a request to get the current tv codes id.

 */
void GattInfraredService::onEnteredGetCodeIdState()
{
	// sanity check we actually have a codeID characterisitc
	if (!m_codeIdCharacteristic || !m_codeIdCharacteristic->isValid()) {
		qWarning("missing code id characterisitc");
		m_stateMachine.postEvent(ReceivedCodeIdEvent);
		return;
	}

	// lambda called if an error occurs reading the codeId characteristic
	const std::function<void(const QString&, const QString&)> errorCallback =
		[this](const QString &errorName, const QString &errorMessage)
		{
			qWarning() << "failed to get initial ir codeId due to"
			           << errorName << errorMessage;

			// tell the state machine we are now ready (even though we have failed)
			m_stateMachine.postEvent(ReceivedCodeIdEvent);
		};

	// lamda called if codeId characteristic read was successiful
	const std::function<void(const QByteArray&)> successCallback =
		[this](const QByteArray &value)
		{
			// store the code id internally
			if (value.length() < 4) {
				qWarning("failed to get initial ir codeId because to small");

			} else {
				qint32 codeId_ = (qint32(value[0]) << 0)  |
				                 (qint32(value[1]) << 8)  |
				                 (qint32(value[2]) << 16) |
				                 (qint32(value[3]) << 24);

				qInfo() << "tv code configuration" << codeId_;

				if (codeId_ != m_codeId) {
					m_codeId = codeId_;
					emit codeIdChanged(m_codeId);
				}
			}

			// tell the state machine we are now ready
			m_stateMachine.postEvent(ReceivedCodeIdEvent);
		};


	// send a request to write the standby mode
	Future<QByteArray> result = m_codeIdCharacteristic->readValue();
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

	Called on entry to the 'getting ir signals' state, which means we have a
	list of Infrared Signal characteristics, but we don't know which keys they
	belong to so we need to read each of their descriptors.


 */
void GattInfraredService::onEnteredGetIrSignalsState()
{
	int readyCount = 0;

	// iterator through all the signal objects, any that aren't ready need
	// to be poked to start
	for (const QSharedPointer<GattInfraredSignal> &irSignal : qAsConst(m_irSignals)) {

		// start the individual IR signal
		irSignal->start();

		// if not ready then listen for the signal
		if (irSignal->isReady()) {
			readyCount++;

		} else {
			QObject::connect(irSignal.data(), &GattInfraredSignal::ready,
			                 this, &GattInfraredService::onIrSignalReady,
			                 Qt::UniqueConnection);
		}
	}

	// check if they're all already ready
	if (readyCount == m_irSignals.length())
		m_stateMachine.postEvent(IrSignalsReadyEvent);
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Called when one of the GattInfraredSignal objects has become ready (has
	read it's reference descriptor).  We check again if now all the signals
	are ready, if they are then emit a signal to the state machine.

 */
void GattInfraredService::onIrSignalReady()
{
	// get the count of ready signals
	int readyCount = 0;
	for (const QSharedPointer<GattInfraredSignal> &irSignal : qAsConst(m_irSignals)) {
		if (irSignal && irSignal->isReady())
			readyCount++;
	}

	// check if they're all ready
	if (readyCount == m_irSignals.length())
		m_stateMachine.postEvent(IrSignalsReadyEvent);
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Utility to create an immediate errored future object.
 */
template<typename T>
Future<T> GattInfraredService::createErrorResult(BleRcuError::ErrorType type,
                                                 const QString &message) const
{
	qDebug() << "future error" << message;
	return Future<T>::createErrored(BleRcuError::errorString(type), message);
}

// -----------------------------------------------------------------------------
/*!
	\overload

 */
qint32 GattInfraredService::codeId() const
{
	return m_codeId;
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Attempts to write the \a codeId value into the RCU device.  The returned
	value is a Future that will triggered when the write completes or fails
	with an error.

 */
Future<> GattInfraredService::writeCodeIdValue(qint32 codeId)
{
	// we don't need to check the current state as that should have been done
	// before calling this

	// sanity check we have characteristic to write to though
	if (!m_codeIdCharacteristic || !m_codeIdCharacteristic->isValid())
		return createErrorResult<>(BleRcuError::General,
		                           QStringLiteral("Missing codeId characteristic"));


	QSharedPointer<Promise<>> promise = QSharedPointer<Promise<>>::create();

	// lambda called if an error occurs reading the codeId characteristic
	const std::function<void(const QString&, const QString&)> errorCallback =
		[promise, codeId](const QString &errorName, const QString &errorMessage)
		{
			qWarning() << "failed to write codeId" << codeId << "due to"
			           << errorName << errorMessage;

			promise->setError(BleRcuError::errorString(BleRcuError::General),
			                  QStringLiteral("Failed to store code ID"));
		};

	// lamda called if codeId characteristic read was successiful
	const std::function<void()> successCallback =
		[promise, codeId]()
		{
			qInfo() << "set code id to" << codeId;

			promise->setFinished();
		};


	// construct the value to write
	struct Q_PACKED {
		qint32 tvCodeId;
		qint32 ampCodeId;
	} codeIds = { codeId, -1 };
	const QByteArray value(reinterpret_cast<const char*>(&codeIds), 8);


	// send a request to write the the code ID value
	Future<> result = m_codeIdCharacteristic->writeValue(value);
	if (!result.isValid() || result.isError()) {
		errorCallback(result.errorName(), result.errorMessage());
	} else if (result.isFinished()) {
		successCallback();
	} else {
		result.connectErrored(this, errorCallback);
		result.connectFinished(this, successCallback);
	}

	return promise->future();
}

// -----------------------------------------------------------------------------
/*!
	\overload

 */
Future<> GattInfraredService::eraseIrSignals()
{
	// check the service is ready
	if (m_stateMachine.state() != RunningState)
		return createErrorResult(BleRcuError::Busy,
		                         QStringLiteral("Service not ready"));

	// check we don't already have an outstanding pending call
	if (m_outstandingOperation && !m_outstandingOperation->isFinished())
		return createErrorResult(BleRcuError::Busy,
		                         QStringLiteral("Service is busy"));


	// for each signal we just program empty data, this will typically just
	// disable the signal rather than actually programming it
	QList< Future<> > results;
	for (const QSharedPointer<GattInfraredSignal> &irSignal : qAsConst(m_irSignals)) {

		if (!irSignal)
			continue;

		// request the operation and add the resulting future to the aggregator
		// so we wait for all operations to complete
		results.append( irSignal->program(QByteArray()) );
	}


	// check we've queued at least one option
	if (results.isEmpty())
		return createErrorResult(BleRcuError::General,
		                         QStringLiteral("Internal error"));

	// create an aggregation of all the future results and return a future
	// that wraps them all
	m_outstandingOperation = QSharedPointer<FutureAggregator>::create(std::move(results));
	return m_outstandingOperation->future();
}


// -----------------------------------------------------------------------------
/*!
	\internal

	Looks up the \a codeId in the database and if found returns a map of
	required keys to IR data blob.  If the key is in the \a keyCodes required
	set, but doesn't exist in the database then an empty QByteArray is added to
	the map for that key.

 */
QMap<Qt::Key, QByteArray> GattInfraredService::getIrSignalData(qint32 codeId,
                                                               const QSet<Qt::Key> &keyCodes) const
{
	// sanity check the database
	if (!m_irDatabase || !m_irDatabase->isValid()) {
		qWarning("missing ir databsae");
		return QMap<Qt::Key, QByteArray>();
	}

	// get the signal data from the database
	IrSignalSet irSignalSet = m_irDatabase->irSignals(IrDatabase::EC10x, codeId);
	if (!irSignalSet.isValid())
		return QMap<Qt::Key, QByteArray>();

	// convert into a local form
	QMap<Qt::Key, QByteArray> irSignalData;
	for (const Qt::Key &requiredKey : keyCodes) {

		if (irSignalSet.contains(requiredKey)) {
#ifndef RDK
			irSignalData.insert(requiredKey, irSignalSet[requiredKey]);
#endif
		}
		else
			irSignalData.insert(requiredKey, QByteArray());
	}

	return irSignalData;
}

// -----------------------------------------------------------------------------
/*!
	\overload

 */
Future<> GattInfraredService::programIrSignalWaveforms(const QMap<Qt::Key, QByteArray> &irWaveforms)
{
	// sanity check we've been asked to program at least one key
	if (irWaveforms.isEmpty())
		return createErrorResult(BleRcuError::InvalidArg,
		                         QStringLiteral("Invalid list of keys to program"));

	// check the service is ready
	if (m_stateMachine.state() != RunningState)
		return createErrorResult(BleRcuError::Busy,
		                         QStringLiteral("Service not ready"));

	// check we don't already have an outstanding pending call
	if (m_outstandingOperation && !m_outstandingOperation->isFinished())
		return createErrorResult(BleRcuError::Busy,
		                         QStringLiteral("Service is busy"));

	m_outstandingOperation.reset();

	// for each signal attempt to program the data, empty data means the signal
	// should be disabled
	QList< Future<> > results;
	for (const QSharedPointer<GattInfraredSignal> &irSignal : qAsConst(m_irSignals)) {

		if (!irSignal)
			continue;

		// check the key is in the dataset to program
		const Qt::Key keyCode = irSignal->keyCode();
		if (!irWaveforms.contains(keyCode))
			continue;

		// request the operation and add the resulting future to the aggregator
		// so we wait for all operations to complete
		results.append( irSignal->program(irWaveforms[keyCode]) );
	}

	// check we've queued at least one option
	if (results.isEmpty())
		return createErrorResult(BleRcuError::General,
		                         QStringLiteral("Internal error"));

	// create an aggregation of all the future results and return a future
	// that wraps them all
	m_outstandingOperation = QSharedPointer<FutureAggregator>::create(std::move(results));
	return m_outstandingOperation->future();
}

Future<> GattInfraredService::programIrSignals(qint32 codeId,
                                               const QSet<Qt::Key> &keyCodes)
{
	// sanity check we've been asked to program at least one key
	if (keyCodes.isEmpty())
		return createErrorResult(BleRcuError::InvalidArg,
		                         QStringLiteral("Invalid list of keys to program"));

	// check the service is ready
	if (m_stateMachine.state() != RunningState)
		return createErrorResult(BleRcuError::Busy,
		                         QStringLiteral("Service not ready"));

	// check we don't already have an outstanding pending call
	if (m_outstandingOperation && !m_outstandingOperation->isFinished())
		return createErrorResult(BleRcuError::Busy,
		                         QStringLiteral("Service is busy"));

	m_outstandingOperation.reset();

	
	// get the signal data from the database
	QMap<Qt::Key, QByteArray> irSignalData = getIrSignalData(codeId, keyCodes);
	if (irSignalData.isEmpty())
		return createErrorResult(BleRcuError::InvalidArg,
		                         QStringLiteral("Invalid codeId value"));



	// for each signal attempt to program the data, empty data means the signal
	// should be disabled
	QList< Future<> > results;
	for (const QSharedPointer<GattInfraredSignal> &irSignal : qAsConst(m_irSignals)) {

		if (!irSignal)
			continue;

		// check the key is in the dataset to program
		const Qt::Key keyCode = irSignal->keyCode();
		if (!irSignalData.contains(keyCode))
			continue;

		// request the operation and add the resulting future to the aggregator
		// so we wait for all operations to complete
		results.append( irSignal->program(irSignalData[keyCode]) );
	}


	// check we've queued at least one option
	if (results.isEmpty())
		return createErrorResult(BleRcuError::General,
		                         QStringLiteral("Internal error"));


	// also queue a write to set the codeId value
	results.append( writeCodeIdValue(codeId) );


	// create an aggregation of all the future results and return a future
	// that wraps them all
	m_outstandingOperation = QSharedPointer<FutureAggregator>::create(std::move(results));
	return m_outstandingOperation->future();
}

// -----------------------------------------------------------------------------
/*!
	\overload

 */
Future<> GattInfraredService::emitIrSignal(Qt::Key keyCode)
{
	// check the service is ready
	if (!isReady() || !m_emitIrCharacteristic)
		return createErrorResult<>(BleRcuError::Busy,
		                           QStringLiteral("Service not ready"));

	// convert the key code a value specified in the GATT spec (table 2.2)
	const quint8 gattKeyCode = keyCodeToGattValue(keyCode);
	if (gattKeyCode == 0xff)
		return createErrorResult<>(BleRcuError::InvalidArg,
		                           QStringLiteral("Invalid key code"));

	// send a request to emit the IR key code
	return m_emitIrCharacteristic->writeValue(QByteArray(1, gattKeyCode));
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Converts a \a keyCode from the enum to the byte value used in the GATT spec.

	Unknown / invalid key codes are converted to \c 0xff.

 */
quint8 GattInfraredService::keyCodeToGattValue(Qt::Key keyCode) const
{
	switch (keyCode) {
		case Qt::Key_Standby:        return 0x0C;
		case Qt::Key_Settings:       return 0x29;   // input select
		case Qt::Key_VolumeUp:       return 0x10;
		case Qt::Key_VolumeDown:     return 0x11;
		case Qt::Key_VolumeMute:     return 0x0D;
		case Qt::Key_Select:         return 0x5C;
		case Qt::Key_Up:             return 0x58;
		case Qt::Key_Left:           return 0x5A;
		case Qt::Key_Right:          return 0x5B;
		case Qt::Key_Down:           return 0x59;
		case Qt::Key_unknown:        return 0xff;
		default:
			qWarning("unknown key code %d", keyCode);
			return 0xff;
	}
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Converts the supplied set of search options to a database lookup type.

 */
static IrDatabase::Type searchOptionsToType(BleRcuInfraredService::SearchOptions options)
{
	// convert the flags to a lookup type, we don't support mixed type lookups
	IrDatabase::Type type;
	switch (options & (BleRcuInfraredService::NoTelevisions | BleRcuInfraredService::NoAVAmplifiers)) {
		case BleRcuInfraredService::NoTelevisions:
			type = IrDatabase::AVAmplifiers;
			break;
		case BleRcuInfraredService::NoAVAmplifiers:
			type = IrDatabase::Televisions;
			break;
		default:
			qWarning("invalid search flags, defaulting to TV type");
			type = IrDatabase::Televisions;
	}

	// don't support non-alphabetic sorting
	if (!options.testFlag(BleRcuInfraredService::SortAlphabetically))
		qWarning("non-alphabetic sorting not supported");

	return type;
}

// -----------------------------------------------------------------------------
/*!
	\reimp

	The IR database is implemented as an sqlite database and the same for all
	GATT based RCUs.  However for legacy reasons the database is associated with
	a particular vendor / RCU, hence why this is implemented here.

 */
Future<BleRcuInfraredService::SearchResults> GattInfraredService::brands(const QString &search,
                                                                         SearchOptions options,
                                                                         qint64 offset,
                                                                         qint64 limit) const
{
	// sanity check the database
	if (!m_irDatabase || !m_irDatabase->isValid())
		return Future<SearchResults>::createErrored(BleRcuError::errorString(BleRcuError::General),
		                                            QStringLiteral("Missing IR database file"));

	// get the type; tv or amp to lookup
	IrDatabase::Type type = searchOptionsToType(options);

	// perform the lookup
	SearchResults results;
	results.results = m_irDatabase->brands(type, search,
	                                       &results.maxResults, offset, limit);

	// return immediately
	return Future<SearchResults>::createFinished(results);
}

// -----------------------------------------------------------------------------
/*!
	\reimp

	Performs a model search for the given brand.  The result is returned as a
	Future to maintain compatability with vendor supplied daemons that perform
	the lookup over dbus.

 */
Future<BleRcuInfraredService::SearchResults> GattInfraredService::models(const QString &brand,
                                                                         const QString &search,
                                                                         SearchOptions options,
                                                                         qint64 offset,
                                                                         qint64 limit) const
{
	// sanity check the database
	if (!m_irDatabase || !m_irDatabase->isValid())
		return Future<SearchResults>::createErrored(BleRcuError::errorString(BleRcuError::General),
		                                            QStringLiteral("Missing IR database file"));

	// get the type; tv or amp to lookup
	IrDatabase::Type type = searchOptionsToType(options);

	// perform the lookup
	SearchResults results;
	results.results = m_irDatabase->models(type, brand, search,
	                                       &results.maxResults, offset, limit);

	// return immediately
	return Future<SearchResults>::createFinished(results);
}

// -----------------------------------------------------------------------------
/*!
	\reimp

	Returns a list of code ids for the given brand and optional model.

 */
Future<IrCodeList> GattInfraredService::codeIds(const QString &brand,
                                                const QString &model,
                                                SearchOptions options) const
{
	// sanity check the database
	if (!m_irDatabase || !m_irDatabase->isValid())
		return Future<IrCodeList>::createErrored(BleRcuError::errorString(BleRcuError::General),
		                                         QStringLiteral("Missing IR database file"));

	// get the type; tv or amp to lookup
	IrDatabase::Type type = searchOptionsToType(options);

	// perform the lookup
	IrCodeList results = m_irDatabase->codeIds(type, brand, model);

	// return immediately
	return Future<IrCodeList>::createFinished(results);
}

// -----------------------------------------------------------------------------
/*!
	\reimp

 */
Future<IrCodeList> GattInfraredService::codeIds(const QByteArray &edid) const
{
	// sanity check the database
	if (!m_irDatabase || !m_irDatabase->isValid())
		return Future<IrCodeList>::createErrored(BleRcuError::errorString(BleRcuError::General),
		                                         QStringLiteral("Missing IR database file"));

	// perform the lookup
	IrCodeList results = m_irDatabase->codeIds(Edid(edid));

	// return immediately
	return Future<IrCodeList>::createFinished(results);
}


