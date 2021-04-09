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
//  edid.h
//  SkyBluetoothRcu
//

#ifndef EDID_H
#define EDID_H

#include <QString>
#include <QByteArray>
#include <QVersionNumber>

class Edid
{
public:
	Edid();
	Edid(const Edid &other);
	Edid(const QByteArray &data);

	Edid &operator=(const Edid &rhs);

public:
	bool isValid() const;

	QByteArray data() const;

	QVersionNumber version() const;
	QString manufacturerId() const;
	quint16 pnpId() const;
	quint16 productCode() const;
	quint32 serialNumber() const;

public:
	static QString pnpIdToString(quint16 pnpId);
	static quint16 stringToPnPId(const QString &str);

	static bool checkEdid(const QByteArray &data);

private:
	friend bool operator!=(const Edid &lhs, const Edid &rhs);
	friend bool operator==(const Edid &lhs, const Edid &rhs);

private:
	bool m_valid;
	quint8 m_data[128];

};

bool operator!=(const Edid &lhs, const Edid &rhs);
bool operator==(const Edid &lhs, const Edid &rhs);

#endif // !defined(EDID_H)
