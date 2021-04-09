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
//  blercuasservice.h
//  SkyBluetoothRcu
//

#ifndef BLERCUASSERVICE_H
#define BLERCUASSERVICE_H

#include <QtGlobal>

#if defined(Q_OS_ANDROID)
#  include "binder/asservice.h"
#elif defined(Q_OS_LINUX)
#  include "dbus/asservice.h"
#endif

#include <QDBusConnection>
#include <QSharedPointer>
#include <QJsonObject>


class IrDatabase;
class BleRcuController;
class BleRcuStatusWebSocket;
class BleRcuFwUpgradeService;


class BleRcuASService : public ASService
{
	Q_OBJECT

public:
	explicit BleRcuASService(const QDBusConnection &dbusConn,
	                         QObject *parent = nullptr);
	~BleRcuASService() final;

	void setController(const QSharedPointer<BleRcuController> &controller);
	void setIrDatabase(const QSharedPointer<IrDatabase> &irDatabase);

protected:
	void onRequest(const ASRequest &request) override;

	QString getSystemSetting(const QString &name) override;
	void setSystemSetting(const QString &name, const QString &value) override;

private:
	void onWebSocketUpdate(const QJsonObject &message);
	void onFwUpgradeStatusChanged(const QJsonObject &message);

	void handleGetRequest(const ASRequest &request);
	void handlePostRequest(const ASRequest &request);

private:
	#define EXECUTOR(fn) \
		void fn (const ASRequest &request)

	EXECUTOR(onRequestStartSearching);
	EXECUTOR(onRequestEDIDInfo);
	EXECUTOR(onRequestEDIDBasedCodes);
	EXECUTOR(onRequestIRCodesManual);
	EXECUTOR(onRequestIRCodesManuf);
	EXECUTOR(onRequestIRCodesModels);
	EXECUTOR(onRequestIRCodesSetCode);
	EXECUTOR(onRequestIRCodesClearCode);

private:
	const QDBusConnection m_dbusConn;

	QSharedPointer<BleRcuController> m_controller;
	QSharedPointer<IrDatabase> m_irDatabase;
	QSharedPointer<BleRcuStatusWebSocket> m_wsStatus;
	QSharedPointer<BleRcuFwUpgradeService> m_fwUpgrade;

};

#endif // BLERCUASSERVICE_H
