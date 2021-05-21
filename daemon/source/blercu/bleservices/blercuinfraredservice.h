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
//  blercuinfraredservice.h
//  SkyBluetoothRcu
//

#ifndef BLERCUINFRAREDSERVICE_H
#define BLERCUINFRAREDSERVICE_H

#include "utils/future.h"

#include <QObject>
#include <QString>
#include <QList>
#include <QSet>
#include <QByteArray>
#include <QSharedPointer>


typedef QList<qint32> IrCodeList;


class BleRcuInfraredService : public QObject
{
	Q_OBJECT

protected:
	explicit BleRcuInfraredService(QObject *parent)
		: QObject(parent)
	{ }

public:
	~BleRcuInfraredService() override = default;

public:
	virtual Future<> emitIrSignal(Qt::Key keyCode) = 0;

public:
	virtual qint32 codeId() const = 0;

	virtual Future<> eraseIrSignals() = 0;
	virtual Future<> programIrSignals(qint32 codeId,
	                                  const QSet<Qt::Key> &keyCodes) = 0;
	virtual Future<> programIrSignalWaveforms(const QMap<Qt::Key, QByteArray> &irWaveforms) = 0;

public:
	enum SearchOption {
		NoOptions = 0x0,
		SortAlphabetically = 0x1,
		NoTelevisions = 0x2,
		NoAVAmplifiers = 0x4,
	};
	Q_DECLARE_FLAGS(SearchOptions, SearchOption)

	struct SearchResults {
		quint64 maxResults;
		QStringList results;
	};

	inline Future<SearchResults> brands(const QString &search,
	                                    SearchOptions options = NoOptions) const
	{
		return this->brands(search, options, -1, -1);
	}
	virtual Future<SearchResults> brands(const QString &search,
	                                     SearchOptions options,
	                                     qint64 offset,
	                                     qint64 limit) const = 0;

	inline Future<SearchResults> models(const QString &brand,
	                                    const QString &search,
	                                    SearchOptions options = NoOptions) const
	{
		return this->models(brand, search, options, -1, -1);
	}
	virtual Future<SearchResults> models(const QString &brand,
	                                     const QString &search,
	                                     SearchOptions options,
	                                     qint64 offset,
	                                     qint64 limit) const = 0;

	inline Future<IrCodeList> codeIds(const QString &brand,
	                                  const QString &model) const
	{
		return this->codeIds(brand, model, NoOptions);
	}
	virtual Future<IrCodeList> codeIds(const QString &brand,
	                                   const QString &model,
	                                   SearchOptions options) const = 0;
	virtual Future<IrCodeList> codeIds(const QByteArray &edid) const = 0;

signals:
	void codeIdChanged(qint32 codeId);

};


Q_DECLARE_METATYPE(BleRcuInfraredService::SearchResults)


#endif // !defined(BLERCUINFRAREDSERVICE_H)
