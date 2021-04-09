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
//  configsettings.cpp
//  SkyBluetoothRcu
//

#include "configsettings.h"

#include <QFile>
#include <QJsonObject>



// -----------------------------------------------------------------------------
/*!
	\internal

	Parses the \a json object to extract the timeout values (in milliseconds).
	The object must be formatted like the following

	\code{.json}
		{
			"discovery": 15000,
			"pair": 15000,
			"setup": 60000,
			"unpair": 20000,

			"hidrawPoll": 20000,
			"hidrawLimit": 65000
		}
	\endcode

	All values should be in milliseconds and correspond to the timeout used
	during the various stages of the pairing process.

	\see fromJsonFile()
 */
ConfigSettings::TimeOuts ConfigSettings::parseTimeouts(const QJsonObject &json)
{
	TimeOuts timeouts = { 15000, 15000, 60000, 20000, 20000, 65000 };

	struct {
		const char *name;
		int *storage;
	} fields[6] = {
		{ "discovery",      &timeouts.discoveryMSecs        },
		{ "pair",           &timeouts.pairingMSecs          },
		{ "setup",          &timeouts.setupMSecs            },
		{ "unpair",         &timeouts.upairingMSecs         },
		{ "hidrawPoll",     &timeouts.hidrawWaitPollMSecs   },
		{ "hidrawLimit",    &timeouts.hidrawWaitLimitMSecs  },
	};

	// process the fields
	for (unsigned int i = 0; i < (sizeof(fields) / sizeof(fields[0])); i++) {

		const QJsonValue value = json[fields[i].name];
		if (!value.isUndefined()) {
			if (!value.isDouble())
				qWarning("invalid '%s' field, reverting to default", fields[i].name);
			else
				*(fields[i].storage) = value.toInt(*(fields[i].storage));
		}
	}

	return timeouts;
}

// -----------------------------------------------------------------------------
/*!
	Returns the default config settings.

	Currently these settings are read from the defaultconfig.json file that is
	stored in the resources of this daemon object.

	\see fromJsonFile()
 */
QSharedPointer<ConfigSettings> ConfigSettings::defaults()
{
	return fromJsonFile(":defaultconfig.json");
}

// -----------------------------------------------------------------------------
/*!
	Parses a json config file and returns a \l{QSharedPointer} to a
	\l{ConfigSettings} object.  If the json is not valid or one or more of
	mandatory fields is missing or malformed then a null shared pointer is
	returned.

	\see defaults()
 */
QSharedPointer<ConfigSettings> ConfigSettings::fromJsonFile(const QString &filePath)
{
	// try and open the config file
	QFile file(filePath);
	if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
		qWarning() << "failed to open config file" << filePath;
		return QSharedPointer<ConfigSettings>();
	}

	// process the file
	return fromJsonFile(&file);
}

// -----------------------------------------------------------------------------
/*!
	Parses a json config file and returns a \l{QSharedPointer} to a
	\l{ConfigSettings} object.  If the json is not valid or one or more of
	mandatory fields is missing or malformed then a null shared pointer is
	returned.

	\see defaults()
 */
QSharedPointer<ConfigSettings> ConfigSettings::fromJsonFile(QIODevice *file)
{
	// parse the json
	QJsonParseError error;
	QJsonDocument jsonDoc = QJsonDocument::fromJson(file->readAll(), &error);
	if (jsonDoc.isNull()) {
		qWarning() << "failed to parse config file" << error.errorString();
		return QSharedPointer<ConfigSettings>();
	}

	// the json must be an object
	if (!jsonDoc.isObject()) {
		qWarning("json config file is not an object");
		return QSharedPointer<ConfigSettings>();
	}

	const QJsonObject jsonObj = jsonDoc.object();


	// find the timeout params
	QJsonValue timeoutsParam = jsonObj["timeouts"];
	if (!timeoutsParam.isObject()) {
		qWarning( "missing or invalid 'timeouts' field in config");
		return QSharedPointer<ConfigSettings>();
	}

	TimeOuts timeouts = parseTimeouts(timeoutsParam.toObject());


	// find the vendor details array
	QJsonValue jsonVendors = jsonObj["models"];
	if (!jsonVendors.isArray()) {
		qWarning("missing or invalid 'models' field in config");
		return QSharedPointer<ConfigSettings>();
	}

	QList<ConfigModelSettings> models;

	const QJsonArray jsonVendorsArray = jsonVendors.toArray();
	for (const QJsonValue jsonVendor : jsonVendorsArray) {

		if (!jsonVendor.isObject())
			continue;

		ConfigModelSettings modelSettings(jsonVendor.toObject());
		if (!modelSettings.isValid())
			continue;

		models.append(modelSettings);
	}

	// finally return the config
	return QSharedPointer<ConfigSettings>::create(timeouts, std::move(models));
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Internal constructor used when config is read from a json document.

	\see fromJsonFile()
 */
ConfigSettings::ConfigSettings(const TimeOuts &timeouts,
                               QList<ConfigModelSettings> &&modelDetails)
	: m_timeOuts(timeouts)
	, m_modelDetails(std::move(modelDetails))
{
}

// -----------------------------------------------------------------------------
/*!
	Deletes the settings.

 */
ConfigSettings::~ConfigSettings()
{
}

// -----------------------------------------------------------------------------
/*!
	Returns the settings for the model with the given \a oui value.  If no
 	matching model is found then an invalid ConfigModelSettings is returned.

 */
ConfigModelSettings ConfigSettings::modelSettings(quint32 oui) const
{
	for (const ConfigModelSettings &settings : m_modelDetails) {
		if (settings.oui() == oui)
			return settings;
	}

	return ConfigModelSettings();
}

// -----------------------------------------------------------------------------
/*!
	Returns the settings for the model with the given \a name value.  If no
 	matching model is found then an invalid ConfigModelSettings is returned.

 */
ConfigModelSettings ConfigSettings::modelSettings(QString name) const
{
	for (const ConfigModelSettings &settings : m_modelDetails) {
		if (settings.scanNameMatcher().exactMatch(name))
			return settings;
	}

	return ConfigModelSettings();
}

// -----------------------------------------------------------------------------
/*!
	Returns list of model settings from the config params.

 */
QList<ConfigModelSettings> ConfigSettings::modelSettings() const
{
	return m_modelDetails;
}

// -----------------------------------------------------------------------------
/*!
	Returns the discovery timeout in milliseconds to use when attempting to
	pair to a new RCU.

	The default value is 15000ms.
 */
int ConfigSettings::discoveryTimeout() const
{
	return m_timeOuts.discoveryMSecs;
}

// -----------------------------------------------------------------------------
/*!
	Returns the pairing timeout in milliseconds to use when attempting to
	pair to a new RCU.  This is the time from when the pair request was made to
	bluez and the time that an acknowledgement was received.

	The default value is 15000ms.
 */
int ConfigSettings::pairingTimeout() const
{
	return m_timeOuts.pairingMSecs;
}

// -----------------------------------------------------------------------------
/*!
	Returns the setup timeout in milliseconds to use when attempting to
	pair to a new RCU.  This is the time from when the device is connected and
	paired but we are fetching the details about the device from the vendor
	daemon.

	The default value is 60000ms.
 */
int ConfigSettings::setupTimeout() const
{
	return m_timeOuts.setupMSecs;
}

// -----------------------------------------------------------------------------
/*!
	Returns the un-pairing timeout in milliseconds to use when attempting to
	pair to a new RCU.  This timeout is used on a failed pairing attempting and
	we want to unpair the device that we tried to pair with.

	The default value is 20000ms.
 */
int ConfigSettings::upairingTimeout() const
{
	return m_timeOuts.upairingMSecs;
}

// -----------------------------------------------------------------------------
/*!
	Returns the interval in milliseconds on which to poll for the hidraw
	device to be present whilst configuring the bluetooth device.

	The default value is 20000ms.

	\see hidrawWaitLimitTimeout()
 */
int ConfigSettings::hidrawWaitPollTimeout() const
{
	return m_timeOuts.hidrawWaitPollMSecs;
}

// -----------------------------------------------------------------------------
/*!
	Returns the timeout in milliseconds in which to wait for the hidraw device
	to show up once bluez tells us that the device is connected and paired.
	If this limit is exceeded we (typically) kick off some recovery mechanism
	as it indicates something has gone wrong in the kernel / daemon.

	The default value is 65000ms.

	\see hidrawWaitLimitTimeout()
 */
int ConfigSettings::hidrawWaitLimitTimeout() const
{
	return m_timeOuts.hidrawWaitLimitMSecs;
}

// -----------------------------------------------------------------------------
/*!
	Debugging function to dump out the settings.

 */
QDebug operator<<(QDebug dbg, const ConfigSettings &settings)
{
	QDebugStateSaver saver(dbg);

	dbg.nospace() << "ConfigSettings("
	              << "discoveryTimeout=" << settings.discoveryTimeout() << "ms, "
	              << "pairingTimeout="   << settings.pairingTimeout() << "ms, "
	              << "setupTimeout="     << settings.setupTimeout() << "ms, "
	              << "upairingTimeout="  << settings.upairingTimeout() << "ms, "
	              << "modelSettings="    << settings.modelSettings().length()
	              << ")";

	return dbg;
}
