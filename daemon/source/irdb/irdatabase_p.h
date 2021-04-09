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
//  irdatabase_p.h
//  SkyBluetoothRcu
//

#ifndef IRDATABASE_P_H
#define IRDATABASE_P_H

#include "irdatabase.h"
#include "sqlite3.h"

#include <QCache>
#include <QVariantList>


Q_DECLARE_OPAQUE_POINTER(sqlite3*)
Q_DECLARE_OPAQUE_POINTER(sqlite3_stmt*)


class IrDatabaseImpl : public IrDatabase
{
public:
	IrDatabaseImpl(const QString &dbPath);
	~IrDatabaseImpl();

public:
	bool isValid() const override;

	QStringList brands(Type type, const QString &search,
	                   quint64 *total = nullptr, qint64 offset = -1,
	                   qint64 limit = -1) const override;

	QStringList models(Type type, const QString &brand,
	                   const QString &search, quint64 *total = nullptr,
	                   qint64 offset = -1, qint64 limit = -1) const override;

	QList<int> codeIds(Type type, const QString &brand,
	                   const QString &model = QStringLiteral("")) const override;

	QList<int> codeIds(const Edid &edid) const override;

	IrSignalSet irSignals(RcuType rcuType, int codeId) const override;


private:
	void init(const QString &dbPath);

	QVariantList execQuery(const QString &query, const QVariantList &params,
	                       quint64 *total, qint64 offset, qint64 limit) const;

	int getBrandId(const QString &brand, Type type) const;

	QString normaliseString(const QString &string) const;

private:
	sqlite3 *m_sqliteDB;

	mutable QCache<QString, int> m_tvBrandsCache;
	mutable QCache<QString, int> m_ampBrandsCache;
};



#endif // !defined(IRDATABASE_P_H)

