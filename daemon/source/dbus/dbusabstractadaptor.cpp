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
//  dbusabstractadaptor.cpp
//  BleRcuDaemon
//

#include "dbusabstractadaptor.h"
#include "utils/logging.h"



DBusAbstractAdaptor::DBusAbstractAdaptor(QObject *parent)
	: QDBusAbstractAdaptor(parent)
	, m_parentContext(nullptr)
{

	// sanity check that the parent object is publicly inherited from
	// QDBusContext, we need this so that we can get the QDBusConnection object
	// that sent the request
	if (parent) {
		void *ptr = parent->qt_metacast("QDBusContext");
		m_parentContext = reinterpret_cast<QDBusContext*>(ptr);
	}

	if (!m_parentContext)
		qError("failed to get dbus context object of the parent");


	// QMetaObject::invokeMethod(this, "setupNotifySignals", Qt::QueuedConnection);
}

DBusAbstractAdaptor::~DBusAbstractAdaptor()
{
}


// -----------------------------------------------------------------------------
/*!
	Adds the dbus \a connection to the internal list of dbus connections to send
 	property change notifications.

 This method must be called from the dbus method slot.

 */
void DBusAbstractAdaptor::registerConnection(const QDBusConnection &connection)
{
	m_dbusConnections.append(connection);
}

void DBusAbstractAdaptor::unregisterConnection(const QDBusConnection &connection)
{
	Q_UNUSED(connection);

}


// -----------------------------------------------------------------------------
/*!
	Sends out a dbus error message with \a name and \a message strings.  The
	\a message string may be empty.

	This method must be called from the dbus method slot.

 */
void DBusAbstractAdaptor::sendErrorReply(const QDBusMessage &request,
                                         const QString &errorName,
                                         const QString &errorMessage) const
{
	// sanity check we have the parents context, needed so we can get the
	// dbus connection that the request was sent on
	if (Q_UNLIKELY(m_parentContext == nullptr)) {
		qWarning("missing parent dbus context");
		return;
	}

	// boilerplate to notify the dbus system that we will send the reply
	request.setDelayedReply(true);

	// create the error message and send it
	QDBusMessage error = request.createErrorReply(errorName, errorMessage);
	if (!m_parentContext->connection().send(error))
		qWarning() << "failed to send error reply" << errorName << errorMessage;
}

// -----------------------------------------------------------------------------
/*!
	Sends out a dbus method reply message with the supplied \a result.

	This method must be called from the dbus method slot.

 */
void DBusAbstractAdaptor::sendReply(const QDBusMessage &request,
                                    const QVariant &result) const
{
	// sanity check we have the parents context, needed so we can get the
	// dbus connection that the request was sent on
	if (Q_UNLIKELY(m_parentContext == nullptr)) {
		qWarning("missing parent dbus context");
		return;
	}

	// boilerplate to notify the dbus system that we will send the reply
	request.setDelayedReply(true);

	QDBusMessage reply = request.createReply();
	if (result.isValid())
		reply << result;

	if (!m_parentContext->connection().send(reply))
		qWarning("failed to send reply");
}

