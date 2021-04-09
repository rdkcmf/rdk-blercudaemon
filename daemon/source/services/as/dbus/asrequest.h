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
//  asrequest.h
//  SkyBluetoothRcu
//

#ifndef ASREQUEST_H
#define ASREQUEST_H

#include <QString>
#include <QMap>
#include <QDBusMessage>
#include <QSharedPointer>

#include "caselessbytearray.h"


class ASRequestPrivate;

class ASRequest
{
public:
    ASRequest(uint requestFlags,
              const QString &requestUrl,
              const QMap<QString,QString> &requestHeaders,
              const QMap<QString,QString> &requestQueryParams,
              const QString &requestBody,
              const QDBusMessage &message);
    ASRequest(const ASRequest &other);
    ~ASRequest() = default;

public:

    enum Method { InvalidMethod, HttpPost, HttpGet };

    Method method() const;

public:

    typedef QMultiMap<QString, QString> QueryStringMap;
    typedef QMultiMap<CaselessByteArray, QByteArray> HeaderMap;

    QString path() const;
    QString body() const;
    HeaderMap headers() const;
    QueryStringMap queryParams() const;

public:
    bool sendReply(uint code) const;
    bool sendReply(uint code, const QString &body) const;
    bool sendReply(uint code, const HeaderMap &headers, const QString &body = QString()) const;

    enum ErrorType
    {
        InvalidUrlError,
        InvalidParametersError,
        GenericFailureError,
        NotSupportedError
    };

    bool sendErrorReply(ErrorType type, const QString &developerMessage = QString()) const;
    bool sendErrorReply(int httpCode, int errorCode, const QString &userMessage,
                        const QString &developerMessage = QString()) const;

private:
    QSharedPointer<ASRequestPrivate> m_private;

};


#endif // ASREQUEST_H
