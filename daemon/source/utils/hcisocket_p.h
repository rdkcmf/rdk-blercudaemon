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
//  hcisocket_p.h
//  SkyBluetoothRcu
//

#ifndef HCISOCKET_P_H
#define HCISOCKET_P_H

#include "hcisocket.h"

#include <QSocketNotifier>

struct evt_le_connection_complete;
struct evt_disconn_complete;
struct evt_le_connection_update_complete;


class HciSocketImpl : public HciSocket
{
public:
	explicit HciSocketImpl(int socketFd, uint hciDeviceId, QObject *parent = nullptr);
	~HciSocketImpl();

public:
	bool isValid() const override;

	bool requestConnectionUpdate(quint16 connHandle,
	                             const BleConnectionParameters &params) override;

	QList<ConnectedDeviceInfo> getConnectedDevices() const override;

	bool sendIncreaseDataCapability(quint16 connHandle) override;

private:
	bool setSocketFilter(int socketFd) const;

	bool bindSocket(int socketFd, uint hciDeviceId) const;

	bool sendCommand(quint16 ogf, quint16 ocf, void *data, quint8 dataLen);

	bool checkConnectionParams(quint16 minInterval, quint16 maxInterval,
	                           quint16 latency, quint16 supervisionTimeout) const;

	void onSocketActivated(int socket);
	void onConnectionCompleteEvent(const evt_le_connection_complete *event);
	void onDisconnectionCompleteEvent(const evt_disconn_complete *event);
	void onUpdateCompleteEvent(const evt_le_connection_update_complete *event);

	const char* hciErrorString(quint8 code) const;

private:
	const uint m_hciDeviceId;
	int m_hciSocket;
	QSocketNotifier *m_notifier;

};


#endif // !defined(HCISOCKET_P_H)
