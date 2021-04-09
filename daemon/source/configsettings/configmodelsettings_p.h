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
//  configmodelsettings_p.h
//  SkyBluetoothRcu
//

#ifndef CONFIGMODELSETTINGS_P_H
#define CONFIGMODELSETTINGS_P_H

#include "utils/bleconnectionparameters.h"

#include <QRegExp>
#include <QString>
#include <QJsonObject>
#include <QSharedPointer>


class ConfigModelSettingsData
{
public:
	ConfigModelSettingsData();
	ConfigModelSettingsData(const QJsonObject &json);
	ConfigModelSettingsData(const ConfigModelSettingsData &other);
	~ConfigModelSettingsData();

private:
	bool stringToServiceFlag(const QString &name,
	                         ConfigModelSettings::Service *service) const;

public:
	bool m_valid;

	quint32 m_oui;
	QString m_name;
	QString m_manufacturer;
	bool m_disabled;
	QByteArray m_pairingNameFormat;
	QRegExp m_scanNameMatcher;
	QSet<quint8> m_filterBytes;

	bool m_hasConnParams;
	BleConnectionParameters m_connParams;

	QString m_dbusObjectPath;
	QString m_dbusServiceName;

	ConfigModelSettings::ServicesType m_servicesType;
	ConfigModelSettings::Services m_servicesSupported;

};


#endif // !defined(CONFIGMODELSETTINGS_P_H)
