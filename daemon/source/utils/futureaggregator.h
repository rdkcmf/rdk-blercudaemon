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
//  futureaggregator.h
//  SkyBluetoothRcu
//

#ifndef FUTUREAGGREGATOR_H
#define FUTUREAGGREGATOR_H

#include "future.h"

#include <QObject>
#include <QString>
#include <QList>
#include <QSharedPointer>


class FutureAggregator : public QObject
{
	Q_OBJECT

public:
	explicit FutureAggregator(const QList< Future<> > &futures,
	                 QObject *parent = nullptr);
	explicit FutureAggregator(const QList< Future<> > &&futures,
	                 QObject *parent = nullptr);
	~FutureAggregator() override;

public:
	bool isEmpty() const;
	QList< Future<> > futures() const;

	bool isFinished() const;
	bool isRunning() const;
	bool isError() const;

	QString errorName() const;
	QString errorMessage() const;

	Future<> future();

signals:
	void finished();
	void errored(const QString &errorName, const QString &errorMessage);

private slots:
	void onFutureFinished();
	void onFutureErrored(const QString &error, const QString &message);

private:
	void connectSignals();
	void onAllFuturesFinished();

private:
	QList< Future<> > m_futures;
	bool m_signalledFinished;
	QString m_errorName;
	QString m_errorMessage;

	QSharedPointer< Promise<> > m_promise;
};



#endif // !defined(FUTUREAGGREGATOR_H)
