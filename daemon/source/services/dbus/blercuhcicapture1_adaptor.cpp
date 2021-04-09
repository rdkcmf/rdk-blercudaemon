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
//  blercuhcicapture1_adaptor.cpp
//  SkyBluetoothRcu
//

#include "blercuhcicapture1_adaptor.h"
#include "blercu/blercuerror.h"
#include "monitors/hcimonitor.h"
#include "utils/logging.h"


#define HCI_MONITOR_BUFSIZE   size_t(8 * 1024 * 1024)


BleRcuHciCapture1Adaptor::BleRcuHciCapture1Adaptor(QObject *parent,
                                                   const QDBusObjectPath &objPath,
                                                   int networkNamespaceFd)
	: DBusAbstractAdaptor(parent)
	, m_dbusObjPath(objPath)
	, m_networkNamespace(networkNamespaceFd)
	, m_hciMonitor(nullptr)
{
	// don't auto relay signals, we don't have any signals
	setAutoRelaySignals(false);


	// create the monitor, this also starts it
	m_hciMonitor = new HciMonitor(0, m_networkNamespace.fd(), HCI_MONITOR_BUFSIZE);
	if (!m_hciMonitor->isValid()) {
		delete m_hciMonitor;
		m_hciMonitor = nullptr;
	}
}

BleRcuHciCapture1Adaptor::~BleRcuHciCapture1Adaptor()
{
	if (m_hciMonitor)
		delete m_hciMonitor;
}

// -----------------------------------------------------------------------------
/*!
	DBus get property call for com.sky.blercu.HciCapture1.Capturing

 */
bool BleRcuHciCapture1Adaptor::isCapturing() const
{
	return (m_hciMonitor != nullptr);
}

// -----------------------------------------------------------------------------
/*!
	DBus method call for com.sky.blercu.HciCapture1.Enable

 */
void BleRcuHciCapture1Adaptor::Enable(const QDBusMessage &message)
{
	// sanity check the monitor is not already running
	if (m_hciMonitor) {
		sendErrorReply(message, BleRcuError::errorString(BleRcuError::General),
		               QStringLiteral("HCI monitor already enabled"));
		return;
	}

	// create the monitor
	m_hciMonitor = new HciMonitor(0, m_networkNamespace.fd(), HCI_MONITOR_BUFSIZE);
	if (!m_hciMonitor->isValid()) {

		delete m_hciMonitor;
		m_hciMonitor = nullptr;

		sendErrorReply(message, BleRcuError::errorString(BleRcuError::General),
		               QStringLiteral("Failed to enable monitor"));
		return;
	}

	// send a property change on the capture state
	sendPropertyChangeNotification<bool>(m_dbusObjPath.path(),
	                                     QStringLiteral("Capturing"), true);

	// success - qt / dbus will send a positive reply
}

// -----------------------------------------------------------------------------
/*!
	DBus method call for com.sky.blercu.HciCapture1.Disable


 */
void BleRcuHciCapture1Adaptor::Disable(const QDBusMessage &message)
{
	// sanity check we are actually monitoring
	if (!m_hciMonitor) {
		sendErrorReply(message, BleRcuError::errorString(BleRcuError::General),
		               QStringLiteral("HCI monitor not enabled"));
		return;
	}

	// delete the monitor which will clean everything up and stop monitoring
	delete m_hciMonitor;
	m_hciMonitor = nullptr;

	// send a property change on the capture state
	sendPropertyChangeNotification<bool>(m_dbusObjPath.path(),
	                                     QStringLiteral("Capturing"), false);

	// success - qt / dbus will send a positive reply
}

// -----------------------------------------------------------------------------
/*!
	DBus method call for com.sky.blercu.HciCapture1.Clear


 */
void BleRcuHciCapture1Adaptor::Clear(const QDBusMessage &message)
{
	// sanity check the monitor is not already running
	if (!m_hciMonitor) {
		sendErrorReply(message, BleRcuError::errorString(BleRcuError::General),
		               QStringLiteral("HCI monitor not enabled"));
		return;
	}

	// ask to clear the buffer
	m_hciMonitor->clear();

	// success - qt / dbus will send a positive reply
}

// -----------------------------------------------------------------------------
/*!
	DBus method call for com.sky.blercu.HciCapture1.Dump

 */
void BleRcuHciCapture1Adaptor::Dump(QDBusUnixFileDescriptor file,
                                    const QDBusMessage &message)
{
	// sanity check the monitor is not already running
	if (!m_hciMonitor) {
		sendErrorReply(message, BleRcuError::errorString(BleRcuError::General),
		               QStringLiteral("HCI monitor not enabled"));
		return;
	}

	// check the supplied file descriptor
	if (!file.isValid()) {
		sendErrorReply(message, BleRcuError::errorString(BleRcuError::FileNotFound),
		               QStringLiteral("Invalid file descriptor"));
		return;
	}

	// gift the file descriptor to a file object (even though it can be a
	// socket or pipe)
	QFile dumpFile;
	if (!dumpFile.open(file.fileDescriptor(), QFile::WriteOnly)) {
		sendErrorReply(message, BleRcuError::errorString(BleRcuError::FileNotFound),
		               QStringLiteral("Failed to access file descriptor"));
		return;
	}

	// finally dump the buffer contents to the file (but don't clear it)
	if (m_hciMonitor->dumpBuffer(&dumpFile, true, false) < 0) {
		sendErrorReply(message, BleRcuError::errorString(BleRcuError::FileNotFound),
		               QStringLiteral("Failed to write to the file descriptor"));
	}

	// flush and close the file wrapper
	dumpFile.flush();
	dumpFile.close();

	// success - qt / dbus will send a positive reply
}


