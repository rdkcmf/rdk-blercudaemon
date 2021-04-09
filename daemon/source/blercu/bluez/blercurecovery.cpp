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
//  blercurecovery.cpp
//  SkyBluetoothRcu
//

#include "blercurecovery.h"

#include <QCoreApplication>
#include <QAbstractEventDispatcher>
#include <QReadWriteLock>
#include <QPointer>



BleRcuRecovery *BleRcuRecovery::instance()
{
	static QReadWriteLock lock_;
	static QPointer<BleRcuRecovery> instance_;

	lock_.lockForRead();
	if (instance_.isNull()) {

		lock_.unlock();
		lock_.lockForWrite();

		if (instance_.isNull()) {

			instance_ = new BleRcuRecovery(QAbstractEventDispatcher::instance());

			QObject::connect(qApp, &QCoreApplication::aboutToQuit,
			                 instance_.data(), &QObject::deleteLater);
		}
	}

	lock_.unlock();
	return instance_.data();
}


BleRcuRecovery::BleRcuRecovery(QObject *parent)
	: QObject(parent)
{
}

BleRcuRecovery::~BleRcuRecovery()
{
}
