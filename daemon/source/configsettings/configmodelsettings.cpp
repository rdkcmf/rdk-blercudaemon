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
//  configmodelsettings.cpp
//  SkyBluetoothRcu
//

#include "configmodelsettings.h"
#include "configmodelsettings_p.h"

#include <QJsonArray>

#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
#include <QDBusObjectPath>
#endif // !defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)




ConfigModelSettingsData::ConfigModelSettingsData()
	: m_valid(false)
	, m_disabled(false)
	, m_servicesType(ConfigModelSettings::DBusServiceType)
	, m_servicesSupported(0)
{
}

ConfigModelSettingsData::ConfigModelSettingsData(const ConfigModelSettingsData &other)
	: m_valid(other.m_valid)
	, m_oui(other.m_oui)
	, m_name(other.m_name)
	, m_manufacturer(other.m_manufacturer)
	, m_disabled(other.m_disabled)
	, m_pairingNameFormat(other.m_pairingNameFormat)
	, m_filterBytes(other.m_filterBytes)
	, m_hasConnParams(other.m_hasConnParams)
	, m_connParams(other.m_connParams)
	, m_servicesType(other.m_servicesType)
	, m_servicesSupported(other.m_servicesSupported)
{
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Converts the \a name string to service flag, the flag is returned in the
	\a service argument.
 
	If \a name doesn't match any service then \c false is returned, otherwise
	\c true is returned.
 */
bool ConfigModelSettingsData::stringToServiceFlag(const QString &name,
                                                  ConfigModelSettings::Service *service) const
{
	if (name.compare("audio", Qt::CaseInsensitive) == 0)
		*service = ConfigModelSettings::AudioService;
	else if (name.compare("battery", Qt::CaseInsensitive) == 0)
		*service = ConfigModelSettings::BatteryService;
	else if (name.compare("deviceInfo", Qt::CaseInsensitive) == 0)
		*service = ConfigModelSettings::DeviceInfoService;
	else if (name.compare("findMe", Qt::CaseInsensitive) == 0)
		*service = ConfigModelSettings::FindMeService;
	else if (name.compare("infrared", Qt::CaseInsensitive) == 0)
		*service = ConfigModelSettings::InfraredService;
	else if (name.compare("touch", Qt::CaseInsensitive) == 0)
		*service = ConfigModelSettings::TouchService;
	else if (name.compare("upgrade", Qt::CaseInsensitive) == 0)
		*service = ConfigModelSettings::UpgradeService;
	else
		return false;

	return true;
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Constructs some vendor settings from the supplied json object, if the json
	object has errors then an invalid object is created.

	The object should be formatted like the following:

	\code
		[
			{
				"name": "EC05x",
				"manufacturer": "Ruwido",
				"oui": "1C:A2:B1",
				"pairingNameFormat": "U%03hhu*",
				"connectionParams": {
					"maxInterval": 15.0,
					"minInterval": 15.0,
					"latency": 332,
					"supervisionTimeout": 15000
				},
				"services": {
					"type": "dbus",
					"dbusServiceName": "com.ruwido.rcu",
					"dbusObjectPath": "/com/ruwido/rcu"
					"supported": [
						"audio",
						"battery",
						"deviceInfo",
						"findMe",
						"infrared",
						"touch"
					]
				}
			},
			...
		]
	\endcode

 */
ConfigModelSettingsData::ConfigModelSettingsData(const QJsonObject &json)
	: m_valid(false)
	, m_disabled(false)
	, m_hasConnParams(false)
	, m_servicesSupported(0)
{
	// name field
	{
		const QJsonValue name = json["name"];
		if (!name.isString()) {
			qWarning("invalid 'name' field");
			return;
		}
		m_name = name.toString();
	}

	// manufacturer field
	{
		const QJsonValue manufacturer = json["manufacturer"];
		if (!manufacturer.isString())
			qWarning("invalid or missing 'manufacturer' field");
		else
			m_manufacturer = manufacturer.toString();
	}

	// oui field
	{
		const QJsonValue oui = json["oui"];
		if (!oui.isString()) {
			qWarning("invalid 'oui' field");
			return;
		}

		quint8 ouiBytes[3];
		if (sscanf(oui.toString().toLatin1().constData(), "%02hhx:%02hhx:%02hhx",
		           &ouiBytes[0], &ouiBytes[1], &ouiBytes[2]) != 3) {
			qWarning("invalid 'oui' field");
			return;
		}

		m_oui = (quint32(ouiBytes[0]) << 16) |
		        (quint32(ouiBytes[1]) << 8)  |
		        (quint32(ouiBytes[2]) << 0);
	}

	// (optional) disabled field
	if (json.contains("disabled")) {

		const QJsonValue disabled = json["disabled"];
		if (!disabled.isBool()) {
			qWarning("invalid 'disabled' field");
			return;
		}

		m_disabled = disabled.toBool();

		// (bit of a hack) but override the config setting for the ECO80 if
		// on an italian build
#if defined(ENABLE_EC080_CONFIG)
		if (m_disabled && (m_name == "EC080")) {
		    m_disabled = false;
		}
#endif // defined(ENABLE_EC080_CONFIG)
	}

	// pairingFormat field
	{
		const QJsonValue pairingFormat = json["pairingNameFormat"];
		if (!pairingFormat.isString()) {
			qWarning("invalid 'pairingNameFormat' field");
			return;
		}
		m_pairingNameFormat = pairingFormat.toString().toLatin1();
	}

	// scanNameFormat field
	{
		const QJsonValue scanNameFormat = json["scanNameFormat"];
		if (!scanNameFormat.isString()) {
			qWarning("invalid 'scanNameFormat' field");
			return;
		}
		m_scanNameMatcher = QRegExp(scanNameFormat.toString(),
		                            Qt::CaseInsensitive,
		                            QRegExp::Wildcard);
	}

	// filterByte field
	{
		const QJsonValue filterBytes = json["filterBytes"];
		if (!filterBytes.isArray()) {
			qWarning("invalid 'filterBytes' field");
			return;
		}

		const QJsonArray filterByteValues = filterBytes.toArray();
		for (const auto &filterByte : filterByteValues) {
			if (!filterByte.isDouble())
				qWarning("invalid entry in 'filterBytes' array");
			else
				m_filterBytes.insert(static_cast<quint8>(filterByte.toInt()));
		}
	}

	// services field
	{
		const QJsonValue services = json["services"];
		if (!services.isObject()) {
			qWarning("missing or invalid 'services' field");
			return;
		}

		const QJsonObject servicesObject = services.toObject();

		// services.type field
		{
			const QJsonValue type = servicesObject["type"];
			if (!type.isString()) {
				qWarning("invalid 'service.type' field");
				return;
			}

			const QString typeStr = type.toString();
			if (typeStr.compare("dbus", Qt::CaseInsensitive) == 0)
				m_servicesType = ConfigModelSettings::DBusServiceType;
			else if (typeStr.compare("gatt", Qt::CaseInsensitive) == 0)
				m_servicesType = ConfigModelSettings::GattServiceType;
			else {
				qWarning("invalid 'service.type' field value");
				return;
			}
		}

#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
		// services.dbus fields
		if (m_servicesType == ConfigModelSettings::DBusServiceType) {

			const QJsonValue serviceName = servicesObject["dbusServiceName"];
			const QJsonValue objectPath = servicesObject["dbusObjectPath"];
			if (!serviceName.isString() || !objectPath.isString()) {
				qWarning("invalid 'dbusServiceName' or 'dbusObjectPath' field");
				return;
			}

			QDBusObjectPath objectPathCheck(objectPath.toString());
			if (objectPathCheck.path().isEmpty()) {
				qWarning("invalid 'dbusObjectPath' field");
				return;
			}

			m_dbusObjectPath = objectPathCheck.path();
			m_dbusServiceName = serviceName.toString();
		}
#endif // defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)

		// services.supported array
		{
			const QJsonValue servicesSupported = servicesObject["supported"];
			if (!servicesSupported.isArray()) {
				qWarning("missing or invalid 'services.supported' field");
				return;
			}

			const QJsonArray servicesArray = servicesSupported.toArray();
			for (const QJsonValue &service : servicesArray) {
				if (!service.isString()) {
					qWarning("invalid 'services.supported' array entry");
					return;
				}

				ConfigModelSettings::Service serviceFlag;
				if (!stringToServiceFlag(service.toString(), &serviceFlag)) {
					qWarning() << "invalid service name" << service;
					return;
				}

				m_servicesSupported |= serviceFlag;
			}
		}
	}

	// (optional) connectionParams
	if (json.contains("connectionParams")) {

		const QJsonValue connParams = json["connectionParams"];
		if (!connParams.isObject()) {
			qWarning("invalid 'connectionParams' field");
			return;
		}

		const QJsonObject connParamsObj = connParams.toObject();
		const QJsonValue maxInterval = connParamsObj["maxInterval"];
		const QJsonValue minInterval = connParamsObj["minInterval"];

		if (maxInterval.isDouble() && minInterval.isDouble()) {
			m_connParams.setIntervalRange(minInterval.toDouble(),
			                              maxInterval.toDouble());
		} else if (maxInterval.type() != minInterval.type()) {
			qWarning("both 'maxInterval' and 'minInterval' must be set to set "
			         "connection interval");
		}

		const QJsonValue latency = connParamsObj["latency"];

		if (latency.isDouble())
			m_connParams.setLatency(latency.toInt(m_connParams.latency()));
		else if (!latency.isUndefined())
			qWarning("invalid type for latency setting");

		const QJsonValue supervisionTimeout = connParamsObj["supervisionTimeout"];

		if (supervisionTimeout.isDouble())
			m_connParams.setSupervisionTimeout(supervisionTimeout.toInt(m_connParams.supervisionTimeout()));
		else if (!supervisionTimeout.isUndefined())
			qWarning("invalid type for supervisionTimeout setting");

		m_hasConnParams = true;
	}

	m_valid = true;
}

ConfigModelSettingsData::~ConfigModelSettingsData()
{
}





ConfigModelSettings::ConfigModelSettings()
	: d(QSharedPointer<ConfigModelSettingsData>::create())
{
}

ConfigModelSettings::ConfigModelSettings(const ConfigModelSettings &other)
	: d(other.d)
{
}

ConfigModelSettings::ConfigModelSettings(const QJsonObject &json)
	: d(QSharedPointer<ConfigModelSettingsData>::create(json))
{
}

ConfigModelSettings::~ConfigModelSettings()
{
}

// -----------------------------------------------------------------------------
/*!
	Returns \c true if the settings are valid.  If the object is invalid then
	all the getters will return undefined results.

 */
bool ConfigModelSettings::isValid() const
{
	return d && d->m_valid;
}

// -----------------------------------------------------------------------------
/*!
	Returns the model name of the RCU.

 */
QString ConfigModelSettings::name() const
{
	return d->m_name;
}

// -----------------------------------------------------------------------------
/*!
	Returns the manufacturer name of the RCU model.

 */
QString ConfigModelSettings::manufacturer() const
{
	return d->m_manufacturer;
}

// -----------------------------------------------------------------------------
/*!
	The organizationally unique identifier (OUI) that identifies the device
	as belonging to this vendor.  The OUI is the first 24-bits of the BDADDR
	(MAC address) of the device.

 */
quint32 ConfigModelSettings::oui() const
{
	return d->m_oui;
}

// -----------------------------------------------------------------------------
/*!
	Returns \c true if the model is disable, i.e. any of these types of RCU that
	try to pair will be rejected and any that are already paired won't be
	managed by this code.

	This was added for the Amidala RCU (EC080), and is better than just removing
	them from the config as it allows us to clean up any boxes that may have
	already paired RCUs in the past.

 */
bool ConfigModelSettings::disabled() const
{
	return d->m_disabled;
}

// -----------------------------------------------------------------------------
/*!
	Printf style format for a regex / wildcard pattern for matching against
	the vendor devices during pairing.

 */
QByteArray ConfigModelSettings::pairingNameFormat() const
{
	return d->m_pairingNameFormat;
}

// -----------------------------------------------------------------------------
/*!
	Returns a regex that can be used to match a SkyQ RCU device in pairing
	mode during a scan.

	This is different from the \a pairingNameFormat() in that is a printf
	style format that expects a pairing byte value to be applied to it to create
	matcher for a single device.  This is a matcher for any device in pairing
	mode.

 */
QRegExp ConfigModelSettings::scanNameMatcher() const
{
	return d->m_scanNameMatcher;
}

// -----------------------------------------------------------------------------
/*!
	Returns the IR filter byte value that the RCU model will send when pairing.

 */
QSet<quint8> ConfigModelSettings::irFilterBytes() const
{
	return d->m_filterBytes;
}

// -----------------------------------------------------------------------------
/*!
	Returns \c true if the special connection parameters should be set for
	the vendor's devices.  Currently this only applies to Ruwido RCUs that
	have voice search ability.

	\see bleConnParams()
 */
bool ConfigModelSettings::hasBleConnParams() const
{
	return d->m_hasConnParams;
}

// -----------------------------------------------------------------------------
/*!
	Returns the connection parameters that should be used for the vendor's
	device.  If hasBleConnParams() returns \c false the returned value is
	undefined.

	\see hasBleConnParams()
 */
BleConnectionParameters ConfigModelSettings::bleConnParams() const
{
	return d->m_connParams;
}

// -----------------------------------------------------------------------------
/*!
	Returns the service type used for the model

 */
ConfigModelSettings::ServicesType ConfigModelSettings::servicesType() const
{
	return d->m_servicesType;
}

// -----------------------------------------------------------------------------
/*!
	Returns the dbus service name and object path for the root object of the
 	vendor daemon.

 	Returns empty service name and object path if the service type is not equal
 	to ConfigModelSettings::DBusServiceType.

 */
ConfigModelSettings::DBusServiceDetails ConfigModelSettings::dbusServiceDetails() const
{
	ConfigModelSettings::DBusServiceDetails details;

	if (d->m_servicesType == ConfigModelSettings::DBusServiceType) {
		details.objectPath = d->m_dbusObjectPath;
		details.serviceName = d->m_dbusServiceName;
	}

	return details;
}

// -----------------------------------------------------------------------------
/*!
	Returns a mask of the services supported by the vendor daemon.

 */
ConfigModelSettings::Services ConfigModelSettings::servicesSupported() const
{
	return d->m_servicesSupported;
}

// -----------------------------------------------------------------------------
/*!
	Debugging function to dump out the settings.

 */
QDebug operator<<(QDebug dbg, const ConfigModelSettings &settings)
{
	QDebugStateSaver saver(dbg);

	dbg.nospace() << "ConfigModelSettings("
	              << "name=" << settings.name() << ", "
	              << "manuf=" << settings.manufacturer() << ", "
	              << "oui=" << settings.oui() << ", "
	              << "pairingFmt=" << settings.pairingNameFormat() << ", ";

	switch (settings.servicesType()) {
		case ConfigModelSettings::GattServiceType:
			dbg.nospace() << "type=gatt, ";
			break;
		case ConfigModelSettings::DBusServiceType:
			{
				ConfigModelSettings::DBusServiceDetails details = settings.dbusServiceDetails();
				dbg.nospace() << "type=dbus{path:" << details.objectPath << ","
				              << "service:" << details.serviceName << "}";
			}
			break;
	}

	if (settings.hasBleConnParams())
		dbg.nospace() << settings.bleConnParams();
	else
		dbg.nospace() << "bleConnParams=null";

	dbg.nospace() << ")";

	return dbg;
}

