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
//  dbusbrokerservice.cpp
//  DBus
//

#include "dbusbrokerservice.h"
#include "logging.h"
#include "androidparcel.h"

#include <QtAndroidExtras>
#include <QThread>



// android.os.IBinder.FIRST_CALL_TRANSACTION value
#define FIRST_CALL_TRANSACTION          1

// these indices must match the order of the functions in the AIDL file
#define TRANSACTION_systemBusAddress    int(FIRST_CALL_TRANSACTION + 0)
#define TRANSACTION_sessionBusAddress   int(FIRST_CALL_TRANSACTION + 1)



DBusBrokerService::DBusBrokerService(QObject *parent)
	: QObject(parent)
	, m_serviceName("com.sky.dbusbroker")
	, m_binderInterfaceName(m_serviceName + ".IDBusBrokerService")
{
}

DBusBrokerService::~DBusBrokerService()
{
}


// -----------------------------------------------------------------------------
/*!
	Returns \c true if the service is available.

 */
bool DBusBrokerService::isAvailable() const
{
	// if we have a binder interface then the service is available
	if (m_binder)
		return true;

	// otherwise re-check the service is there
	return checkServiceAvailable(false);
}

// -----------------------------------------------------------------------------
/*!
	Blocks the calling thread until either the service is up and available or
	the \a msecs time out occurs.

	Returns \c true if the process was started successfully; otherwise returns
	\c false (if the operation timed out or if an error occurred).

	This function can operate without an event loop.

	If \a msecs is -1, this function will not time out.

 */
bool DBusBrokerService::waitForAvailable(int msecs)
{
	// unfortunately we have to poll on the service being available, AFAICT
	// there is no way to get a signal when a service is registered
	const unsigned long delaySeq[] = { 100, 100, 200, 300, 500, 800, 1300, 2100 };
	int delayIdx = 0;

	// if no timeout then the same as isAvailable()
	if (msecs == 0)
		return isAvailable();
	if (msecs < 0)
		msecs = -1;

	// setup the deadline
	QDeadlineTimer deadline(msecs);

	// poll on the service being available
	for (delayIdx = 0; !deadline.hasExpired(); ) {

		if (checkServiceAvailable(false))
			break;

		QThread::msleep(delaySeq[delayIdx]);
		delayIdx = qBound(0, (delayIdx + 1), 7);
	}

	if (deadline.hasExpired())
		return false;

	// poll on receiving a valid system bus address
	for (delayIdx = 0; !deadline.hasExpired(); ) {

		const QString busAddress = systemBusAddress();
		if (!busAddress.isEmpty())
			return true;

		QThread::msleep(delaySeq[delayIdx]);
		delayIdx = qBound(0, (delayIdx + 1), 7);
	}

	return false;
}

// -----------------------------------------------------------------------------
/*!
	Returns the address of the dbus system bus provided by the dbus-broker
	service.  If the service is not available or the dbus-broker service hasn't
	yet setup the system bus then a null string is returned.

 */
QString DBusBrokerService::systemBusAddress() const
{
	// check the service is available
	if (!isAvailable())
		return QString();


	QAndroidJniEnvironment env;

	// set the data to send to the service
	AndroidParcel data;
	data.writeInterfaceToken(m_binderInterfaceName);

	// call the method
	QAndroidParcel reply;
	if (!m_binder->transact(TRANSACTION_systemBusAddress, data, &reply)) {
		qError("failed to issue 'systemBusAddress' transaction");
		return QString();
	}

	AndroidParcel result(reply);

	// check for any exceptions
	result.readException();
	if (env->ExceptionCheck()) {
		qError("exception occurred in IBinder transaction");
		env->ExceptionDescribe();
		env->ExceptionClear();
		return QString();
	}

	// finally read the result (it may be empty)
	return result.readString();
}

// -----------------------------------------------------------------------------
/*!
	Returns the address of the dbus session bus provided by the dbus-broker
	service.

 */
QString DBusBrokerService::sessionBusAddress() const
{
	qError("session bus not implemented");
	return QString();
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Checks if the service is available, if not a timer is started to re-check
	the service at a later time.

 */
bool DBusBrokerService::checkServiceAvailable(bool logErrors) const
{
	// sanity check we don't already have the binder interface
	if (Q_UNLIKELY(m_binder)) {
		qDebug("already have binder interface");
		return true;
	}


	QAndroidJniEnvironment env;

	// create the service name
	const QAndroidJniObject serviceName =
		QAndroidJniObject::fromString("com.sky.dbusbroker");

	// call the static android ServiceManager.addService() function passing
	// it the jni handle for this class (inherited from QAndroidBinder)
	const QAndroidJniObject binder =
		QAndroidJniObject::callStaticObjectMethod("android/os/ServiceManager",
		                                          "getService",
		                                          "(Ljava/lang/String;)Landroid/os/IBinder;",
		                                          serviceName.object<jstring>());
	if (env->ExceptionCheck()) {
		if (logErrors) {
			qError("failed to get DBusBrokerService service from the system");
			env->ExceptionDescribe();
		}
		env->ExceptionClear();

	} else if (!binder.isValid()) {
		if (logErrors)
			qError("failed to get DBusBrokerService service from the system");

	} else {
		m_binder = QSharedPointer<QAndroidBinder>::create(binder);
	}

	return !m_binder.isNull();
}

