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
//  gatt_audioservice.cpp
//  SkyBluetoothRcu
//

#include "gatt_audioservice.h"
#include "gatt_audiopipe.h"

#include "blercu/blegattservice.h"
#include "blercu/blegattcharacteristic.h"

#include "utils/logging.h"

#include <QIODevice>
#include <QFile>
#include <QtDBus>

#include <unistd.h>
#include <errno.h>


const BleUuid GattAudioService::m_serviceUuid(BleUuid::SkyQVoice);


GattAudioService::GattAudioService()
	: BleRcuAudioService(nullptr)
	, m_packetsPerFrame(5)
	, m_timeoutEventId(-1)
	, m_gainLevel(0xFF)
	, m_audioCodecs(0)
	, m_emitOneTimeStreamingSignal(true)
{
	// clear the last stats
	m_lastStats.lastError = NoError;
	m_lastStats.actualPackets = 0;
	m_lastStats.expectedPackets = 0;

	// setup the state machine
	init();
}

GattAudioService::~GattAudioService()
{
	// ensure the service is stopped
	stop();
}

// -----------------------------------------------------------------------------
/*!
	Returns the constant gatt service uuid.

 */
BleUuid GattAudioService::uuid()
{
	return m_serviceUuid;
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Configures and starts the state machine
 */
void GattAudioService::init()
{
	m_stateMachine.setObjectName(QStringLiteral("GattAudioService"));
	m_stateMachine.setTransistionLogLevel(QtInfoMsg, &milestone());

	// add all the states and the super state groupings
	m_stateMachine.addState(IdleState, QStringLiteral("Idle"));
	m_stateMachine.addState(ReadyState, QStringLiteral("Ready"));

	m_stateMachine.addState(StreamingSuperState, QStringLiteral("StreamingSuperState"));
	m_stateMachine.addState(StreamingSuperState, EnableNotificationsState, QStringLiteral("EnableNotifications"));
	m_stateMachine.addState(StreamingSuperState, StartStreamingState, QStringLiteral("StartStreaming"));
	m_stateMachine.addState(StreamingSuperState, StreamingState, QStringLiteral("Streaming"));
	m_stateMachine.addState(StreamingSuperState, StopStreamingState, QStringLiteral("StopStreaming"));


	// add the transitions:      From State         ->      Event                   ->  To State
	m_stateMachine.addTransition(IdleState,                 StartServiceRequestEvent,   ReadyState);
	m_stateMachine.addTransition(ReadyState,                StopServiceRequestEvent,    IdleState);
	m_stateMachine.addTransition(ReadyState,                StartStreamingRequestEvent, EnableNotificationsState);

	m_stateMachine.addTransition(EnableNotificationsState,  NotificationsEnabledEvent,  StartStreamingState);

	m_stateMachine.addTransition(StartStreamingState,       StreamingStartedEvent,      StreamingState);

	m_stateMachine.addTransition(StreamingState,            StopStreamingRequestEvent,  StopStreamingState);
	m_stateMachine.addTransition(StreamingState,            OutputPipeCloseEvent,       StopStreamingState);

	m_stateMachine.addTransition(StopStreamingState,        StreamingStoppedEvent,      ReadyState);

	m_stateMachine.addTransition(StreamingSuperState,       GattErrorEvent,             ReadyState);


	// connect to the state entry / exit signals
	QObject::connect(&m_stateMachine, &StateMachine::entered,
	                 this, &GattAudioService::onEnteredState);
	QObject::connect(&m_stateMachine, &StateMachine::exited,
	                 this, &GattAudioService::onExitedState);


	// set the initial state of the state machine and start it
	m_stateMachine.setInitialState(IdleState);
	m_stateMachine.start();
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Attempts to create a proxy to the GATT interface for the Audio Codecs
	characteristic, returns \c true on success and \c false on failure.

 */
bool GattAudioService::getAudioCodecsCharacteristic(const QSharedPointer<const BleGattService> &gattService)
{
	// don't re-create if we already have valid proxies
	if (m_audioCodecsCharacteristic && m_audioCodecsCharacteristic->isValid())
		return true;

	// get the chararacteristic for the audio control
	m_audioCodecsCharacteristic = gattService->characteristic(BleUuid::AudioCodecs);
	if (!m_audioCodecsCharacteristic || !m_audioCodecsCharacteristic->isValid()) {
		qWarning("failed to get audio codecs characteristic");
		return false;
	}

	// set the timeout to two slave latencies, rather than the full 30 seconds
	m_audioCodecsCharacteristic->setTimeout(11000);

	return true;
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Attempts to create a proxy to the GATT interface for the Audio Gain
	characteristic, returns \c true on success and \c false on failure.

 */
bool GattAudioService::getAudioGainCharacteristic(const QSharedPointer<const BleGattService> &gattService)
{
	// don't re-create if we already have valid proxies
	if (m_audioGainCharacteristic && m_audioGainCharacteristic->isValid())
		return true;

	// get the chararacteristic for the audio control
	m_audioGainCharacteristic = gattService->characteristic(BleUuid::AudioGain);
	if (!m_audioGainCharacteristic || !m_audioGainCharacteristic->isValid()) {
		qWarning("failed to get audio gain characteristic");
		return false;
	}

	// set the timeout to two slave latencies, rather than the full 30 seconds
	m_audioGainCharacteristic->setTimeout(11000);

	return true;
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Attempts to create a proxy to the GATT interface for the Audio Control
	characteristic, returns \c true on success and \c false on failure.

 */
bool GattAudioService::getAudioControlCharacteristic(const QSharedPointer<const BleGattService> &gattService)
{
	// don't re-create if we already have valid proxies
	if (m_audioCtrlCharacteristic && m_audioCtrlCharacteristic->isValid())
		return true;

	// get the chararacteristic for the audio control
	m_audioCtrlCharacteristic = gattService->characteristic(BleUuid::AudioControl);
	if (!m_audioCtrlCharacteristic || !m_audioCtrlCharacteristic->isValid()) {
		qWarning("failed to get audio control characteristic");
		return false;
	}

	return true;
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Attempts to create a proxy to the GATT interface for the Audio Data
	characteristic, returns \c true on success and \c false on failure.

 */
bool GattAudioService::getAudioDataCharacteristic(const QSharedPointer<const BleGattService> &gattService)
{
	// don't re-create if we already have valid proxies
	if (m_audioDataCharacteristic && m_audioDataCharacteristic->isValid())
		return true;

	// get the chararacteristic for the audio data
	m_audioDataCharacteristic = gattService->characteristic(BleUuid::AudioData);
	if (!m_audioDataCharacteristic || !m_audioDataCharacteristic->isValid()) {
		qWarning("failed to get audio data characteristic");
		return false;
	}

	// nb: we don't need to bother getting the client configuration
	// chararacteristic descriptor (CCCD) as bluez automatically sets the
	// values when notifications are enabled


	// connect to the value change notification, this is how we get the audio
	// data notification packets which make up the frame
	QObject::connect(m_audioDataCharacteristic.data(),
	                 &BleGattCharacteristic::valueChanged,
	                 this, &GattAudioService::onAudioDataNotification);

	return true;
}

// -----------------------------------------------------------------------------
/*!
	Starts the service by sending a initial request for the audio to stop
	streaming.  When a reply is received we signal that this service is now
	ready.
 */
bool GattAudioService::start(const QSharedPointer<const BleGattService> &gattService)
{
	// sanity check the supplied info is valid
	if (!gattService->isValid() || (gattService->uuid() != m_serviceUuid)) {
		qWarning("invalid skyq voice gatt service info");
		return false;
	}

	// get the dbus proxies to the audio characteristics
	if (!getAudioGainCharacteristic(gattService) ||
	    !getAudioControlCharacteristic(gattService) ||
	    !getAudioDataCharacteristic(gattService) ||
	    !getAudioCodecsCharacteristic(gattService)) {
		qWarning("failed to get one or more gatt characteristics");
		return false;
	}

	requestGainLevel();
	requestAudioCodecs();

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
	Stops the service

 */
void GattAudioService::stop()
{
	// if currently streaming then set the last error as 'disconnected'
	if (m_stateMachine.inState(StreamingSuperState))
		m_lastStats.lastError = DeviceDisconnectedError;

	// post the event to move the state machine
	m_stateMachine.postEvent(StopServiceRequestEvent);
}

// -----------------------------------------------------------------------------
/*!
	\reimp

 */
bool GattAudioService::isReady() const
{
	return m_stateMachine.inState( { ReadyState, StreamingSuperState } );
}

// -----------------------------------------------------------------------------
/*!
	\reimp

 */
bool GattAudioService::isStreaming() const
{
	return m_stateMachine.inState(StreamingState);
}

// -----------------------------------------------------------------------------
/*!
	\internal

 */
void GattAudioService::onEnteredState(int state)
{
	switch (state) {
		case IdleState:
			if (m_audioDataCharacteristic) {
				qInfo() << "Disabling notifications for m_audioDataCharacteristic";
				m_audioDataCharacteristic->enableNotifications(false);
			}
			m_audioGainCharacteristic.reset();
			m_audioCtrlCharacteristic.reset();
			m_audioDataCharacteristic.reset();
			break;

		case ReadyState:
			emit ready();
			break;

		case EnableNotificationsState:
			onEnteredEnableNotificationsState();
			break;

		case StartStreamingState:
			onEnteredStartStreamingState();
			break;
		case StreamingState:
			onEnteredStreamingState();
			break;
		case StopStreamingState:
			onEnteredStopStreamingState();
			break;

		default:
			// don't care about other states
			break;
	}
}

// -----------------------------------------------------------------------------
/*!
	\internal

 */
void GattAudioService::onExitedState(int state)
{
	switch (state) {
		case StreamingState:
			onExitedStreamingState();
		case StreamingSuperState:
			onExitedStreamingSuperState();
			break;

		default:
			// don't care about other states
			break;
	}
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Private slot called upon entry to the 'enable notifications' state, this is
	where we ask the system to set the CCCD and start funneling notifications to
	the app.

 */
void GattAudioService::onEnteredEnableNotificationsState()
{
	// lambda called if an error occurs enabling the notifications
	const std::function<void(const QString&, const QString&)> errorCallback =
		[this](const QString &errorName, const QString &errorMessage)
		{
			// this is bad if this happens as basically voice search won't
			// work
			qError() << "failed to enable audio notifications due to"
			         << errorName << errorMessage;

			m_lastStats.lastError = StreamingError::InternalError;
			m_stateMachine.postEvent(GattErrorEvent);
		};

	// lamda called if notifications are successifully enabled
	const std::function<void()> successCallback =
		[this](void)
		{
			// notifications enabled so post an event to the state machine
			m_stateMachine.postEvent(NotificationsEnabledEvent);
		};


	// send a request to set CCCD for audio data characteristic
	Future<> result = m_audioDataCharacteristic->enableNotifications(true);
	if (!result.isValid() || result.isError()) {
		errorCallback(result.errorName(), result.errorMessage());

	} else if (result.isFinished()) {
		successCallback();

	} else {
		// connect functors to the future async completion
		result.connectErrored(this, errorCallback);
		result.connectFinished(this, successCallback);
	}
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Called on entry to the start streaming state, here we need to perform the
	GATT write to enable the audio streaming on the RCU.

 */
void GattAudioService::onEnteredStartStreamingState()
{
	// lambda called if an error occurs writing the characteristic
	const std::function<void(const QString&, const QString&)> errorCallback =
		[this](const QString &errorName, const QString &errorMessage)
		{
			qWarning() << "failed to write audio control characteristic due to"
			           << errorName << errorMessage;

			m_lastStats.lastError = StreamingError::InternalError;
			m_stateMachine.postEvent(GattErrorEvent);
		};

	// lamda called if the characterisitc write was successiful
	const std::function<void()> successCallback =
		[this](void)
		{
			// notifications enabled so post an event to the state machine
			m_stateMachine.postEvent(StreamingStartedEvent);
		};


	// the first byte is the codec to use, the second byte is to enable voice
	const char rawValue[2] = { 0x01, 0x01 };
	const QByteArray value(rawValue, 2);

	// send a write request to write the control characteristic
	Future<> result = m_audioCtrlCharacteristic->writeValueWithoutResponse(value);
	if (!result.isValid() || result.isError()) {
		errorCallback(result.errorName(), result.errorMessage());
		return;
	}

	// connect functors to the future async completion
	result.connectErrored(this, errorCallback);
	result.connectFinished(this, successCallback);
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Called on entry to the streaming state, here we just tell the audio pipe to
	start streaming and complete the promise.

 */
void GattAudioService::onEnteredStreamingState()
{
	// sanity check we have an audio output pipe
	if (Q_UNLIKELY(!m_audioPipe)) {
		qError("odd, no audio pipe already created");
		m_lastStats.lastError = StreamingError::InternalError;
		m_stateMachine.postEvent(GattErrorEvent);
		return;
	}

/*
	if (Q_UNLIKELY(m_audioPipe))
		qError("odd, audio pipe already created ?");

	// create a new audio pipe for the client
	m_audioPipe = QSharedPointer<GattAudioPipe>::create(m_pipeWriteFd);
	if (!m_audioPipe || !m_audioPipe->isValid()) {
		m_audioPipe.reset();
		m_lastStats.lastError = StreamingError::InternalError;
		m_stateMachine.postEvent(GattErrorEvent);
		return;
	}
*/

	if (Q_UNLIKELY(!m_audioPipe->isOutputOpen()))
		qError("output pipe closed before streaming started");


	// connect to the closed signal from the client audio pipe
	QObject::connect(m_audioPipe.data(), &GattAudioPipe::outputPipeClosed,
	                 this, &GattAudioService::onOutputPipeClosed,
	                 Qt::QueuedConnection);

	// and finally start the audio pipe
	m_audioPipe->start();



	// complete the pending operation with a positive result
	if (m_startStreamingPromise) {
		m_startStreamingPromise->setFinished(m_audioPipe->takeOutputReadFd());
		m_startStreamingPromise.reset();
	} else if (m_startStreamingToPromise) {
		m_startStreamingToPromise->setFinished();
		m_startStreamingToPromise.reset();;
	} else {
		qError("odd, missing promise to send the reply to");
	}

	// schedule a timeout event for automatically cancelling the voice search
	// after 30 seconds
	m_timeoutEventId = m_stateMachine.postDelayedEvent(StopStreamingRequestEvent, 30000);

	// Once streaming data is actually received, emit the streamingChanged signal a single time
	m_emitOneTimeStreamingSignal = true;
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Called on exit from the streaming state, here we just tell the audio pipe to
	stop streaming.

 */
void GattAudioService::onExitedStreamingState()
{
	// destroy the audio pipe
	if (Q_UNLIKELY(!m_audioPipe)) {
		qError("odd, audio pipe not created ?");
	} else {

		// before destruction get the frame stats
		m_audioPipe->stop();
		m_lastStats.actualPackets = m_audioPipe->framesReceived() * m_packetsPerFrame;
		m_lastStats.expectedPackets = m_audioPipe->framesExpected() * m_packetsPerFrame;
		m_lastStats.expectedPackets = qMax(m_lastStats.expectedPackets,
		                                   m_lastStats.actualPackets);

		qInfo("audio frame stats: actual=%u, expected=%u",
		      m_lastStats.actualPackets, m_lastStats.expectedPackets);

		// destroy the audio pipe (closes all file handles)
		m_audioPipe.reset();
	}

	// cancel the timeout event
	if (m_timeoutEventId >= 0) {
		m_stateMachine.cancelDelayedEvent(m_timeoutEventId);
		m_timeoutEventId = -1;
	}

	// tell anyone who cares that streaming has stopped, but only if we've received actual
	// audio data and streamingChanged(true) was previously signaled
	if (!m_emitOneTimeStreamingSignal) {
		emit streamingChanged(false);
	}
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Private slot called when the user requests to stop streaming or the output
	pipe has been closed and therefore we've entered the 'stop streaming' state.
	In this state we just send a request to the RCU to stop streaming.

 */
void GattAudioService::onEnteredStopStreamingState()
{
	// lambda called if an error occurs writing the characteristic
	const std::function<void(const QString&, const QString&)> errorCallback =
		[this](const QString &errorName, const QString &errorMessage)
		{
			qWarning() << "failed to write audio control characteristic due to"
			           << errorName << errorMessage;

			m_lastStats.lastError = StreamingError::InternalError;
			m_stateMachine.postEvent(GattErrorEvent);
		};

	// lamda called if the characterisitc write was successiful
	const std::function<void()> successCallback =
		[this](void)
		{
			// notifications enabled so post an event to the state machine
			m_stateMachine.postEvent(StreamingStoppedEvent);
		};


	// the first byte is the codec to use, the second byte is to disable voice
	const char rawValue[2] = { 0x01, 0x00 };
	const QByteArray value(rawValue, 2);

	// send a write request to write the control characteristic
	Future<> result = m_audioCtrlCharacteristic->writeValueWithoutResponse(value);
	if (!result.isValid() || result.isError()) {
		errorCallback(result.errorName(), result.errorMessage());
		return;
	}

	// connect functors to the future async completion
	result.connectErrored(this, errorCallback);
	result.connectFinished(this, successCallback);
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Private slot called when the state machine leaves the 'streaming super'
	state. In this state we destroy the output fifo as the streaming has stopped.
 */
void GattAudioService::onExitedStreamingSuperState()
{
	// close the streaming pipe (if we haven't already)
	if (m_audioPipe) {
		m_lastStats.actualPackets = m_audioPipe->framesReceived() * m_packetsPerFrame;
		m_lastStats.expectedPackets = m_audioPipe->framesExpected() * m_packetsPerFrame;
		m_lastStats.expectedPackets = qMax(m_lastStats.expectedPackets,
		                                   m_lastStats.actualPackets);

		m_audioPipe.reset();

		qInfo("audio frame stats: actual=%u, expected=%u",
		      m_lastStats.actualPackets, m_lastStats.expectedPackets);
	}

	// complete any promises that may still be outstanding
	if (m_stopStreamingPromise) {
		m_stopStreamingPromise->setFinished();
		m_stopStreamingPromise.reset();
	}

	if (m_startStreamingPromise) {
		m_startStreamingPromise->setError(BleRcuError::errorString(BleRcuError::General),
		                                  QStringLiteral("Streaming stopped"));
		m_startStreamingPromise.reset();
	} else if (m_startStreamingToPromise) {
		m_startStreamingToPromise->setError(BleRcuError::errorString(BleRcuError::General),
		                                    QStringLiteral("Streaming stopped"));
		m_startStreamingToPromise.reset();
	}
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Called when a notification is received from the Audio Data characteristic.

 */
void GattAudioService::onAudioDataNotification(const QByteArray &value)
{
	// This way the streamingChanged signal is emitted only when we actually receive
	// audio data.  But this should only be emitted for the first notification.
	if (m_emitOneTimeStreamingSignal) {
		emit streamingChanged(true);
		m_emitOneTimeStreamingSignal = false;
	}

	// all notifications from the audio pipe should be 20 bytes in size, any
	// other size is an error
	if (Q_UNLIKELY(value.size() != 20)) {
		qWarning("audio data notification not 20 bytes in size (%d bytes)",
		         value.size());
		return;
	}

	// add the notification to the audio pipe
	if (m_audioPipe)
		m_audioPipe->addNotification(reinterpret_cast<const quint8*>(value.constData()));
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Called when AS has closed the voice audio pipe, it does this when it wants
	to stop the audio streaming.

 */
void GattAudioService::onOutputPipeClosed()
{
	qInfo("audio output pipe closed");

	m_stateMachine.postEvent(OutputPipeCloseEvent);
}

// -----------------------------------------------------------------------------
/*!
	\overload

	The gain level is currently not used in a real production box, however
	just in case for debugging we get the gain level as a blocking operation.

	\note This may block for the slave latency period, which is 5 seconds.

 */
quint8 GattAudioService::gainLevel() const
{
	return m_gainLevel;
}

// -----------------------------------------------------------------------------
/*!
	\overload
	Audio Codecs bit mask listing the codecs supported by the remote
 */
quint32 GattAudioService::audioCodecs() const
{
	return m_audioCodecs;
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Sends a request to org.bluez.GattCharacteristic1.Value() to get the value
	propery of the characteristic which contains the current gain level.

 */
void GattAudioService::requestGainLevel()
{
	// lambda called if an error occurs enabling the notifications
	const std::function<void(const QString&, const QString&)> errorCallback =
		[this](const QString &errorName, const QString &errorMessage)
		{
			qError() << "failed to get gain level due to"
			         << errorName << errorMessage;
		};

	// lamda called if notifications are successifully enabled
	const std::function<void(const QByteArray &value)> successCallback =
		[this](const QByteArray &value)
		{
			// sanity check the data received
			if (value.length() != 1) {
				qError("gain value received has invalid length (%d bytes)",
					value.length());
			} else {
				m_gainLevel = static_cast<quint8>(value.at(0));
				qInfo() << "Successfully read from RCU gain level =" << m_gainLevel;
				emit gainLevelChanged(m_gainLevel);
			}
		};


	// send a request to the bluez daemon to start notifing us of battery
	// level changes
	Future<QByteArray> result = m_audioGainCharacteristic->readValue();
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

	Sends a request to org.bluez.GattCharacteristic1.Value() to get the value
	propery of the characteristic which contains the current audio codec.

 */
void GattAudioService::requestAudioCodecs()
{
	// lambda called if an error occurs enabling the notifications
	const std::function<void(const QString&, const QString&)> errorCallback =
		[this](const QString &errorName, const QString &errorMessage)
		{
			qError() << "failed to get audio codec due to"
			         << errorName << errorMessage;
		};

	// lamda called if notifications are successifully enabled
	const std::function<void(const QByteArray &value)> successCallback =
		[this](const QByteArray &value)
		{
			// sanity check the data received
			if (value.length() != sizeof(m_audioCodecs)) {
				qError("audio codec received has invalid length (%d bytes)",
					value.length());
			} else {
				memcpy(&m_audioCodecs, value.constData(), sizeof(m_audioCodecs));
				qInfo().nospace() << "Successfully read from RCU audio codecs bit mask = 0x" << hex << m_audioCodecs;
				emit audioCodecsChanged(m_audioCodecs);
			}
		};


	// send a request to the bluez daemon for characteristic value
	Future<QByteArray> result = m_audioCodecsCharacteristic->readValue();
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
	\overload

	The gain level is currently not used in a real production box, however
	just in case for debugging we get the gain level as a blocking operation.

	\note This may block for the slave latency period, which is 5 seconds.

 */
void GattAudioService::setGainLevel(quint8 level)
{
	// check if the service is running, if not give up
	if (!m_audioGainCharacteristic || m_stateMachine.inState(IdleState))
		return;

	// lambda called if an error occurs disabling find me
	const std::function<void(const QString&, const QString&)> errorCallback =
		[this](const QString &errorName, const QString &errorMessage)
		{
			qError() << "failed to write audio gain level due to" << errorName << errorMessage;
		};

	// lamda called findme was disabled
	const std::function<void()> successCallback =
		[this]()
		{
			qInfo() << "successfully wrote audio gain level, reading new value";
			requestGainLevel();
		};


	const QByteArray value(1, level);
	Future<> result = m_audioGainCharacteristic->writeValue(value);
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

	Utility to create an immediate errored future object.
 */
template<typename T>
Future<T> GattAudioService::createErrorResult(BleRcuError::ErrorType type,
                                              const QString &message) const
{
	return Future<T>::createErrored(BleRcuError::errorString(type), message);
}

// -----------------------------------------------------------------------------
/*!
	\overload

 */
Future<FileDescriptor> GattAudioService::startStreaming(Encoding encoding)
{
	// check the current state
	if (m_stateMachine.state() != ReadyState)
		return createErrorResult<FileDescriptor>(BleRcuError::Busy,
		                                         QStringLiteral("Service not ready"));

	// check we don't already have an outstanding pending call
	if (m_startStreamingPromise || m_startStreamingToPromise || m_stopStreamingPromise)
		return createErrorResult<FileDescriptor>(BleRcuError::Busy,
		                                         QStringLiteral("Service is busy"));

	// clear the last stats
	m_lastStats.lastError = NoError;
	m_lastStats.actualPackets = 0;
	m_lastStats.expectedPackets = 0;


	// check the encoding
	GattAudioPipe::OutputEncoding outputEncoding = GattAudioPipe::PCM16;
	switch (encoding) {
		case BleRcuAudioService::PCM16:
			outputEncoding = GattAudioPipe::PCM16;
			break;
		case BleRcuAudioService::ADPCM:
			outputEncoding = GattAudioPipe::ADPCM;
			break;
		default:
		m_lastStats.lastError = StreamingError::InternalError;
		return createErrorResult<FileDescriptor>(BleRcuError::InvalidArg,
		                                         QStringLiteral("Unsupported audio encoding"));
	}


	// create a new audio pipe for the client
	m_audioPipe = QSharedPointer<GattAudioPipe>::create(outputEncoding);
	if (!m_audioPipe || !m_audioPipe->isValid()) {
		m_audioPipe.reset();
		m_lastStats.lastError = StreamingError::InternalError;
		return createErrorResult<FileDescriptor>(BleRcuError::InvalidArg,
		                                         QStringLiteral("Failed to create streaming pipe"));
	}


	// create a promise to store the result of the streaming
	m_startStreamingPromise = QSharedPointer< Promise<FileDescriptor> >::create();
	Future<FileDescriptor> futureResult = m_startStreamingPromise->future();


	// post a message to the state machine to start moving into the streaming
	// state
	m_stateMachine.postEvent(StartStreamingRequestEvent);

	// return the result (it may have already completed)
	return futureResult;
}

// -----------------------------------------------------------------------------
/*!
	\overload

 */
Future<> GattAudioService::startStreamingTo(Encoding encoding, int fd)
{
	// check the current state
	if (m_stateMachine.state() == IdleState)
		return createErrorResult<>(BleRcuError::Busy,
		                           QStringLiteral("Service not ready"));

	// check we're not already stream or we already have an outstanding pending
	// start/stop streaming call
	if (m_stateMachine.inState(StreamingSuperState) ||
	    (m_startStreamingPromise || m_startStreamingToPromise || m_stopStreamingPromise))
		return createErrorResult<>(BleRcuError::Busy,
		                           QStringLiteral("Service is busy"));

	// clear the last stats
	m_lastStats.lastError = NoError;
	m_lastStats.actualPackets = 0;
	m_lastStats.expectedPackets = 0;


	// check the encoding
	GattAudioPipe::OutputEncoding outputEncoding = GattAudioPipe::PCM16;
	switch (encoding) {
		case BleRcuAudioService::PCM16:
			outputEncoding = GattAudioPipe::PCM16;
			break;
		case BleRcuAudioService::ADPCM:
			outputEncoding = GattAudioPipe::ADPCM;
			break;
		default:
		m_lastStats.lastError = StreamingError::InternalError;
		return createErrorResult<>(BleRcuError::InvalidArg,
		                           QStringLiteral("Unsupported audio encoding"));
	}


	// create a new audio pipe for the client
	m_audioPipe = QSharedPointer<GattAudioPipe>::create(outputEncoding, fd);
	if (!m_audioPipe || !m_audioPipe->isValid()) {
		m_audioPipe.reset();
		m_lastStats.lastError = StreamingError::InternalError;
		return createErrorResult<>(BleRcuError::InvalidArg,
		                           QStringLiteral("Failed to create streaming pipe"));
	}


	// create a promise to store the result of the streaming
	m_startStreamingToPromise = QSharedPointer< Promise<> >::create();
	Future<> futureResult = m_startStreamingToPromise->future();


	// post a message to the state machine to start moving into the streaming
	// state
	m_stateMachine.postEvent(StartStreamingRequestEvent);

	// return the result (it may have already completed)
	return futureResult;
}

// -----------------------------------------------------------------------------
/*!
	\overload

 */
Future<> GattAudioService::stopStreaming()
{
	// check the service is ready
	if (m_stateMachine.state() != StreamingState)
		return createErrorResult(BleRcuError::Busy,
		                         QStringLiteral("Service not currently streaming"));

	// check we don't already have an outstanding pending call
	if (m_startStreamingPromise || m_startStreamingToPromise || m_stopStreamingPromise)
		return createErrorResult(BleRcuError::Busy, QStringLiteral("Service is busy"));


	// create a promise to store the result of the streaming
	m_stopStreamingPromise = QSharedPointer< Promise<> >::create();
	Future<> futureResult = m_stopStreamingPromise->future();

	// post a message to the state machine
	m_stateMachine.postEvent(StopStreamingRequestEvent);

	return futureResult;
}

// -----------------------------------------------------------------------------
/*!
	\overload
 
	Returns the status of the current or previous audio recording.  Because this
	API can return to previous state, this object must store the last error
	state even across the service starting / stopping.

 */
Future<BleRcuAudioService::StatusInfo> GattAudioService::status()
{
	StatusInfo info;

	// if the audio pipe is still alive then use the lastest stats from that
	if (m_audioPipe) {
		info.lastError = NoError;
		info.actualPackets = m_audioPipe->framesReceived() * m_packetsPerFrame;
		info.expectedPackets = m_audioPipe->framesExpected() * m_packetsPerFrame;
		info.expectedPackets = qMax(info.expectedPackets, info.actualPackets);

	} else {
		// otherwise use the stored stats
		info = m_lastStats;

	}

	return Future<StatusInfo>::createFinished(info);
}

