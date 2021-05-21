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
//  blercustatuswebsocket.h
//  SkyBluetoothRcu
//

#ifndef BLERCUSTATUSWEBSOCKET_H
#define BLERCUSTATUSWEBSOCKET_H

#include "utils/bleaddress.h"

#include "blercu/blercuerror.h"
#include "blercu/blercucontroller.h"
#include "blercu/bleservices/blercuaudioservice.h"

#include <QObject>
#include <QMutex>
#include <QSharedPointer>
#include <QJsonObject>
#include <QMap>


#include <functional>


class BleRcuDevice;
class InputDeviceInfo;
class InputDeviceManager;


class BleRcuStatusWebSocket final : public QObject
{
	Q_OBJECT

public:
	explicit BleRcuStatusWebSocket(int asVersion, QObject *parent = nullptr);
	~BleRcuStatusWebSocket() final;

public:
	void setController(const QSharedPointer<BleRcuController> &controller);

private:
	void onControllerStateChanged(BleRcuController::State state);
	void onPairingStateChanged(bool pairing);

	void onDeviceAdded(const BleAddress &address);
	void onDeviceRemoved(const BleAddress &address);

	void onDeviceIdChanged(const BleAddress &address, int deviceId);

	void onDeviceNameChanged(const BleAddress &address, const QString &name);
	void onDeviceReadyChanged(const BleAddress &address, bool ready);
	void onDeviceBatteryLevelChanged(const BleAddress &address, int level);
	void onDeviceSoftwareVersionChanged(const BleAddress &address, const QString &swVersion);

	void addDeviceToStatus(const QSharedPointer<BleRcuDevice> &device);

	void updateDeviceInfo(const QSharedPointer<BleRcuDevice> &device,
	                      QJsonObject *remote);

	void updateDeviceStatus(const BleAddress &bdaddr,
	                        const QString &key,
	                        const QJsonValue &value);

private:
	void onInputDeviceAdded(const InputDeviceInfo &info);
	void onInputDeviceRemoved(const InputDeviceInfo &info);

	void updateDeviceIdMap(const BleAddress &address, int deviceId);

signals:
	void updateWebSocket(const QJsonObject &message);

private:
	void invalidateWebSocket();
	void onInvalidatedWebSocket();

	QString controllerStateString(BleRcuController::State state) const;

private:
	const int m_asVersion;

	QSharedPointer<InputDeviceManager> m_inputDeviceManager;

	QSharedPointer<BleRcuController> m_controller;

	QMap<BleAddress, int> m_deviceIdMap;

	QMutex m_lock;

	BleRcuController::State m_controllerState;

	bool m_pairingInProgress;
	QMap<BleAddress, QJsonObject> m_remotes;

	QJsonObject m_status;
	QJsonObject m_lastStatus;

private:
	const struct
	{
		QString status;
		QString pairinginprogress;
		QString remotes;
		QString bdaddr;
		QString connected;
		QString name;
		QString deviceid;
		QString make;
		QString model;
		QString hwrev;
		QString serialno;
		QString rcuswver;
		QString btlswver;
		QString batterylevel;
	} JSON;
};

#endif // BLERCUSTATUSWEBSOCKET_H
