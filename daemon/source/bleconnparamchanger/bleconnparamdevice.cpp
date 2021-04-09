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
//  bleconnparamdevice.cpp
//  BleRcuDaemon
//

#include "bleconnparamdevice.h"

#include "utils/hcisocket.h"
#include "utils/logging.h"



BleConnParamDevice::BleConnParamDevice(const QSharedPointer<HciSocket> &hciSocket,
                                             quint16 handle,
                                             const BleAddress &address,
                                             const BleConnectionParameters &params,
                                             int postConnectionTimeout,
                                             int postUpdateTimeout,
                                             int retryTimeout,
                                             QObject *parent)
	: QObject(parent)
	, m_hciSocket(hciSocket)
	, m_handle(handle)
	, m_address(address)
	, m_desiredParams(params)
	, m_postConnectionTimeout(postConnectionTimeout)
	, m_postUpdateTimeout(postUpdateTimeout)
	, m_retryTimeout(retryTimeout)
	, m_connParamsOk(true)
	, m_timer(new QTimer(this))
{
	m_timer->setSingleShot(true);

	QObject::connect(m_timer, &QTimer::timeout, this, &BleConnParamDevice::onTimeout);
}

BleConnParamDevice::~BleConnParamDevice()
{
	m_timer->stop();
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Checks if the supplied connection \a params are 'close enough' to our
	desired values.  For 'close enough' the interval must be within the desired
	supplied range, the supervisory timeout should be within 1 second and the
	latency should be within 5%.

 */
bool BleConnParamDevice::connectionParamsCloseEnough(const BleConnectionParameters &params) const
{
	// check the interval is within range
	const double interval = params.minimumInterval();
	if ((interval > m_desiredParams.maximumInterval()) ||
	    (interval < m_desiredParams.minimumInterval()))
		return false;

	// the latency is in a range of 0 - 499
	const int latency = params.latency();
	const int minLatency = m_desiredParams.latency() - 25;
	const int maxLatency = m_desiredParams.latency() + 25;
	if ((latency > maxLatency) || (latency < minLatency))
		return false;

	// the supervisory timeout is a value between 100 and 32000
	const int timeout = params.supervisionTimeout();
	const int minTimeout = m_desiredParams.supervisionTimeout() - 1000;
	const int maxTimeout = m_desiredParams.supervisionTimeout() + 1000;
	if ((timeout > maxTimeout) || (timeout < minTimeout))
		return false;

	return true;
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Called by the \l{HciSocket} object when the driver tells us that a new
	connection has been completed.  The \a device argument is the BDADDR of
	the remote device that connected and \a params contains the current
	connection parameters used for the new connection.

	We first check the connection parameters match our current desired
	parameters, if they don't then we (re)start the timer to fire in 5 seconds
	to update the parameters.  If they do match we stop the timer.

	Typically after an initial connection the remote device will issue a
	request to update it's parameters, this should happen within 5 seconds so
	it's likely the timer won't expire before onConnectionUpdated() is called.
	So this function is mainly used in case the remote device doesn't
	request an update.

 */
void BleConnParamDevice::onConnectionCompleted(const BleConnectionParameters &params)
{
	qMilestone() << m_address << "(" << m_handle << ") connected with params" << params;

	// check if the connection params are ok, if not start a timer to correct
	// them in the future
	m_connParamsOk = connectionParamsCloseEnough(params);
	if (m_connParamsOk) {
		m_timer->stop();

	} else {
		qInfo("connection params don't match our desired parameters, starting a"
		      " timer to update params in %0.1f seconds time",
		      float(m_postConnectionTimeout) / 1000.0f);

		m_timer->start(m_postConnectionTimeout);
	}
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Called by the \l{HciSocket} object when the driver tells us that the
	connection parameters have been updated.  The \a params contains the new
	connection parameters used for the new connection.

	We first check the connection parameters match our current desired
	parameters, if they don't then we (re)start the timer to fire in 2 seconds
	to update the parameters.  If they do match we stop the timer.

 */
void BleConnParamDevice::onConnectionUpdated(const BleConnectionParameters &params)
{
	qMilestone() << m_address << "(" << m_handle << ") params changed to" << params;

	// check if the new connection params are ok
	m_connParamsOk = connectionParamsCloseEnough(params);
	if (m_connParamsOk) {
		m_timer->stop();

	} else {
		qInfo("new connection params don't match our desired parameters, "
		      "starting a timer to update params in %0.1f seconds time",
		      float(m_postUpdateTimeout) / 1000.0f);

		m_timer->start(m_postUpdateTimeout);
	}
}

// -----------------------------------------------------------------------------
/*!
	Should be called when the driver tells us that the connected device has
	disconnected.


 */
void BleConnParamDevice::onDisconnectionCompleted(HciSocket::HciStatus reason)
{
	qMilestone() << m_address << "(" << m_handle << ") disconnected due to" << reason;

	m_timer->stop();
}

// -----------------------------------------------------------------------------
/*!
	Forces a connection parameter update in \a msecs milliseconds.  If
	onConnectionUpdated() is called before the \a msecs period then the update
	may not happen if the parameters in the update match the desired params.

 */
void BleConnParamDevice::triggerUpdate(int msecs)
{
	m_connParamsOk = false;
	m_timer->start(msecs);
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Timer callback, here we check if the connection parameters still don't match
	the desired params and if so ask the \l{HciSocket} object to re-apply our
	parameters.

	This method will restart the timer so it fires in another 5 seconds time, in
	the happy case the timer will be stopped when onConnectionUpdated() is
	called with our desired parameters.  In the unhappy case we don't get the
	update notification so we apply the settings again (in 5 seconds time).

 */
void BleConnParamDevice::onTimeout()
{
	if (!m_connParamsOk && m_hciSocket) {

		qMilestone() << m_address << "(" << m_handle
		             << ") requesting an update of connection parameters to"
		             << m_desiredParams;

		// request the driver to update the connection parameters
		m_hciSocket->requestConnectionUpdate(m_handle, m_desiredParams);

		// (re)start the timer to fire in X seconds, this is just in case the
		// settings don't stick (and we don't get a onConnectionUpdated()
		// callback), in which case we should try applying them again
		m_timer->start(m_retryTimeout);
	}
}

