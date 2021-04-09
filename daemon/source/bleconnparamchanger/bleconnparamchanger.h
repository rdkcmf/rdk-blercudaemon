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
//  blercuconnparamchanger.h
//  BleRcuDaemon
//

#ifndef BLECONNPARAMCHANGER_H
#define BLECONNPARAMCHANGER_H

#include "utils/bleaddress.h"
#include "utils/bleconnectionparameters.h"

#include "utils/hcisocket.h"

#include <QObject>
#include <QMutex>
#include <QMap>
#include <QSharedPointer>


class BleConnParamDevice;


class BleConnParamChanger : public QObject
{
	Q_OBJECT

public:
	BleConnParamChanger(const QSharedPointer<HciSocket> &hciSocket,
	                    int postConnectionTimeout = 30000,
	                    int postUpdateTimeout = 5000,
	                    int retryTimeout = 60000,
	                    int startupTimeout = 1000,
	                    QObject *parent = nullptr);
	~BleConnParamChanger();

public:
	bool isValid() const;

	BleConnectionParameters connectionParamsFor(quint32 deviceOui) const;
	bool setConnectionParamsFor(quint32 deviceOui,
	                            const BleConnectionParameters &params);

	bool start();
	void stop();

private:
	void onConnectionCompleted(quint16 handle, const BleAddress &device,
	                           const BleConnectionParameters &params);
	void onConnectionUpdated(quint16 handle,
	                         const BleConnectionParameters &params);
	void onDisconnectionCompleted(quint16 handle, HciSocket::HciStatus reason);

private:
	const QSharedPointer<HciSocket> m_hciSocket;
	const int m_postConnectionTimeout;
	const int m_postUpdateTimeout;
	const int m_retryTimeout;
	const int m_startupTimeout;

	mutable QMutex m_lock;

	QMap<quint32, BleConnectionParameters> m_desiredParams;
	QMap<quint16, QSharedPointer<BleConnParamDevice>> m_devices;

};

#endif // !defined(BLECONNPARAMCHANGER_H)
