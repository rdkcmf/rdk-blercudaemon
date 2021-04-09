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
//  asservice.h
//  SkyBluetoothRcu
//

#ifndef ASSERVICE_H
#define ASSERVICE_H

#include <QObject>
#include <QSharedPointer>
#include <QDBusConnection>
#include <QDBusContext>


class ASRequest;
class ASServiceAdaptor;


class ASService : public QObject
{
	Q_OBJECT

public:
	ASService(const QDBusConnection &dbusConn,
	          const QString &serviceName,
	          const QString &configJson,
	          QObject *parent = nullptr);
	virtual ~ASService();

protected:
	virtual void onRequest(const ASRequest &request);

	virtual QVariantMap systemInfo();

	virtual QString getSystemSetting(const QString &name);
	virtual void setSystemSetting(const QString &name, const QString &value);

	virtual QString getTestPreference(const QString &name);
	virtual void setTestPreference(const QString &name, const QString &value, int pin);

public:
	void updateWebSocket(const QString &wsUrl, const QJsonObject &wsMessage);
	void updateHttpUrl(const QString &httpUrl, qint64 tag);

private:
	friend class ASServiceAdaptor;
	ASServiceAdaptor *m_adaptor;
};



#endif // ASSERVICE_H
