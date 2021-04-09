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
//  dbusadaptorhelpers.h
//  BleRcuDaemon
//

#ifndef DBUSADAPTORHELPERS_H
#define DBUSADAPTORHELPERS_H

#include "future.h"

#include <QObject>
#include <QMetaClassInfo>
#include <QDebug>

#include <QString>
#include <QVariant>
#include <QSharedPointer>

#include <QDBusMessage>
#include <QDBusConnection>

#include <functional>


template <class T>
class DBusAdaptorHelpers
{

protected:

	// -------------------------------------------------------------------------
	/*!
		Sends out a dbus error message with \a name and \a message strings.  The
		\a message string may be empty.

	 */
	void sendError(const QDBusConnection &connection,
	               const QDBusMessage &request,
	               const QString &errorName,
	               const QString &errorMessage) const
	{
		// boilerplate to notify the dbus system that we will send the reply
		request.setDelayedReply(true);

		// create the error message and send it
		QDBusMessage error = request.createErrorReply(errorName, errorMessage);
		if (!connection.send(error))
			qWarning() << "failed to send error reply" << errorName << errorMessage;
	}

	// -------------------------------------------------------------------------
	/*!
		Sends out a dbus method reply message with the supplied \a result.

	 */
	void sendResult(const QDBusConnection &connection,
	                const QDBusMessage &request,
	                const QVariant &result) const
	{
		// boilerplate to notify the dbus system that we will send the reply
		request.setDelayedReply(true);

		QDBusMessage reply = request.createReply();
		if (result.isValid())
			reply << result;

		if (!connection.send(reply))
			qWarning("failed to send reply");
	}

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
	void connectFutureToDBusReply(const QDBusConnection &connection,
	                              const QDBusMessage &request,
	                              const Future<R> &result) const
	{
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
			result.connectFinished(static_cast<const T*>(this), finishedLambda);
			result.connectErrored(static_cast<const T*>(this), errorLambda);
		}
	}

	// -------------------------------------------------------------------------
	/*!
		This is a specialisation for an \l{Future} object with void template
		argument, i.e. the \a result doesn't contain a value.

	 */
	void connectFutureToDBusReply(const QDBusConnection &connection,
	                              const QDBusMessage &request,
	                              const Future<void> &result) const
	{
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
			result.connectFinished(static_cast<const T*>(this), finishedLambda);
			result.connectErrored(static_cast<const T*>(this), errorLambda);
		}
	}

	// -------------------------------------------------------------------------
	/*!
		Connects an \l{Future} object to the a dbus request message with a
		custom function ton convert the \a result to \l{QList<QVariant>}.


	 */
	template <typename R>
	void connectFutureToDBusReply(const QDBusConnection &connection,
	                              const QDBusMessage &request,
	                              const Future<R> &result,
	                              const std::function<QList<QVariant>(const R&)> &convertor) const
	{
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
			result.connectFinished(static_cast<const T*>(this), finishedLambda);
			result.connectErrored(static_cast<const T*>(this), errorLambda);
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

	 */
	template <typename T1>
	bool sendPropertyChangeNotification(const QDBusConnection &connection,
	                                    const QString &objectPath,
	                                    const QString &propertyName,
	                                    const T1 &propertyValue) const
	{
		// interrogate this object for the "D-Bus Interface" class info, i.e
		// the value set by the Q_CLASSINFO("D-Bus Interface", ...) macro
		const QMetaObject *metaObj_ = static_cast<const T*>(this)->metaObject();
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

		return connection.send(propChangeSignal);
	}

};

#endif // !defined(DBUSADAPTORHELPERS_H)
