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
//  asserviceadaptor.cpp
//  SkyBluetoothRcu
//

#include "asserviceadaptor.h"
#include "asservice.h"

#include "asrequest.h"

#include <inttypes.h>


#if 1
// Marshall the MyStructure data into a D-Bus argument
QDBusArgument &operator<<(QDBusArgument &argument,
                          const ASServiceAdaptor::Response &value)
{
	argument.beginStructure();
	argument << value.code;
	argument << value.headers;
	argument << value.body;
	argument.endStructure();

	return argument;
}

// Retrieve the MyStructure data from the D-Bus argument
const QDBusArgument &operator>>(const QDBusArgument &argument,
                                ASServiceAdaptor::Response &value)
{
	qWarning("de-marshing not implemented");
	return argument;
}
#endif


// -----------------------------------------------------------------------------
/*!


 */
ASServiceAdaptor::ASServiceAdaptor(const QDBusConnection &dbusConn,
                                   const QString &dbusObjectPath,
                                   const QString &serviceConfig,
                                   ASService *parent)
	: QDBusAbstractAdaptor(parent)
	, m_objectPath(dbusObjectPath)
	, m_serviceConfig(serviceConfig)
	, m_parent(parent)
{

	// register a string map with qt dbus so it recognises it
	qDBusRegisterMetaType<QMap<QString,QString>>();
	qRegisterMetaType<ASServiceAdaptor::Response>();
	qDBusRegisterMetaType<ASServiceAdaptor::Response>();


	// initialise the dbus service watcher, we use this to check if one of our
	// registered listener clients has dropped of the bus
	m_serviceWatcher.setWatchMode(QDBusServiceWatcher::WatchForUnregistration);
	QObject::connect(&m_serviceWatcher, &QDBusServiceWatcher::serviceUnregistered,
	                 this, &ASServiceAdaptor::onServiceUnregistered);


}

// -----------------------------------------------------------------------------
/*!


 */
ASServiceAdaptor::~ASServiceAdaptor()
{
    // destructor
}

// -----------------------------------------------------------------------------
/*!
    \internal

    Called when a service has disconnected from the bus, we use this to
    determine if one of our listeners has disconnected.

 */
void ASServiceAdaptor::onServiceUnregistered(const QString &serviceName)
{
    qInfo("service '%s' has been removed from the bus", qPrintable(serviceName));

    // check registered ws clients
    {
        auto it = m_registeredWsClients.begin();
        while (it != m_registeredWsClients.end())
        {
            const QString &name = it.value();
            if (serviceName == name)
                it = m_registeredWsClients.erase(it);
            else
                ++it;
        }
    }

    // check registered update clients
    {
        auto it = m_registeredUpdatesClients.begin();
        while (it != m_registeredUpdatesClients.end())
        {
            const QString &name = it.value();
            if (serviceName == name)
                it = m_registeredUpdatesClients.erase(it);
            else
                ++it;
        }
    }
}

// -----------------------------------------------------------------------------
/*!
    \internal

    Returns the AS config for the service.

 */
QString ASServiceAdaptor::Config()
{
    return m_serviceConfig;
}

// -----------------------------------------------------------------------------
/*!
    \internal

    Called by remote clients to perform an AS request.

 */
ASServiceAdaptor::Response ASServiceAdaptor::Request(uint requestFlags,
                                                     const QString &requestUrl,
                                                     const QMap<QString,QString> &requestHeaders,
                                                     const QMap<QString,QString> &requestQueryParams,
                                                     const QString &requestBody,
                                                     const QDBusMessage &message)
{
    qInfo("handle method call com.sky.as.Service1.Request");

    // indicate that the reply will be delayed
    message.setDelayedReply(true);

    // wrap the reply in an response object
    m_parent->onRequest(ASRequest(requestFlags, requestUrl, requestHeaders,
                                  requestQueryParams, requestBody, message));

    // return value ignored because we've told qt it's a delayed response
    return Response();
}

// -----------------------------------------------------------------------------
/*!
    \internal


 */
QVariantMap ASServiceAdaptor::GetSystemInfo()
{
    return m_parent->systemInfo();
}

// -----------------------------------------------------------------------------
/*!
    \internal


 */
QString ASServiceAdaptor::GetSystemSetting(const QString &name)
{
    return m_parent->getSystemSetting(name);
}

// -----------------------------------------------------------------------------
/*!
    \internal


 */
void ASServiceAdaptor::SetSystemSetting(const QString &name, const QString &value)
{
    m_parent->setSystemSetting(name, value);
}

// -----------------------------------------------------------------------------
/*!
    \internal


 */
QString ASServiceAdaptor::GetTestPreference(const QString &name)
{
    return m_parent->getTestPreference(name);
}

// -----------------------------------------------------------------------------
/*!
    \internal


 */
void ASServiceAdaptor::SetTestPreference(const QString &name, const QString &value, int pin)
{
    m_parent->setTestPreference(name, value, pin);
}

// -----------------------------------------------------------------------------
/*!
    \internal

    Called by remote clients to register for updates to a given websocket.

 */
void ASServiceAdaptor::RegisterWebSocketListener(const QString &wsUrl,
                                                 const QDBusMessage &message)
{
    // get the caller and add to the set of registered clients
    const QString caller = message.service();
    m_registeredWsClients.insert(wsUrl, caller);

    // next queue up an immediate ws update for the given client
    QTimer::singleShot(0, this, std::bind(&ASServiceAdaptor::sendCachedWsUpdateTo,
                                          this, caller, wsUrl));
}

// -----------------------------------------------------------------------------
/*!
    \internal

    Called by remote clients to unregister for updates to a given websocket.

 */
void ASServiceAdaptor::UnregisterWebSocketListener(const QString &wsUrl,
                                                   const QDBusMessage &message)
{
    // get the caller and add to the set of registered clients
    const QString caller = message.service();
    if (m_registeredWsClients.remove(wsUrl, caller) < 0)
    {
        qWarning("failed to find registered listener '%s' for url '%s'",
                 qPrintable(caller), qPrintable(wsUrl));
    }
}

// -----------------------------------------------------------------------------
/*!
    \internal


 */
void ASServiceAdaptor::RegisterUpdatesListener(const QString &httpUrl,
                                               const QDBusMessage &message)
{
    // get the caller and add to the set of registered clients
    const QString caller = message.service();
    m_registeredUpdatesClients.insert(httpUrl, caller);

    // next queue up an immediate ws update for the given client
    QTimer::singleShot(0, this, std::bind(&ASServiceAdaptor::sendCachedHttpUpdateTo,
                                          this, caller, httpUrl));
}

// -----------------------------------------------------------------------------
/*!
    \internal


 */
void ASServiceAdaptor::UnregisterUpdatesListener(const QString &httpUrl,
                                                 const QDBusMessage &message)
{
    // get the caller and add to the set of registered clients
    const QString caller = message.service();
    if (m_registeredUpdatesClients.remove(httpUrl, caller) < 0)
    {
        qWarning("failed to find registered listener '%s' for url '%s'",
                 qPrintable(caller), qPrintable(httpUrl));
    }
}

// -----------------------------------------------------------------------------
/*!
    Updates the message in the websocket with the given url.  If any remote
    listeners a registered with the given url then a the new message is sent
    to them.

    The value is cached, such that when a new client registers for notifications
    it will immediately be sent the last message.

 */
void ASServiceAdaptor::updateWebSocket(const QString &wsUrl,
                                       const QJsonObject &wsMessage)
{
    // convert the json object to a string
    const QJsonDocument jsonDoc(wsMessage);
    const QByteArray message = jsonDoc.toJson();

    qDebug("caching message '%s' for ws url '%s'",
           qPrintable(wsUrl), message.constData());

    // update the cached value
    m_wsCacheMessages.insert(wsUrl, message);

    // then send signals to any listeners
    auto range = m_registeredWsClients.equal_range(wsUrl);
    for (auto it = range.first; it != range.second; ++it)
    {
        sendWsUpdateTo(it.value(), wsUrl, message);
    }
}

// -----------------------------------------------------------------------------
/*!
    \internal


 */
void ASServiceAdaptor::sendWsUpdateTo(const QString &service,
                                      const QString &wsUrl,
                                      const QByteArray &message)
{
    qDebug("sending message '%s' for ws url '%s' to '%s'",
           message.constData(), qPrintable(wsUrl), qPrintable(service));

    // create the signal
    QDBusMessage signal =
        QDBusMessage::createTargetedSignal(service,
                                           m_objectPath,
                                           QStringLiteral("com.sky.as.Service1"),
                                           QStringLiteral("WebSocketUpdate"));

    // add the arguments
    signal << wsUrl << QString(message);

    // and finally send the signal
    QDBusConnection::systemBus().send(signal);

}

// -----------------------------------------------------------------------------
/*!
    \internal

    Called when a new client has registered a listener and we therefore need to
    send it the cached value (if one).


 */
void ASServiceAdaptor::sendCachedWsUpdateTo(const QString &service,
                                            const QString &wsUrl)
{
    // check if we have a cached value
    auto it = m_wsCacheMessages.find(wsUrl);
    if (it == m_wsCacheMessages.end())
    {
        qWarning("no cached ws message for url '%s'", qPrintable(wsUrl));
        return;
    }

    // send off the cached value
    sendWsUpdateTo(service, wsUrl, it.value());
}

// -----------------------------------------------------------------------------
/*!
    Public API called by the service code to update the 64-bit tag value
    associated with an http url in the /as/updates websocket.

 */
void ASServiceAdaptor::updateHttpUrl(const QString &httpUrl, qint64 tag)
{
    // update the cached value
    m_httpCachedTag.insert(httpUrl, tag);

    qDebug("caching tag %" PRId64 " for http url '%s'",
           tag, qPrintable(httpUrl));

    // then send signals to any listeners
    auto range = m_registeredUpdatesClients.equal_range(httpUrl);
    for (auto it = range.first; it != range.second; ++it)
    {
        sendHttpUpdateTo(it.value(), httpUrl, tag);
    }
}

// -----------------------------------------------------------------------------
/*!
    \internal

 */
void ASServiceAdaptor::sendHttpUpdateTo(const QString &service,
                                        const QString &httpUrl,
                                        int64_t tag)
{
    qDebug("sending tag %" PRId64 " for http url '%s' to '%s'",
           tag, qPrintable(httpUrl), qPrintable(service));

    // create the signal
    QDBusMessage signal =
        QDBusMessage::createTargetedSignal(service,
                                           m_objectPath,
                                           QStringLiteral("com.sky.as.Service1"),
                                           QStringLiteral("HttpUpdate"));

    // add the arguments
    signal << httpUrl << tag;

    // and finally send the signal
    QDBusConnection::systemBus().send(signal);
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Called when a new client has registered a listener for updates to an http
	domain and we therefore need to send it the cached value (if one).


 */
void ASServiceAdaptor::sendCachedHttpUpdateTo(const QString &service,
                                              const QString &httpUrl)
{
	// check if we have a cached value
	auto it = m_httpCachedTag.find(httpUrl);
	if (it == m_httpCachedTag.end()) {
		qWarning("no cached tag for url '%s'", qPrintable(httpUrl));
		return;
	}

	// send off the cached value
	sendHttpUpdateTo(service, httpUrl, it.value());
}
