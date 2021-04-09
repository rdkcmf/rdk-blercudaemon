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
//  blercufwupgradeservice.h
//  SkyBluetoothRcu
//

#ifndef BLERCUFWUPGRADESERVICE_H
#define BLERCUFWUPGRADESERVICE_H

#include "utils/bleaddress.h"
#include "utils/future.h"

#include <QObject>
#include <QMap>
#include <QUuid>
#include <QSharedPointer>
#include <QJsonObject>
#include <QJsonArray>
#include <QElapsedTimer>


class ASRequest;
class BleRcuController;
class FwImageFile;

class BleRcuFwUpgradeMonitor;


class BleRcuFwUpgradeService : public QObject
{
	Q_OBJECT

public:
	explicit BleRcuFwUpgradeService(const QSharedPointer<BleRcuController> &controller,
	                                QObject *parent = nullptr);
	~BleRcuFwUpgradeService() final;

public:
	void handleRequest(const ASRequest &request);

	QJsonObject status() const;

signals:
	void statusChanged(const QJsonObject &status);

private:
	void onDeviceRemoved(const BleAddress &address);

private:
	void onUploadFileStart(const ASRequest &request);
	void onUploadFileData(const QUuid &uuid, const ASRequest &request);
	void onUploadFileDelete(const QUuid &uuid, const ASRequest &request);

	void onStartFwUpgrade(const ASRequest &request);
	void onAbortFwUpgrade(const ASRequest &request);

	QSharedPointer<FwImageFile> copyFwMemoryFile(int memFd) const;

	void connectFutureToServiceReply(const ASRequest &request,
	                                 const Future<void> &result) const;

	void updateWebSocket();

private:
	const QSharedPointer<BleRcuController> m_controller;

	QJsonArray m_remotes;

	struct FwMemoryFile
	{
		int memFd = -1;
		size_t size = 0;
		QElapsedTimer created;
	};

	QMap<QUuid, FwMemoryFile> m_uploadedFiles;

	QMap<BleAddress, QSharedPointer<BleRcuFwUpgradeMonitor>> m_deviceMonitors;
};

#endif // BLERCUFWUPGRADESERVICE_H
