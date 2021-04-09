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
//  bleconnectionparameters.cpp
//  SkyBluetoothRcu
//

#include "bleconnectionparameters.h"


// -----------------------------------------------------------------------------
/*!
	\class BleConnectionParameters
	\brief The \l{BleConnectionParameters} class is used when requesting or
	reporting an update of the parameters of a Bluetooth LE connection.

	The HciConnectionParameters class is used when requesting or reporting an
	update of the parameters of a Bluetooth LE connection.

	\sa HciSocket::requestConnectionUpdate
	\sa HciSocket::connectionCompleted, HciSocket::connectionUpdated
 */

void BleConnectionParameters::registerType()
{
	static int id = -1;

	if (Q_UNLIKELY(id < 0)) {
		int id_ = qRegisterMetaType<BleConnectionParameters>();
		__sync_bool_compare_and_swap(&id, -1, id_);
	}
}

BleConnectionParameters::BleConnectionParameters()
	: m_maxInterval(70.0f)
	, m_minInterval(50.0f)
	, m_latency(499)
	, m_supvTimeout(420)
{
	registerType();
}

BleConnectionParameters::BleConnectionParameters(double minInterval,
                                                 double maxInterval,
                                                 int latency,
                                                 int supervisionTimeout)
	: m_maxInterval(70.0f)
	, m_minInterval(50.0f)
	, m_latency(499)
	, m_supvTimeout(420)
{
	registerType();

	setIntervalRange(minInterval, maxInterval);
	setLatency(latency);
	setSupervisionTimeout(supervisionTimeout);
}

BleConnectionParameters::BleConnectionParameters(const BleConnectionParameters &other)
	: m_maxInterval(other.m_maxInterval)
	, m_minInterval(other.m_minInterval)
	, m_latency(other.m_latency)
	, m_supvTimeout(other.m_supvTimeout)
{
	registerType();
}

BleConnectionParameters::~BleConnectionParameters()
{
}

BleConnectionParameters &BleConnectionParameters::operator=(const BleConnectionParameters &other)
{
	m_maxInterval = other.m_maxInterval;
	m_minInterval = other.m_minInterval;
	m_latency = other.m_latency;
	m_supvTimeout = other.m_supvTimeout;
	return *this;
}

bool BleConnectionParameters::operator!=(const BleConnectionParameters &other) const
{
	return !qFuzzyCompare(m_maxInterval, other.m_maxInterval) ||
	       !qFuzzyCompare(m_minInterval, other.m_minInterval) ||
	       (m_latency != other.m_latency) ||
	       (m_supvTimeout != other.m_supvTimeout);
}

bool BleConnectionParameters::operator==(const BleConnectionParameters &other) const
{
	return qFuzzyCompare(m_maxInterval, other.m_maxInterval) &&
	       qFuzzyCompare(m_minInterval, other.m_minInterval) &&
	       (m_latency == other.m_latency) &&
	       (m_supvTimeout == other.m_supvTimeout);
}

int BleConnectionParameters::latency() const
{
	return m_latency;
}

void BleConnectionParameters::setLatency(int latency)
{
	if ((latency < 0) || (latency > 499)) {
		qWarning("invalid latency value");
		return;
	}

	m_latency = latency;
}

double BleConnectionParameters::maximumInterval() const
{
	return m_maxInterval;
}

double BleConnectionParameters::minimumInterval() const
{
	return m_minInterval;
}

void BleConnectionParameters::setIntervalRange(double minimum, double maximum)
{
	if ((minimum < 7.5f) || (minimum > 4000.0f)) {
		qWarning("invalid minimum connection interval value");
		return;
	}
	if ((maximum < 7.5f) || (maximum > 4000.0f)) {
		qWarning("invalid maximum connection interval value");
		return;
	}

	if (maximum < minimum)
		maximum = minimum;

	m_minInterval = minimum;
	m_maxInterval = maximum;
}

void BleConnectionParameters::setSupervisionTimeout(int timeout)
{
	if ((timeout < 100) || (timeout > 32000)) {
		qWarning("invalid supervision timeout value");
		return;
	}

	m_supvTimeout = timeout;
}

int BleConnectionParameters::supervisionTimeout() const
{
	return m_supvTimeout;
}

QDebug operator<<(QDebug dbg, const BleConnectionParameters &params)
{
	QDebugStateSaver saver(dbg);
	dbg.setAutoInsertSpaces(false);

	const double max = params.maximumInterval();
	const double min = params.minimumInterval();

	if (qFuzzyCompare(max, min))
		dbg << "BleConnectionParameters(interval=" << max;
	else
		dbg << "BleConnectionParameters(interval=" << max << "-" << min;

	dbg << ", latency=" << params.latency();
	dbg << ", timeout=" << params.supervisionTimeout();
	dbg << ")";

	return dbg;
}


