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
//  blercumanager.h
//  SkyBluetoothRcu
//

#ifndef BLERCUADAPTER_H
#define BLERCUADAPTER_H

#include "utils/bleaddress.h"
#include "utils/dumper.h"

#include <QObject>
#include <QString>
#include <QMap>
#include <QSet>
#include <QSharedPointer>

class BleRcuDevice;


class BleRcuAdapter : public QObject
{
	Q_OBJECT

public:
	virtual ~BleRcuAdapter()
	{ }

protected:
	BleRcuAdapter(QObject *parent)
		: QObject(parent)
	{ }

public:
	virtual bool isValid() const = 0;
	virtual bool isAvailable() const = 0;
	virtual bool isPowered() const = 0;

	virtual bool isDiscovering() const = 0;
	virtual bool startDiscovery(int pairingCode) = 0;
	virtual bool stopDiscovery() = 0;

	virtual bool isPairable() const = 0;
	virtual bool enablePairable(int timeout) = 0;
	virtual bool disablePairable() = 0;

	virtual QSet<BleAddress> pairedDevices() const = 0;
	virtual QMap<BleAddress, QString> deviceNames() const = 0;

	virtual QSharedPointer<BleRcuDevice> getDevice(const BleAddress &address) const = 0;

	virtual bool isDevicePaired(const BleAddress &address) const = 0;

	virtual bool addDevice(const BleAddress &address) = 0;
	virtual bool removeDevice(const BleAddress &address) = 0;

	virtual void dump(Dumper out) const = 0;

signals:
	void poweredChanged(bool powered, QPrivateSignal);
	void poweredInitialised(QPrivateSignal);

	void discoveryChanged(bool discovering, QPrivateSignal);
	void pairableChanged(bool pairable, QPrivateSignal);

	void deviceFound(const BleAddress &address, const QString &name, QPrivateSignal);
	void deviceRemoved(const BleAddress &address, QPrivateSignal);

	void deviceNameChanged(const BleAddress &address, const QString &name, QPrivateSignal);
	void devicePairingChanged(const BleAddress &address, bool paired, QPrivateSignal);
	void deviceReadyChanged(const BleAddress &address, bool ready, QPrivateSignal);


protected:
	inline struct QPrivateSignal privateSignal() { return QPrivateSignal(); }

};

#endif // !defined(BLERCUADAPTER_H)
