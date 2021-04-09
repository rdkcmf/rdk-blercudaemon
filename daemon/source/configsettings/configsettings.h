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
//  configsettings.h
//  SkyBluetoothRcu
//

#ifndef CONFIGSETTINGS_H
#define CONFIGSETTINGS_H

#include "configmodelsettings.h"

#include <QDebug>
#include <QString>
#include <QList>
#include <QSharedPointer>

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>



class ConfigSettings
{
public:
	~ConfigSettings();

	static QSharedPointer<ConfigSettings> defaults();
	static QSharedPointer<ConfigSettings> fromJsonFile(const QString &filePath);
	static QSharedPointer<ConfigSettings> fromJsonFile(QIODevice *file);

private:
	struct TimeOuts {
		int discoveryMSecs;
		int pairingMSecs;
		int setupMSecs;
		int upairingMSecs;

		int hidrawWaitPollMSecs;
		int hidrawWaitLimitMSecs;
	};

private:
	friend class QSharedPointer<ConfigSettings>;
	ConfigSettings(const TimeOuts &timeouts,
	               QList<ConfigModelSettings> &&modelDetails);

public:
	int discoveryTimeout() const;
	int pairingTimeout() const;
	int setupTimeout() const;
	int upairingTimeout() const;

	int hidrawWaitPollTimeout() const;
	int hidrawWaitLimitTimeout() const;

	ConfigModelSettings modelSettings(quint32 oui) const;
	ConfigModelSettings modelSettings(QString name) const;
	QList<ConfigModelSettings> modelSettings() const;

private:
	static TimeOuts parseTimeouts(const QJsonObject &json);

private:
	const TimeOuts m_timeOuts;
	const QList<ConfigModelSettings> m_modelDetails;
};

QDebug operator<<(QDebug dbg, const ConfigSettings &settings);


#endif // !defined(CONFIGSETTINGS_H)
