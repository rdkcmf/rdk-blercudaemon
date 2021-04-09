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
//  irpairing.h
//  SkyBluetoothRcu
//

#ifndef IRPAIRING_H
#define IRPAIRING_H

#include <QObject>
#include <QSharedPointer>

class InputDeviceManager;
class InputDevice;
class InputDeviceInfo;
class BleRcuController;


class IrPairing : public QObject
{
public:
	explicit IrPairing(const QSharedPointer<BleRcuController> &controller,
	                   QObject *parent = nullptr);
	~IrPairing() final = default;

private:
	bool isIrInputDevice(const InputDeviceInfo &deviceInfo) const;

	void onInputDeviceAdded(const InputDeviceInfo &deviceInfo);
	void onInputDeviceRemoved(const InputDeviceInfo &deviceInfo);

	void onIrKeyPress(quint16 keyCode, qint32 scanCode);

private:
	const QSharedPointer<BleRcuController> m_controller;
	const QSharedPointer<InputDeviceManager> m_inputDeviceManager;

	QSharedPointer<InputDevice> m_irInputDevice;

};

#endif // !defined(IRPAIRING_H)
