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
//  dbusabstractadaptor.h
//  BleRcuDaemon
//

#ifndef DBUSABSTRACTADAPTOR_H
#define DBUSABSTRACTADAPTOR_H

#include "utils/future.h"

#include <QObject>
#include <QString>
#include <QDebug>
#include <QList>
#include <QMetaClassInfo>

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusContext>
#include <QDBusAbstractAdaptor>

#include <functional>


class DBusAbstractAdaptor : public QDBusAbstractAdaptor
{
	Q_OBJECT

protected:
	explicit DBusAbstractAdaptor(QObject *parent);

public:
	virtual ~DBusAbstractAdaptor();

	void registerConnection(const QDBusConnection &dbusConn);
	void unregisterConnection(const QDBusConnection &dbusConn);

protected:

	void sendErrorReply(const QDBusMessage &request,
	                    const QString &errorName,
	                    const QString &errorMessage) const;

	void sendReply(const QDBusMessage &request,
	               const QVariant &result) const;


protected:

	// -------------------------------------------------------------------------
	/*!
		Connects an \l{Future} object to a dbus request message
		such that when the \a result emits either a \l{Future::error()}
		or \l{Future::finished()} signal then a dbus reply message is sent
		across the bus.

		The result type of the \l{Future} will be converted to a dbus value
		and sent provided the type is not \c void (i.e. \c Future<void>)

	 */
	template <typename R>
	void connectFutureToDBusReply(const QDBusMessage &request,
	                              const Future<R> &result) const
	{
		// sanity check we have the parents context, needed so we can get the
		// dbus connection that the request was sent on
		if (Q_UNLIKELY(m_parentContext == nullptr)) {
			qWarning("missing parent dbus context");
			return;
		}

		const QDBusConnection connection(m_parentContext->connection());

		// create a lambda to create an error message and send it
		const std::function<void(const QString&, const QString&)> errorLambda =
			[connection, request] (const QString &errName, const QString &errMessage) {

				QDBusMessage error = request.createErrorReply(errName, errMessage);
				if (!connection.send(error))
					qWarning() << "failed to send error reply to request" << request;
			};

		// create a lambda to convert the result into a dbus message and send it
		const  std::function<void(const R&)> finishedLambda =
			[connection, request] (const R &r) {

				QDBusMessage reply = request.createReply(QVariant::fromValue<R>(r));
				if (!connection.send(reply))
					qWarning() << "failed to send reply to request" << request;
			};


		// boilerplate to notify the dbus system that we will send the reply
		request.setDelayedReply(true);

		// check if the result is already available and if so just send a
		// reply, don't bother with attaching to the result signals
		if (result.isFinished()) {

			if (result.isError())
				errorLambda(result.errorName(), result.errorMessage());
			else
				finishedLambda(result.result());

		} else {

			// connect the lambda's to the finished and error signals
			result.connectFinished(this, finishedLambda);
			result.connectErrored(this, errorLambda);
		}
	}

	// -------------------------------------------------------------------------
	/*!
		This is a specialisation for an \l{Future} object with void template
		argument, i.e. the \a result doesn't contain a value.

	 */
	void connectFutureToDBusReply(const QDBusMessage &request,
	                              const Future<void> &result) const
	{
		// sanity check we have the parents context, needed so we can get the
		// dbus connection that the request was sent on
		if (Q_UNLIKELY(m_parentContext == nullptr)) {
			qWarning("missing parent dbus context");
			return;
		}

		const QDBusConnection connection(m_parentContext->connection());

		// create a lambda to create an error message and send it
		const std::function<void(const QString&, const QString&)> errorLambda =
			[connection, request] (const QString &errName, const QString &errMessage) {

				QDBusMessage error = request.createErrorReply(errName, errMessage);
				if (!connection.send(error))
					qWarning() << "failed to send error reply to request" << request;
			};

		// create a lambda to convert the result into a dbus message and send it
		const  std::function<void()> finishedLambda =
			[connection, request] () {

				QDBusMessage reply = request.createReply();
				if (!connection.send(reply))
					qWarning() << "failed to send reply to request" << request;
			};


		// boilerplate to notify the dbus system that we will send the reply
		request.setDelayedReply(true);

		// check if the result is already available and if so just send a
		// reply, don't bother with attaching to the result signals
		if (result.isFinished()) {

			if (result.isError())
				errorLambda(result.errorName(), result.errorMessage());
			else
				finishedLambda();

		} else {

			// connect the lambda's to the finished and error signals
			result.connectFinished(this, finishedLambda);
			result.connectErrored(this, errorLambda);
		}
	}

	// -------------------------------------------------------------------------
	/*!
		Connects an \l{Future} object to the a dbus request message with a
		custom function ton convert the \a result to \l{QList<QVariant>}.


	 */
	template <typename R>
	void connectFutureToDBusReply(const QDBusMessage &request,
	                              const Future<R> &result,
	                              const std::function<QList<QVariant>(const R&)> &convertor) const
	{
		// sanity check we have the parents context, needed so we can get the
		// dbus connection that the request was sent on
		if (Q_UNLIKELY(m_parentContext == nullptr)) {
			qWarning("missing parent dbus context");
			return;
		}

		const QDBusConnection connection(m_parentContext->connection());

		// create a lambda to create an error message and send it
		const std::function<void(const QString&, const QString&)> errorLambda =
			[connection, request] (const QString &errName, const QString &errMessage) {

				QDBusMessage error = request.createErrorReply(errName, errMessage);
				if (!connection.send(error))
					qWarning() << "failed to send error reply to request" << request;
			};

		// create a lambda to convert the result into a dbus message and send it
		const  std::function<void(const R&)> finishedLambda =
			[connection, request, convertor] (const R &r) {

				QDBusMessage reply = request.createReply(convertor(r));
				if (!connection.send(reply))
					qWarning() << "failed to send reply to request" << request;
			};


		// boilerplate to notify the dbus system that we will send the reply
		request.setDelayedReply(true);

		// check if the result is already available and if so just send a
		// reply, don't bother with attaching to the result signals
		if (result.isFinished()) {

			if (result.isError())
				errorLambda(result.errorName(), result.errorMessage());
			else
				finishedLambda(result.result());

		} else {

			// connect the lambda's to the finished and error signals
			result.connectFinished(this, finishedLambda);
			result.connectErrored(this, errorLambda);
		}
	}

protected:

	// -------------------------------------------------------------------------
	/*!
		The Qt DBus / Property system doesn't support natively sending out
		property change notifications as signals (it's something they really
		should add), anyway I've implemented this helper to wrap up the
		boilerplate to support it.

		The format of the signal is:
		\code
			org.freedesktop.DBus.Properties.PropertiesChanged (STRING interface_name,
			                                                   DICT<STRING,VARIANT> changed_properties,
			                                                   ARRAY<STRING> invalidated_properties);
		\endcode

		Calling this function will send a \c org.freedesktop.DBus.Properties.PropertiesChanged
		signal with the \c interface_name gleaned from the current class, the
		\c changed_properties dictionary is created from \a propertyName and
		\a propertyValue arguments, the \c invalidated_properties is left
		empty.

	 	The notification will be sent on all dbus connections the parent object
	 	is registered on.

	 */
	template <typename T1>
	bool sendPropertyChangeNotification(const QString &objectPath,
	                                    const QString &propertyName,
	                                    const T1 &propertyValue) const
	{
		// interrogate this object for the "D-Bus Interface" class info, i.e
		// the value set by the Q_CLASSINFO("D-Bus Interface", ...) macro
		const QMetaObject *metaObj_ = metaObject();
		int ifaceNameIndex = metaObj_->indexOfClassInfo("D-Bus Interface");
		if (ifaceNameIndex < 0) {
			qWarning("class doesn't have a Q_CLASSINFO(\"D-Bus Interface\", ..) entry");
			return false;
		}

		const QMetaClassInfo ifaceNameInfo = metaObj_->classInfo(ifaceNameIndex);


		// prepare the arguments for the signal
		const QVariant value = QVariant::fromValue<T1>(propertyValue);
		if (!value.isValid()) {
			qWarning("failed to convert propery value");
			return false;
		}

		const QVariantMap changedProps = { { propertyName, value } };
		const QStringList invalidatedProps = { };


		// create the dbus signal
		QDBusMessage propChangeSignal =
			QDBusMessage::createSignal(objectPath,
			                           QStringLiteral("org.freedesktop.DBus.Properties"),
			                           QStringLiteral("PropertiesChanged"));

		propChangeSignal << QString(ifaceNameInfo.value());
		propChangeSignal << changedProps;
		propChangeSignal << invalidatedProps;

		// send the signal on all dbus connections
		for (const QDBusConnection &dbusConn : m_dbusConnections) {
			if (!dbusConn.send(propChangeSignal))
				qWarning("failed to send property change notification signal");
		}

		return true;
	}


private:
	QList<QDBusConnection> m_dbusConnections;
	QDBusContext *m_parentContext;

};


#endif // DBUSABSTRACTADAPTOR_H
