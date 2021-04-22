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
//  irpairing.cpp
//  SkyBluetoothRcu
//

#include "irpairing.h"

#include "utils/inputdevicemanager.h"
#include "utils/inputdevice.h"
#include "utils/logging.h"
#include "blercu/blercucontroller.h"

// -----------------------------------------------------------------------------
/*!
	\class IrPairing
	\brief Object to listen for pairing IR key events and trigger the pairing
	state machine.

	At creation time this tries to find the linux input event node that
	corresponds to the IR device (typically from uinput), if found then install
	listener for events from the device.  If not then listeners are installed
	to detect when / if the IR input device is added to the kernel.

 */


IrPairing::IrPairing(const QSharedPointer<BleRcuController> &controller,
                     QObject *parent)
	: QObject(parent)
	, m_controller(controller)
	, m_inputDeviceManager(InputDeviceManager::create())
{
	// connect to the events from the Android InputDevice manager
	QObject::connect(m_inputDeviceManager.data(), &InputDeviceManager::deviceAdded,
	                 this, &IrPairing::onInputDeviceAdded,
	                 Qt::QueuedConnection);
	QObject::connect(m_inputDeviceManager.data(), &InputDeviceManager::deviceRemoved,
	                 this, &IrPairing::onInputDeviceRemoved,
	                 Qt::QueuedConnection);

	// try and find the IR input device
	const QList<InputDeviceInfo> inputDevices = m_inputDeviceManager->inputDevices();
	for (const InputDeviceInfo &deviceInfo : inputDevices) {

		qDebug() << deviceInfo;

		if (isIrInputDevice(deviceInfo)) {
			m_irInputDevice = m_inputDeviceManager->getDevice(deviceInfo);
			if (!m_irInputDevice || !m_irInputDevice->isValid()) {
				qWarning() << "failed to open input device" << deviceInfo;
				m_irInputDevice.clear();

			} else {
				QObject::connect(m_irInputDevice.data(), &InputDevice::keyPress,
				                 this, &IrPairing::onIrKeyPress);
				break;
			}
		}
	}

	// it's possible that the IR input device has not yet been loaded, however
	// this is not the typical case and therefore we should log the following
	// error to help with debugging
	if (!m_irInputDevice) {
		qError("failed to find IR input device to monitor for pairing requests");
		for (const InputDeviceInfo &deviceInfo : inputDevices)
			qError() << "possible input devices: " << deviceInfo;
	}

}

// -----------------------------------------------------------------------------
/*!
	\internal

	Checks if the device info corresponds to the IR input device on the current
	platform.

 */
bool IrPairing::isIrInputDevice(const InputDeviceInfo &deviceInfo) const
{
#ifdef RDK
	return deviceInfo.name() == "uinput-remote";
#else
#	error "Unknown RDK platform"
#endif
}

// -----------------------------------------------------------------------------
/*!
	Called when a new input device is added to the system.  We use this to check
	if the IR device has been added and if so create a wrapper for it and
	install a listener for IR key presses.

 */
void IrPairing::onInputDeviceAdded(const InputDeviceInfo &deviceInfo)
{
	if (isIrInputDevice(deviceInfo)) {
		qInfo("infra-red input device '%s' added", qPrintable(deviceInfo.name()));

		m_irInputDevice = m_inputDeviceManager->getDevice(deviceInfo);
		QObject::connect(m_irInputDevice.data(), &InputDevice::keyPress,
		                 this, &IrPairing::onIrKeyPress);
	}
}

// -----------------------------------------------------------------------------
/*!
	Called when a input device has been removed.  Used to determine if the IR
	input device has disappeared, in which case we need to close our wrapper
	object.

 */
void IrPairing::onInputDeviceRemoved(const InputDeviceInfo &deviceInfo)
{
	if (isIrInputDevice(deviceInfo)) {
		qInfo("infra-red input device '%s' removed", qPrintable(deviceInfo.name()));

		m_irInputDevice.clear();
	}
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Called when an input event is received from the IR input device node.

	The format of the scan code follows the Sky RC-6 format which is documented
	in the "Ethan Command codes.docx" document which you should be able to find
	in confluence.

 */
void IrPairing::onIrKeyPress(quint16 keyCode, qint32 scanCode)
{
	const quint8 scc = ((scanCode >> 20) & 0x0f);
	const quint8 filterByte = ((scanCode >> 8) & 0xff);
	const quint8 commandCode = ((scanCode >> 0) & 0xff);

	qDebug("received IR key code %d (scan code 0x%06x : scc=%02hhu, fb=%hhu,"
	       " code=%hhu)", keyCode, scanCode, scc, filterByte, commandCode);

	// check if a pairing IR event
	if (scc == 5) {
		m_controller->startPairing(filterByte, commandCode);
	}
}
