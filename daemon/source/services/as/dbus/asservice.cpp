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
//  asservice.cpp
//  SkyBluetoothRcu
//

#include "asservice.h"
#include "asserviceadaptor.h"
#include "asrequest.h"


ASService::ASService(const QDBusConnection &dbusConn,
                     const QString &serviceName,
                     const QString &configJson,
                     QObject *parent)
	: QObject(parent)
{
	// the object path of the service is always fixed
	const QString objectPath("/com/sky/as/service");

	// create a new adpator attached to this class
	m_adaptor = new ASServiceAdaptor(dbusConn, objectPath, configJson, this);

	// register the dbus object
	QDBusConnection dbusConn_(dbusConn);
	if (!dbusConn_.registerObject(objectPath, this, QDBusConnection::ExportAdaptors)) {
		qWarning("failed to register service on name on dbus");
	}


	// register ourselves as an AS service (it's possible to register yourself with
	// multiple service names)
	if (!dbusConn_.registerService(serviceName))
		qWarning() << "failed to register service due to" << dbusConn.lastError();
	else
		qInfo("register as service on dbus");


}

ASService::~ASService()
{
}

// -----------------------------------------------------------------------------
/*!
    Expected to be overridden, it's called when a request from the remote
    client is received.

 */
void ASService::onRequest(const ASRequest &request)
{
	Q_UNUSED(request)

}

// -----------------------------------------------------------------------------
/*!


 */
QVariantMap ASService::systemInfo()
{

}

// -----------------------------------------------------------------------------
/*!


 */
QString ASService::getSystemSetting(const QString &name)
{
	Q_UNUSED(name)

	return QString();
}

// -----------------------------------------------------------------------------
/*!


 */
void ASService::setSystemSetting(const QString &name, const QString &value)
{
	Q_UNUSED(name)
	Q_UNUSED(value)
}

// -----------------------------------------------------------------------------
/*!


 */
QString ASService::getTestPreference(const QString &name)
{
	Q_UNUSED(name)

	return QString();
}

// -----------------------------------------------------------------------------
/*!


 */
void ASService::setTestPreference(const QString &name, const QString &value, int pin)
{
	Q_UNUSED(name)
	Q_UNUSED(value)
	Q_UNUSED(pin)

}

// -----------------------------------------------------------------------------
/*!


 */
void ASService::updateWebSocket(const QString &wsUrl, const QJsonObject &wsMessage)
{
	m_adaptor->updateWebSocket(wsUrl, wsMessage);
}

// -----------------------------------------------------------------------------
/*!


 */
void ASService::updateHttpUrl(const QString &httpUrl, qint64 tag)
{
	m_adaptor->updateHttpUrl(httpUrl, tag);
}
