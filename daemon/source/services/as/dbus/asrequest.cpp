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
//  asrequest.cpp
//  SkyBluetoothRcu
//

#include "asrequest.h"
#include "asrequest_p.h"
#include "asserviceadaptor.h"




ASRequest::ASRequest(uint requestFlags,
                     const QString &requestUrl,
                     const QMap<QString,QString> &requestHeaders,
                     const QMap<QString,QString> &requestQueryParams,
                     const QString &requestBody,
                     const QDBusMessage &message)
    : m_private(QSharedPointer<ASRequestPrivate>::create(requestFlags,
                                                         requestUrl,
                                                         requestHeaders,
                                                         requestQueryParams,
                                                         requestBody,
                                                         message))
{
}

ASRequest::ASRequest(const ASRequest &other)
    : m_private(other.m_private)
{
}

ASRequest::Method ASRequest::method() const
{
    return m_private->m_method;
}

QString ASRequest::path() const
{
    return m_private->m_path;
}

QString ASRequest::body() const
{
    return m_private->m_body;
}

ASRequest::HeaderMap ASRequest::headers() const
{
    return m_private->m_headers;
}

ASRequest::QueryStringMap ASRequest::queryParams() const
{
    return m_private->m_queryParams;
}

bool ASRequest::sendReply(uint code) const
{
    return m_private->sendReply(code, HeaderMap(), QString());
}

bool ASRequest::sendReply(uint code, const QString &body) const
{
    return m_private->sendReply(code, HeaderMap(), body);
}

bool ASRequest::sendReply(uint code, const HeaderMap &headers, const QString &body) const
{
    return m_private->sendReply(code, headers, body);
}

bool ASRequest::sendErrorReply(int httpCode, int errorCode,
                               const QString &userMessage,
                               const QString &developerMessage) const
{
	// form into json
	QString body;
	if (developerMessage.isEmpty()) {
		body = QString::asprintf(R"JSON({ "errorCode": "%d", "userMessage": "%s" })JSON",
		                         errorCode, qPrintable(userMessage));
	} else {
		body = QString::asprintf(R"JSON({ "errorCode": "%d", "userMessage": "%s", "developerMessage": "%s" })JSON",
		                         errorCode, qPrintable(userMessage), qPrintable(developerMessage));
	}

	return m_private->sendReply(httpCode, HeaderMap(), body);
}

bool ASRequest::sendErrorReply(ErrorType type, const QString &developerMessage) const
{
	struct CannedError
	{
		int httpCode;
		int errorCode;
		QString userMessage;
	};

	static const QMap<ErrorType, CannedError> cannedErrors =
	{
		{   InvalidUrlError,        {  404, 101, QStringLiteral("Invalid URL")                  }   },
		{   InvalidParametersError, {  400, 102, QStringLiteral("Invalid Parameters")           }   },
		{   GenericFailureError,    {  500, 103, QStringLiteral("Generic failure")              }   },
		{   NotSupportedError,      {  404, 104, QStringLiteral("Not supported on this device") }   },
	};

	const CannedError &error = cannedErrors.value(type);
	return sendErrorReply(error.httpCode, error.errorCode, error.userMessage, developerMessage );
}


// -----------------------------------------------------------------------------
/*!
    \internal

    Helper to convert header map types.

 */
static ASRequest::HeaderMap convertHeaderMap(const QMap<QString,QString> &requestHeaders)
{
    ASRequest::HeaderMap headers;

    auto it = requestHeaders.begin();
    for (; it != requestHeaders.end(); ++it)
    {
        headers.insert(qUtf8Printable(it.key()), it.value().toUtf8());
    }

    return headers;
}

// -----------------------------------------------------------------------------
/*!
    \internal

    Helper to determine the request method from the flags.

 */
static ASRequest::Method methodFromFlags(uint requestFlags)
{
    switch (requestFlags & 0xf)
    {
        case 0x1:
            return ASRequest::HttpGet;
        case 0x2:
            return ASRequest::HttpPost;
        default:
            return ASRequest::InvalidMethod;
    }
}

// -----------------------------------------------------------------------------
/*!
    \internal



 */
ASRequestPrivate::ASRequestPrivate(uint requestFlags,
                                   const QString &requestUrl,
                                   const QMap<QString,QString> &requestHeaders,
                                   const QMap<QString,QString> &requestQueryParams,
                                   const QString &requestBody,
                                   const QDBusMessage &message)
    : m_sentReply(false)
    , m_method(methodFromFlags(requestFlags))
    , m_path(requestUrl)
    , m_body(requestBody)
    , m_headers(convertHeaderMap(requestHeaders))
	, m_queryParams(requestQueryParams)
    , m_reply(message.createReply())
{
}

// -----------------------------------------------------------------------------
/*!
    \internal

    Destructor, checks if a response has been sent and if not logs a warning
    and sends a default error response.

 */
ASRequestPrivate::~ASRequestPrivate()
{
    if (!m_sentReply)
    {
        qWarning("as request object destroyed without sending a reply,"
                 " sending default reply");

        // create a generic http error reply
        const QString body = QString::asprintf(
                 R"JSON( { "errorCode": "105", "userMessage": "%s", "developerMessage": "%s" } )JSON",
                 "Service failure",
                 "Service failed to send response to request");

        // send off the reply
        sendReply(500, ASRequest::HeaderMap(), body);
    }
}

// -----------------------------------------------------------------------------
/*!
    \internal

    Sends a reply to the request back to the remote service.

 */
bool ASRequestPrivate::sendReply(uint code,
                                 const ASRequest::HeaderMap &headers,
                                 const QString &body)
{
    // FIXME: yet another copy convert of maps
    QMap<QString,QString> headerMap;

    auto it = headers.begin();
    for (; it != headers.end(); ++it)
    {
        headerMap.insert(it.value(), it.key());
    }

    // marshall the response args into the reply
    ASServiceAdaptor::Response response(code, body, headerMap);

    QDBusArgument args;
    args << response;

    m_reply << QVariant::fromValue(args);

    // send off the reply
    m_sentReply = QDBusConnection::systemBus().send(m_reply);

    return true;
}
