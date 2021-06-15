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
//  configmodelsettings.h
//  SkyBluetoothRcu
//

#ifndef CONFIGMODELSETTINGS_H
#define CONFIGMODELSETTINGS_H

#include "utils/bleconnectionparameters.h"

#include <QSet>
#include <QDebug>
#include <QString>
#include <QSharedPointer>
#include <QJsonObject>
#include <QRegExp>


class ConfigModelSettingsData;


class ConfigModelSettings
{
public:
	ConfigModelSettings();
	ConfigModelSettings(const ConfigModelSettings &other);
	~ConfigModelSettings();

private:
	friend class ConfigSettings;
	explicit ConfigModelSettings(const QJsonObject &json);

public:
	bool isValid() const;

	QString manufacturer() const;
	QString name() const;
	quint32 oui() const;

	bool disabled() const;

	QByteArray pairingNameFormat() const;
	QRegExp scanNameMatcher() const;

	QSet<quint8> irFilterBytes() const;

	QString standbyMode() const;

public:
	enum ServicesType {
		DBusServiceType,
		GattServiceType
	};

	ServicesType servicesType() const;

public:
	enum Service {
		NoServices = 0x00,
		AudioService = 0x01,
		BatteryService = 0x02,
		DeviceInfoService = 0x04,
		FindMeService = 0x08,
		InfraredService = 0x10,
		TouchService = 0x20,
		UpgradeService = 0x40,
		RemoteControlService = 0x80,
	};
	Q_DECLARE_FLAGS(Services, Service)

	Services servicesSupported() const;

public:
	bool hasBleConnParams() const;
	BleConnectionParameters bleConnParams() const;

public:
	struct DBusServiceDetails {
		QString objectPath;
		QString serviceName;
	};

	DBusServiceDetails dbusServiceDetails() const;

private:
	QSharedPointer<ConfigModelSettingsData> d;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(ConfigModelSettings::Services)

QDebug operator<<(QDebug dbg, const ConfigModelSettings &settings);


#endif // !defined(CONFIGMODELSETTINGS_H)
