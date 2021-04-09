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
//  bleconnparamchanger.h
//  BleRcuDaemon
//

#ifndef BLECONNPARAMCDEVICE_H
#define BLECONNPARAMCDEVICE_H

#include "utils/bleaddress.h"
#include "utils/bleconnectionparameters.h"

#include "utils/hcisocket.h"

#include <QObject>
#include <QTimer>



class BleConnParamDevice : public QObject
{
	Q_OBJECT

public:
	BleConnParamDevice(const QSharedPointer<HciSocket> &hciSocket,
	                      quint16 handle, const BleAddress &address,
	                      const BleConnectionParameters &params,
	                      int postConnectionTimeout,
	                      int postUpdateTimeout,
	                      int retryTimeout,
	                      QObject *parent = nullptr);
	~BleConnParamDevice();

public slots:
	void onConnectionCompleted(const BleConnectionParameters &params);
	void onConnectionUpdated(const BleConnectionParameters &params);
	void onDisconnectionCompleted(HciSocket::HciStatus reason);

	void triggerUpdate(int msecs = 1000);

private slots:
	void onTimeout();

private:
	bool connectionParamsCloseEnough(const BleConnectionParameters &params) const;

private:
	const QSharedPointer<HciSocket> m_hciSocket;
	const quint16 m_handle;
	const BleAddress m_address;
	const BleConnectionParameters m_desiredParams;
	const int m_postConnectionTimeout;
	const int m_postUpdateTimeout;
	const int m_retryTimeout;

	bool m_connParamsOk;

	QTimer *m_timer;
};

#endif // !defined(BLECONNPARAMCDEVICE_H)
