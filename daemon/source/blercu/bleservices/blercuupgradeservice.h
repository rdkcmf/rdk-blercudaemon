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
//  blercuupgradeservice.h
//  SkyBluetoothRcu
//

#ifndef BLERCUUPGRADESERVICE_H
#define BLERCUUPGRADESERVICE_H

#include "utils/future.h"
#include "utils/fwimagefile.h"

#include <QObject>



class BleRcuUpgradeService : public QObject
{
	Q_OBJECT

protected:
	explicit BleRcuUpgradeService(QObject *parent)
		: QObject(parent)
	{ }

public:
	~BleRcuUpgradeService() override = default;

public:
	virtual Future<> startUpgrade(const QSharedPointer<FwImageFile> &fwFile) = 0;
	virtual Future<> cancelUpgrade() = 0;

	virtual bool upgrading() const = 0;
	virtual int progress() const = 0;

signals:
	void upgradingChanged(bool upgrading);
	void progressChanged(int progress);
	void error(const QString &message);

};


#endif // !defined(BLERCUUPGRADESERVICE_H)

