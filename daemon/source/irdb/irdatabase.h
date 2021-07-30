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
//  irdatabase.h
//  SkyBluetoothRcu
//

#ifndef IRDATABASE_H
#define IRDATABASE_H

#include "irsignalset.h"

#include <QString>
#include <QStringList>
#include <QFlags>
#include <QList>
#include <QtPlugin>

class Edid;

class IrDatabase
{
public:
	virtual ~IrDatabase()
	{ }

public:
	enum Type {
		UnknownType,
		Televisions,
		AVAmplifiers,
	};

	enum RcuType {
		EC05x,
		EC080,  // not currently supported
		EC10x,
		EC20x,
	};

public:
	virtual bool isValid() const = 0;

	virtual QStringList brands(Type type, const QString &search,
	                           quint64 *total = nullptr, qint64 offset = -1,
	                           qint64 limit = -1) const = 0;

	virtual QStringList models(Type type, const QString &brand,
	                           const QString &search, quint64 *total = nullptr,
	                           qint64 offset = -1, qint64 limit = -1) const = 0;

	virtual QList<int> codeIds(Type type, const QString &brand,
	                           const QString &model = QString("")) const = 0;
	virtual QList<int> codeIds(const Edid &edid) const = 0;

	virtual IrSignalSet irSignals(RcuType rcuType, int codeId) const = 0;

};

Q_DECLARE_INTERFACE(IrDatabase, "IrDatabaseInterface")


#endif // !defined(IRDATABASE_H)
