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
//  DBusAbstractInterface.cpp
//  BleRcuDaemon
//

#include "dbusabstractinterface.h"

#include <QDebug>
#include <QMetaProperty>

#include <QDBusMetaType>
#include <QDBusArgument>



// -----------------------------------------------------------------------------
/*!
	\class DBusAbstractInterface
	\brief Wrapper around the QDBusAbstractInterface class to provide notify
	signals for property changes.

	The dbus specification defines the org.freedesktop.DBus.Properties interface
	for getting / setting properties, Qt already implements this, however it
	doesn't implement a handler for the org.freedesktop.DBus.Properties.PropertiesChanged
	signal.

	This is a problem for us as bluetoothd uses this to notify us of all sorts
	of things; i.e. scan on/off, powered on/off, etc

	\see https://randomguy3.wordpress.com/2010/09/07/the-magic-of-qtdbus-and-the-propertychanged-signal/
	\see https://github.com/nemomobile/qtdbusextended


	So how do you use this class ?
	   1. Generate an interface class using the Qt qdbusxml2cpp tool
	   2. Change the generated class so that it inherits from the
	      \a DBusAbstractInterface class rather than \a QDBusAbstractInterface
	   3. Add the \c NOTIFY option to the properties of the generated class and
	      matching signals (just like you would for an ordinary notify property)

 */


const QString DBusAbstractInterface::m_dbusPropertiesInterface("org.freedesktop.DBus.Properties");
const QString DBusAbstractInterface::m_dbusPropertiesChangedSignal("PropertiesChanged");


DBusAbstractInterface::DBusAbstractInterface(const QString &service,
                                             const QString &path,
                                             const char *interface,
                                             const QDBusConnection &connection,
                                             QObject *parent)
	: QDBusAbstractInterface(service, path, interface, connection, parent)
	, m_propChangedConnected(false)
{

	// get the dbus connection object, the supplied one is const
	QDBusConnection conn = QDBusAbstractInterface::connection();

	// setup the argument match so we filter out other properties changes on
	// this object that don't belong to this interface
	QStringList argumentMatch;
	argumentMatch << QDBusAbstractInterface::interface();

	// get the service name of the proxy, on peer-to-peer connections (i.e. in
	// unit tests) the service name should be empty
	QString serviceName = QDBusAbstractInterface::service();
#if defined(AI_BUILD_TYPE) && (AI_BUILD_TYPE == AI_DEBUG)
	if (connection.isConnected() && connection.baseService().isEmpty())
		serviceName.clear();
#endif

	// try and connect to this signal for this object
	if (!conn.connect(serviceName,
	                  QDBusAbstractInterface::path(),
	                  m_dbusPropertiesInterface,
	                  m_dbusPropertiesChangedSignal,
	                  argumentMatch,
	                  QStringLiteral("sa{sv}as"),
	                  this, SLOT(onPropertiesChanged(QString, QVariantMap, QStringList)))) {

		qWarning() << "failed to connect to PropertiesChanged signal for object"
		           << QDBusAbstractInterface::path() << "and interface"
		           << QDBusAbstractInterface::interface();
	}

}

// -----------------------------------------------------------------------------
/*!
	Checks whether the supplied method matches a notification signal, if it
	is \c true is returned.

	Returns \c true if the suppled method is a signal that is attached to a
	property as a change notification. i.e. the signal that is supplied after
	the \c NOTIFY key word in the \c Q_PROPERTY declaraction.

 */
bool DBusAbstractInterface::isSignalPropertyNotification(const QMetaMethod &signal) const
{
	if (Q_UNLIKELY(signal.methodType() != QMetaMethod::Signal))
		return false;

	const int index = signal.methodIndex();

	// loop through the properties, check if any have a notify signal and if
	// so does the index of the notify signal match the supplied signal
	const int numProps = metaObject()->propertyCount();
	for (int i = 0; i < numProps; i++) {

		const QMetaProperty prop = metaObject()->property(i);

		if (prop.isValid() && prop.hasNotifySignal() && (prop.notifySignalIndex() == index))
			return true;
	}

	return false;
}

// -----------------------------------------------------------------------------
/*!
	Called by QT when someone connects/disconnects from one of our signals.

	We override this as normally QDBusAbstractInterface would handle this and
	use this point to install a dbus match filter with the signal name. Why?
	because any signals you put in class are assumed to be signals sent by
	the remote object over dbus.  However this is not true of property change
	notify signals, rather these are signals used to notify of a change to
	a property as sent by the org.freedesktop.DBus.Properties.PropertiesChanged
	signal ... so we want to stop the base class (QDBusAbstractInterface) from
	install dbus match filters for these signals.

 */
void DBusAbstractInterface::connectNotify(const QMetaMethod &signal)
{
	// TODO: Change this so we only connect to the change dbus signal when the
	// first slot is attached to the notification event

	if (!isSignalPropertyNotification(signal))
		QDBusAbstractInterface::connectNotify(signal);
}

void DBusAbstractInterface::disconnectNotify(const QMetaMethod &signal)
{
	if (!isSignalPropertyNotification(signal))
		QDBusAbstractInterface::disconnectNotify(signal);
}

// -----------------------------------------------------------------------------
/*!
	dbus callback called when PropertiesChanged signal is received

	Handles the \c org.freedesktop.DBus.Properties.PropertiesChanged signal.

	This will go through the list of properties that have changed and searches
	to see if this object has a \c Q_PROPERTY that matches, if it does and that
	property has a \c NOTIFY signal we call the signal.

	The notify signal may either have no args or a single arg that matches
	the property type.  This code will handle both types.

	Note more than one property change could be signalled by this method

 */
void DBusAbstractInterface::onPropertiesChanged(const QString& interfaceName,
                                                const QVariantMap& changedProperties,
                                                const QStringList& invalidatedProperties)
{
	Q_UNUSED(invalidatedProperties);

	// sanity check the interface is correct
	if (Q_UNLIKELY(interfaceName != QDBusAbstractInterface::interface())) {
		qWarning() << "odd, received PropertiesChanged signal from wrong interface";
		return;
	}

	// iterate through the changed properties
	QVariantMap::const_iterator it = changedProperties.begin();
	for (; it != changedProperties.end(); ++it) {

		const QString &propName = it.key();
		const QVariant &propValue = it.value();

		// try and find the meta index of the named property
		const int propIndex = metaObject()->indexOfProperty(propName.toLatin1().constData());
		if (propIndex < 0) {
			qWarning("odd, no meta property for %s.%s",
			      qPrintable(interfaceName), qPrintable(propName));
			continue;
		}

		// check if the property has the notify flag, if not ignore the change
		const QMetaProperty propMeta = metaObject()->property(propIndex);
		if (!propMeta.hasNotifySignal()) {
			qDebug("skipping property change notification for %s.%s",
			       qPrintable(interfaceName), qPrintable(propName));
			continue;
		}

		const QMetaMethod notifyMethod = propMeta.notifySignal();
		if (Q_UNLIKELY(!notifyMethod.isValid())) {
			qWarning("odd, invalid property notify signal for %s.%s",
			         qPrintable(interfaceName), qPrintable(propName));
			continue;
		}

		// invoke the notify signal
		invokeNotifySignal(notifyMethod, propName, propValue, propMeta.userType());
	}

	// TODO: handle invalidatedProperties as well
}

// -----------------------------------------------------------------------------
/*!
	Invokes the supplied signal with the variant value and it's type

 */
void DBusAbstractInterface::invokeNotifySignal(const QMetaMethod &method,
                                               const QString &name,
                                               const QVariant &value,
                                               int propertyType)
{
	// check if the notify signal has any parameters, if not then no point
	// demarshalling the dbus parameters and instead just emit the signal
	// and return
	if (method.parameterCount() == 0) {
		method.invoke(this);
		return;
	}

	// it has argument(s), it should only have one that matches the property
	// type and therefore should already support native demarshalling
	if (Q_UNLIKELY(method.parameterCount() > 1)) {
		qWarning("to many parameters for '%s' property notify signal",
		         qPrintable(name));
		return;
	}

	// sanity check that the notify signal takes the same type as the actual
	// property, if you see the following error then check that the signal
	// in the Q_PROPERTY(... NOTIFY <signal>) takes a matching argument type
	if (Q_UNLIKELY(propertyType != method.parameterType(0))) {
		qWarning("'%s' property notify signal takes inconsistent parameter type",
		         qPrintable(name));
		return;
	}

	// if the received property type matches then
	if (value.userType() == propertyType) {

		QGenericArgument argument(QMetaType::typeName(propertyType),
		                          const_cast<void*>(value.constData()));
		method.invoke(this, Qt::DirectConnection, argument);


	// if the received property type is a bit more complex ...
	} else if (value.userType() == qMetaTypeId<QDBusArgument>()) {

		const QDBusArgument dbusValue = value.value<QDBusArgument>();

		// get the expected signature from the actual qt properties ...
		const char *expectedSignature = QDBusMetaType::typeToSignature(propertyType);

		// ... then check against what dbus sent us
		if (dbusValue.currentSignature() != expectedSignature) {
			qWarning("mismatch signature on property '%s' notify signal, "
			         "expected %s actual %s", qPrintable(name),
			         expectedSignature, qPrintable(dbusValue.currentSignature()));

		} else {

			QVariant result = QVariant(propertyType, nullptr);
			if (!QDBusMetaType::demarshall(dbusValue, propertyType, result.data()) ||
				!result.isValid()) {
				qWarning("failed to demarshall property '%s' value", qPrintable(name));

			} else {

				QGenericArgument argument(QMetaType::typeName(result.userType()),
				                          const_cast<void*>(result.constData()));
				method.invoke(this, Qt::DirectConnection, argument);
				
			}
		}
		
	}
	
}

// -----------------------------------------------------------------------------
/*!
	Performs a org.freedesktop.DBus.Properties.Get method call requesting
	the given property, however it returns the pending reply rather than
	blocking.

 */
QDBusPendingReply<QDBusVariant> DBusAbstractInterface::asyncProperty(const char *name) const
{
	// construct the message
	QDBusMessage msg = QDBusMessage::createMethodCall(QDBusAbstractInterface::service(),
	                                                  QDBusAbstractInterface::path(),
	                                                  m_dbusPropertiesInterface,
	                                                  QStringLiteral("Get"));
	msg << QDBusAbstractInterface::interface();
	msg << QString::fromUtf8(name);


	// get the dbus connection object and send out the message
	const QDBusConnection conn = QDBusAbstractInterface::connection();
	return conn.asyncCall(msg);
}

// -----------------------------------------------------------------------------
/*!
	Performs a org.freedesktop.DBus.Properties.Set method call requesting
	a set on the given property, however it returns the pending reply rather
	than blocking.

 */
QDBusPendingReply<> DBusAbstractInterface::asyncSetProperty(const char *name,
                                                            const QVariant &value) const
{
	// construct the message
	QDBusMessage msg = QDBusMessage::createMethodCall(QDBusAbstractInterface::service(),
	                                                  QDBusAbstractInterface::path(),
	                                                  m_dbusPropertiesInterface,
	                                                  QStringLiteral("Set"));
	msg << QDBusAbstractInterface::interface();
	msg << QString::fromUtf8(name);
	msg << QVariant::fromValue(QDBusVariant(value));

	// get the dbus connection object and send out the message
	const QDBusConnection conn = QDBusAbstractInterface::connection();
	return conn.asyncCall(msg);
}

// -----------------------------------------------------------------------------
/*!
	Performs a org.freedesktop.DBus.Properties.GetAll method call requesting
	the all properties of a given interface.  This is a non-block async call.

 */
QDBusPendingReply<DBusPropertiesMap> DBusAbstractInterface::asyncGetAllProperties() const
{
	// construct the message
	QDBusMessage msg = QDBusMessage::createMethodCall(QDBusAbstractInterface::service(),
	                                                  QDBusAbstractInterface::path(),
	                                                  m_dbusPropertiesInterface,
	                                                  QStringLiteral("GetAll"));
	msg << QDBusAbstractInterface::interface();


	// get the dbus connection object and send out the message
	const QDBusConnection conn = QDBusAbstractInterface::connection();
	return conn.asyncCall(msg);
}

