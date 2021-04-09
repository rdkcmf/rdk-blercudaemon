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
//  dbusabstractinterface.h
//  SkyBluetoothRcu
//

#ifndef DBUSABSTRACTINTRFACE_H
#define DBUSABSTRACTINTRFACE_H

#include <QObject>
#include <QString>

#include <QDBusConnection>
#include <QDBusAbstractInterface>



class DBusAbstractInterface : public QDBusAbstractInterface
{
	Q_OBJECT

protected:
	explicit DBusAbstractInterface(const QString &service,
	                               const QString &path,
	                               const char *interface,
	                               const QDBusConnection &connection,
	                               QObject *parent);
	void connectNotify(const QMetaMethod &signal) Q_DECL_OVERRIDE;
	void disconnectNotify(const QMetaMethod &signal) Q_DECL_OVERRIDE;

private slots:
	void onPropertiesChanged(const QString& interfaceName,
	                         const QVariantMap& changedProperties,
	                         const QStringList& invalidatedProperties);

private:
	bool isSignalPropertyNotification(const QMetaMethod &signal) const;

	void invokeNotifySignal(const QMetaMethod &method,
	                        const QVariant &value,
	                        int propertyType);

private:
	bool m_connected;
	bool m_propChangedConnected;

	static const QString m_dbusPropertiesInterface;
	static const QString m_dbusPropertiesChangedSignal;

};


#endif // DBUSABSTRACTINTRFACE_H
