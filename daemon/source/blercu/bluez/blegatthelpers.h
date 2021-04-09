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
//  blegatthelpers.h
//  SkyBluetoothRcu
//

#ifndef BLUEZ_BLEGATTHELPERS_H
#define BLUEZ_BLEGATTHELPERS_H

#include "utils/future.h"

#include <QDebug>
#include <QSharedPointer>
#include <QDBusPendingReply>



template<typename T = void>
struct dbusPendingReplyToFutureImpl;

template<typename T>
Future<T> dbusPendingReplyToFuture(const QDBusPendingReply<T> &pendingReply)
{
	return dbusPendingReplyToFutureImpl<T>::fn(pendingReply);
}

template<typename T>
struct dbusPendingReplyToFutureImpl
{
	static Future<T> fn(const QDBusPendingReply<T> &pendingReply)
	{
		QSharedPointer<Promise<T>> promise = QSharedPointer<Promise<T>>::create();

		// create a new dbus pending reply watcher, it will be freed when the
		// call has received a reply or timed-out
		QDBusPendingCallWatcher *dbusWatcher = new QDBusPendingCallWatcher(pendingReply);

		// create a lambda function, c programmers look away now before you wretch ...
		// the following effectively defines an inline function, that owns a weak
		// copy of the watcher shared pointer and takes an argument of type
		// QDBusPendingCallWatcher*.  We then store this inline function in the
		// variable called lambda, it will be called when the dbus call completes
		std::function<void(QDBusPendingCallWatcher*)> lambda =
			[promise] (QDBusPendingCallWatcher *call) {

				// sanity check the supplied call pointer is valid (needed?)
				if (Q_UNLIKELY(call == nullptr)) {
					qWarning("missing dbus watcher call in slot");
					return;
				}

				// check for error, if it is an error this is bad
				QDBusPendingReply<T> reply = *call;
				if (reply.isError()) {
					const QDBusError error = reply.error();
					promise->setError(error.name(), error.message());

				} else {
					// not an error so get the result
					// qDebug() << "received dbus result" << reply.argumentAt(0);
					promise->setFinished(reply.value());
				}

				// clean up the pending call on the next time through the event loop
				call->deleteLater();
			};

		// now we want the above lambda to be called when the dbus request has
		// finished
		QObject::connect(dbusWatcher, &QDBusPendingCallWatcher::finished, lambda);

		// and that's it we're done, now when the dbus request finishes the lambda
		// will be called which will signal the promise object and free the
		// pending object
		return promise->future();
	}

};

template<>
struct dbusPendingReplyToFutureImpl<void>
{
	static Future<> fn(const QDBusPendingReply<> &pendingReply)
	{
		QSharedPointer<Promise<>> promise = QSharedPointer<Promise<>>::create();

		// create a new dbus pending reply watcher, it will be freed when the
		// call has received a reply or timed-out
		QDBusPendingCallWatcher *dbusWatcher = new QDBusPendingCallWatcher(pendingReply);

		// create a lambda function, c programmers look away now before you wretch ...
		// the following effectively defines an inline function, that owns a weak
		// copy of the watcher shared pointer and takes an argument of type
		// QDBusPendingCallWatcher*.  We then store this inline function in the
		// variable called lambda, it will be called when the dbus call completes
		std::function<void(QDBusPendingCallWatcher*)> lambda =
			[promise] (QDBusPendingCallWatcher *call) {

				// sanity check the supplied call pointer is valid (needed?)
				if (Q_UNLIKELY(call == nullptr)) {
					qWarning("missing dbus watcher call in slot");
					return;
				}

				// check for error, if it is an error this is bad
				QDBusPendingReply<> reply = *call;
				if (reply.isError()) {
					const QDBusError error = reply.error();
					promise->setError(error.name(), error.message());

				} else {
					// not an error so get the result
					// qDebug() << "received dbus result" << reply.argumentAt(0);
					promise->setFinished();
				}

				// clean up the pending call on the next time through the event loop
				call->deleteLater();
			};

		// now we want the above lambda to be called when the dbus request has
		// finished
		QObject::connect(dbusWatcher, &QDBusPendingCallWatcher::finished, lambda);

		// and that's it we're done, now when the dbus request finishes the lambda
		// will be called which will signal the watcher object
		return promise->future();
	}
};


#endif // !defined(BLUEZ_BLEGATTHELPERS_H)
