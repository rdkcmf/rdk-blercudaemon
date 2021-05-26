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
//  blercudevice.h
//  SkyBluetoothRcu
//

#ifndef BLERCUDEVICE_H
#define BLERCUDEVICE_H


#include "utils/bleaddress.h"
#include "utils/dumper.h"

#include <QObject>
#include <QString>
#include <QTextStream>
#include <QSharedPointer>


class BleRcuAudioService;
class BleRcuBatteryService;
class BleRcuDeviceInfoService;
class BleRcuFindMeService;
class BleRcuInfraredService;
class BleRcuTouchService;
class BleRcuUpgradeService;
class BleRcuRemoteControlService;




class BleRcuDevice : public QObject
{
	Q_OBJECT

protected:
	explicit BleRcuDevice(QObject *parent = nullptr)
		: QObject(parent)
	{ }

public:
	~BleRcuDevice() override = default;

public:
	virtual void dump(Dumper out) const = 0;

	virtual bool isValid() const = 0;
	virtual bool isConnected() const = 0;
	virtual bool isPairing() const = 0;
	virtual bool isPaired() const = 0;
	virtual bool isReady() const = 0;

	virtual qint64 msecsSinceReady() const = 0;
	virtual void shutdown() = 0;

	virtual BleAddress address() const = 0;
	virtual QString name() const = 0;

	// virtual Future<qint16> rssi() const = 0;

	virtual int deviceId() const
	{
		return -1;
	}

public:
	virtual QSharedPointer<BleRcuAudioService> audioService() const = 0;
	virtual QSharedPointer<BleRcuBatteryService> batteryService() const = 0;
	virtual QSharedPointer<BleRcuDeviceInfoService> deviceInfoService() const = 0;
	virtual QSharedPointer<BleRcuFindMeService> findMeService() const = 0;
	virtual QSharedPointer<BleRcuInfraredService> infraredService() const = 0;
	virtual QSharedPointer<BleRcuTouchService> touchService() const = 0;
	virtual QSharedPointer<BleRcuUpgradeService> upgradeService() const = 0;
	virtual QSharedPointer<BleRcuRemoteControlService> remoteControlService() const = 0;

public:
	template<typename T>
	QSharedPointer<T> service() const;

signals:
	void connectedChanged(bool connected, QPrivateSignal);
	void pairedChanged(bool paired, QPrivateSignal);
	void nameChanged(const QString &name, QPrivateSignal);
	void readyChanged(bool ready, QPrivateSignal);

protected:
	inline struct QPrivateSignal privateSignal() { return QPrivateSignal(); }

};


#if !defined(Q_QDOC)

template<>
inline QSharedPointer<BleRcuAudioService> BleRcuDevice::service() const
{ return audioService(); }

template<>
inline QSharedPointer<BleRcuBatteryService> BleRcuDevice::service() const
{ return batteryService(); }

template<>
inline QSharedPointer<BleRcuDeviceInfoService> BleRcuDevice::service() const
{ return deviceInfoService(); }

template<>
inline QSharedPointer<BleRcuFindMeService> BleRcuDevice::service() const
{ return findMeService(); }

template<>
inline QSharedPointer<BleRcuInfraredService> BleRcuDevice::service() const
{ return infraredService(); }

template<>
inline QSharedPointer<BleRcuTouchService> BleRcuDevice::service() const
{ return touchService(); }

template<>
inline QSharedPointer<BleRcuUpgradeService> BleRcuDevice::service() const
{ return upgradeService(); }

template<>
inline QSharedPointer<BleRcuRemoteControlService> BleRcuDevice::service() const
{ return remoteControlService(); }

#endif // !defined(Q_QDOC)


QDebug operator<<(QDebug dbg, const BleRcuDevice &device);


#endif // !defined(BLERCUDEVICE_H)
