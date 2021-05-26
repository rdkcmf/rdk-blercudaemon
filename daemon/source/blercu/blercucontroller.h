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
//  blercucontroller.h
//  SkyBluetoothRcu
//

#ifndef BLERCUCONTROLLER_H
#define BLERCUCONTROLLER_H

#include "utils/bleaddress.h"
#include "blercuerror.h"
#include "utils/dumper.h"

#include <QObject>
#include <QString>
#include <QSharedPointer>
#include <QSet>


class BleRcuDevice;


class BleRcuController : public QObject
{
	Q_OBJECT

public:
	~BleRcuController() override = default;

protected:
	explicit BleRcuController(QObject *parent)
		: QObject(parent)
	{ }

public:
	enum State
	{
		Initialising,
		Idle,
		Searching,
		Pairing,
		Complete,
		Failed
	};

public:
	virtual void dump(Dumper out) const = 0;

	virtual bool isValid() const = 0;
	virtual State state() const = 0;

	virtual BleRcuError lastError() const = 0;

	virtual bool isPairing() const = 0;
	virtual int pairingCode() const = 0;

	virtual bool startPairing(quint8 filterByte, quint8 pairingCode) = 0;
	virtual bool startPairingMacHash(quint8 filterByte, quint8 macHash) = 0;
	virtual bool cancelPairing() = 0;

	virtual bool isScanning() const = 0;
	virtual bool startScanning(int timeoutMs) = 0;
	virtual bool cancelScanning() = 0;

	virtual QSet<BleAddress> managedDevices() const = 0;
	virtual QSharedPointer<BleRcuDevice> managedDevice(const BleAddress &address) const = 0;

	virtual bool unpairDevice(const BleAddress &address) const = 0;
	virtual void disconnectAllDevices() const = 0;

signals:
	void managedDeviceAdded(BleAddress address);
	void managedDeviceRemoved(BleAddress address);
	void scanningStateChanged(bool scanning);
	void pairingStateChanged(bool pairing);
	void stateChanged(State state);
};

#endif // !defined(BLERCUCONTROLLER_H)
