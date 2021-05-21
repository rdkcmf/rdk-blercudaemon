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
//  blercudebug1_adaptor.cpp
//  SkyBluetoothRcu
//

#include "blercudebug1_adaptor.h"
#include "blercu/blercucontroller.h"
#include "utils/logging.h"

#include <QCoreApplication>



BleRcuDebug1Adaptor::BleRcuDebug1Adaptor(QObject *parent,
                                         const QSharedPointer<BleRcuController> &controller)
	: DBusAbstractAdaptor(parent)
	, m_controller(controller)
{
	// don't auto relay signals, we don't have any signals
	setAutoRelaySignals(false);

}

// -----------------------------------------------------------------------------
/*!
	DBus get property call for com.sky.blercu.Debug1.LogToConsole

 */
bool BleRcuDebug1Adaptor::isConsoleEnabled() const
{
	return getLogTargets() & LoggingTarget::Console;
}

void BleRcuDebug1Adaptor::enableConsole(bool enable)
{
	LoggingTargets targets = getLogTargets();
	if (enable) targets |= LoggingTarget::Console;
	else        targets &= ~LoggingTarget::Console;
	setLogTargets(targets);
}

// -----------------------------------------------------------------------------
/*!
	DBus get property call for com.sky.blercu.Debug1.LogToEthanLog

 */
bool BleRcuDebug1Adaptor::isEthanlogEnabled() const
{
	return getLogTargets() & LoggingTarget::EthanLog;
}

void BleRcuDebug1Adaptor::enableEthanlog(bool enable)
{
	LoggingTargets targets = getLogTargets();
	if (enable) targets |= LoggingTarget::EthanLog;
	else        targets &= ~LoggingTarget::EthanLog;
	setLogTargets(targets);
}

// -----------------------------------------------------------------------------
/*!
	DBus get property call for com.sky.blercu.Debug1.LogToSysLog

 */
bool BleRcuDebug1Adaptor::isSyslogEnabled() const
{
	return getLogTargets() & LoggingTarget::SysLog;
}

void BleRcuDebug1Adaptor::enableSyslog(bool enable)
{
	LoggingTargets targets = getLogTargets();
	if (enable) targets |= LoggingTarget::SysLog;
	else        targets &= ~LoggingTarget::SysLog;
	setLogTargets(targets);
}

// -----------------------------------------------------------------------------
/*!
	DBus get property call for com.sky.blercu.Debug1.LogLevels

 */
quint32 BleRcuDebug1Adaptor::logLevels() const
{
	const LoggingLevels levels = getLogLevels();
	return static_cast<quint32>(levels);
}

void BleRcuDebug1Adaptor::setLogLevels(quint32 levels)
{
	LoggingLevels mask = LoggingLevel::Fatal;
	if (levels & 0x002)
		mask |= LoggingLevel::Error;
	if (levels & 0x004)
		mask |= LoggingLevel::Warning;
	if (levels & 0x008)
		mask |= LoggingLevel::Milestone;
	if (levels & 0x010)
		mask |= LoggingLevel::Info;
	if (levels & 0x020)
		mask |= LoggingLevel::Debug;

	::setLogLevels(mask);
}

