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
//  asserviceadaptor.h
//  SkyBluetoothRcu
//

#ifndef ASSERVICEADAPTOR_H
#define ASSERVICEADAPTOR_H

#include "asrequest.h"

#include <QObject>
#include <QMap>
#include <QSet>
#include <QMultiMap>
#include <QString>
#include <QMetaObject>
#include <QVariant>

#include <QtDBus>

#include <functional>



class Player;
class ASService;

class ASServiceAdaptor final : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "com.sky.as.Service1")
    Q_CLASSINFO("D-Bus Introspection", ""
                "  <interface name=\"com.sky.as.Service1\">\n"
                "    <method name=\"Config\">\n"
                "      <arg direction=\"out\" type=\"s\" name=\"configJson\"/>\n"
                "    </method>\n"
                "    <method name=\"Request\">\n"
                "      <arg direction=\"in\" type=\"u\" name=\"requestFlags\"/>\n"
                "      <arg direction=\"in\" type=\"s\" name=\"requestUrl\"/>\n"
                "      <arg direction=\"in\" type=\"a{ss}\" name=\"requestHeaders\"/>\n"
                "      <arg direction=\"in\" type=\"a{ss}\" name=\"requestQueryParams\"/>\n"
                "      <arg direction=\"in\" type=\"s\" name=\"requestBody\"/>\n"
                "      <arg direction=\"out\" type=\"(ua{ss}s)\" name=\"response\"/>\n"
                "    </method>\n"
                "    <method name=\"RegisterWebSocketListener\">\n"
                "      <arg direction=\"in\" type=\"s\" name=\"wsUrl\"/>\n"
                "    </method>\n"
                "    <method name=\"UnregisterWebSocketListener\">\n"
                "      <arg direction=\"in\" type=\"s\" name=\"wsUrl\"/>\n"
                "    </method>\n"
                "    <signal name=\"WebSocketUpdate\">\n"
                "      <arg type=\"s\" name=\"url\"/>\n"
                "      <arg type=\"s\" name=\"message\"/>\n"
                "    </signal>\n"
                "    <method name=\"RegisterUpdatesListener\">\n"
                "      <arg direction=\"in\" type=\"s\" name=\"httpUrl\"/>\n"
                "    </method>\n"
                "    <method name=\"UnregisterUpdatesListener\">\n"
                "      <arg direction=\"in\" type=\"s\" name=\"httpUrl\"/>\n"
                "    </method>\n"
                "    <signal name=\"HttpUpdate\">\n"
                "      <arg type=\"s\" name=\"url\"/>\n"
                "      <arg type=\"x\" name=\"tag\"/>\n"
                "    </signal>\n"
                "    <method name=\"GetSystemInfo\">\n"
                "      <arg direction=\"out\" type=\"a{sv}\" name=\"info\"/>\n"
                "    </method>\n"
                "    <method name=\"GetSystemSetting\">\n"
                "      <arg direction=\"in\" type=\"s\" name=\"name\"/>\n"
                "      <arg direction=\"out\" type=\"s\" name=\"value\"/>\n"
                "    </method>\n"
                "    <method name=\"SetSystemSetting\">\n"
                "      <arg direction=\"in\" type=\"s\" name=\"name\"/>\n"
                "      <arg direction=\"in\" type=\"s\" name=\"value\"/>\n"
                "    </method>\n"
                "    <method name=\"GetTestPreference\">\n"
                "      <arg direction=\"in\" type=\"s\" name=\"name\"/>\n"
                "      <arg direction=\"out\" type=\"s\" name=\"value\"/>\n"
                "    </method>\n"
                "    <method name=\"SetTestPreference\">\n"
                "      <arg direction=\"in\" type=\"s\" name=\"name\"/>\n"
                "      <arg direction=\"in\" type=\"s\" name=\"value\"/>\n"
                "      <arg direction=\"in\" type=\"i\" name=\"pin\"/>\n"
                "    </method>\n"
                "  </interface>\n"
                "")

public:
    ASServiceAdaptor(const QDBusConnection &dbusConn,
                     const QString &dbusObjectPath,
                     const QString &serviceConfig,
                     ASService *parent);
    ~ASServiceAdaptor() final;

public:
    struct Response
    {
        uint code;
        QMap<QString,QString> headers;
        QString body;

        Response()
            : code(0)
        { }

        Response(uint code_, const char *body_)
            : code(code_), body(body_)
        { }

        Response(uint code_, const QString &body_)
            : code(code_), body(body_)
        { }

        Response(uint code_, const QString &body_, const QMap<QString,QString> &headers_)
            : code(code_), headers(headers_), body(body_)
        { }

        Response(uint code_, QString &&body_)
            : code(code_), body(std::move(body_))
        { }

        Response(uint code_, QString &&body_, QMap<QString,QString> &&headers_)
            : code(code_), headers(std::move(headers_)), body(std::move(body_))
        { }
    };

public slots:
    QString Config();

    Response Request(uint requestFlags,
                     const QString &requestUrl,
                     const QMap<QString,QString> &requestHeaders,
                     const QMap<QString,QString> &requestQueryParams,
                     const QString &requestBody,
                     const QDBusMessage &message);

    QVariantMap GetSystemInfo();

    QString GetSystemSetting(const QString &name);
    void SetSystemSetting(const QString &name, const QString &value);

    QString GetTestPreference(const QString &name);
    void SetTestPreference(const QString &name, const QString &value, int pin);

    void RegisterUpdatesListener(const QString &httpUrl, const QDBusMessage &message);
    void UnregisterUpdatesListener(const QString &httpUrl, const QDBusMessage &message);

    void RegisterWebSocketListener(const QString &wsUrl, const QDBusMessage &message);
    void UnregisterWebSocketListener(const QString &wsUrl, const QDBusMessage &message);

signals:
    // void HttpUpdate(const QString &url, int64_t tag);
    // void WebSocketUpdate(const QString &url, const QString &message);


public:
    void updateWebSocket(const QString &wsUrl, const QJsonObject &wsMessage);
    void updateHttpUrl(const QString &httpUrl, qint64 tag);


private:
    void onServiceUnregistered(const QString &serviceName);

private:
    const QString m_objectPath;
    const QString m_serviceConfig;
    ASService * const m_parent;

    QDBusServiceWatcher m_serviceWatcher;

private:
    void sendWsUpdateTo(const QString &service, const QString &wsUrl,
                        const QByteArray &message);
    void sendCachedWsUpdateTo(const QString &service, const QString &wsUrl);

    void sendHttpUpdateTo(const QString &service, const QString &httpUrl,
                          int64_t tag);
    void sendCachedHttpUpdateTo(const QString &service, const QString &httpUrl);


    QMap<QString, QByteArray> m_wsCacheMessages;
    QMap<QString, int64_t> m_httpCachedTag;

    QMultiMap<QString, QString> m_registeredWsClients;
    QMultiMap<QString, QString> m_registeredUpdatesClients;
};


Q_DECLARE_METATYPE(ASServiceAdaptor::Response)


QDBusArgument &operator<<(QDBusArgument &argument, const ASServiceAdaptor::Response &value);
const QDBusArgument &operator>>(const QDBusArgument &argument, ASServiceAdaptor::Response &value);


#endif // ASSERVICEADAPTOR_H
