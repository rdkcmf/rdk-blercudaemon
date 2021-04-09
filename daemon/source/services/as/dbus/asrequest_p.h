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
//  asrequest_p.h
//  SkyBluetoothRcu
//

#ifndef ASREQUEST_P_H
#define ASREQUEST_P_H

#include "asrequest.h"

class ASRequestPrivate
{

public:
    ASRequestPrivate(uint requestFlags,
                     const QString &requestUrl,
                     const QMap<QString,QString> &requestHeaders,
                     const QMap<QString,QString> &requestQueryParams,
                     const QString &requestBody,
                     const QDBusMessage &message);
    ~ASRequestPrivate();

public:
    bool sendReply(uint code, const ASRequest::HeaderMap &headers, const QString &body);

private:
    friend class ASRequest;

    bool m_sentReply;

    const ASRequest::Method m_method;

    const QString m_path;
    const QString m_body;
    const ASRequest::HeaderMap m_headers;
    const ASRequest::QueryStringMap m_queryParams;

    QDBusMessage m_reply;
};

#endif // ASREQUEST_P_H
