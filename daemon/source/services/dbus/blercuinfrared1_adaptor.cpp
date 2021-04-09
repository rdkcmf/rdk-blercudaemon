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
//  blercuinfrared1_adaptor.cpp
//  SkyBluetoothRcu
//

#include "blercuinfrared1_adaptor.h"

#include "blercu/blercuerror.h"
#include "blercu/blercudevice.h"
#include "blercu/bleservices/blercuinfraredservice.h"


BleRcuInfrared1Adaptor::BleRcuInfrared1Adaptor(const QSharedPointer<BleRcuDevice> &device,
                                               QObject *parent)
	: DBusAbstractAdaptor(parent)
	, m_device(device)
{
	// this interface doesn't use signals
	setAutoRelaySignals(false);
}


BleRcuInfrared1Adaptor::~BleRcuInfrared1Adaptor()
{
	// destructor
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Utility function for converting \a flags received from dbus to search
	options
 */
BleRcuInfraredService::SearchOptions BleRcuInfrared1Adaptor::flagsToSearchOptions(quint32 flags)
{
	// sanity check and convert the flags
	BleRcuInfraredService::SearchOptions options = 0;
	if (flags & 0x01)
		options |= BleRcuInfraredService::SortAlphabetically;
	if (flags & 0x02)
		options |= BleRcuInfraredService::NoTelevisions;
	if (flags & 0x04)
		options |= BleRcuInfraredService::NoAVAmplifiers;

	return options;
}

// -----------------------------------------------------------------------------
/*!
	DBus get property call for com.sky.blercu.Infrared1.GetCodes

 */
void BleRcuInfrared1Adaptor::GetCodes(const QString &manufacturer,
                                      const QString &model,
                                      quint32 flags,
                                      const QDBusMessage &request)
{
	// convert the flags
	BleRcuInfraredService::SearchOptions options = flagsToSearchOptions(flags);

	// get the service, perform the request and attach the result to the dbus reply
	const QSharedPointer<const BleRcuInfraredService> service = m_device->infraredService();
	Future<IrCodeList> results = service->codeIds(manufacturer, model, options);

	connectFutureToDBusReply(request, results);
}

// -----------------------------------------------------------------------------
/*!
	DBus get property call for com.sky.blercu.Infrared1.GetCodesFromEDID

 */
void BleRcuInfrared1Adaptor::GetCodesFromEDID(const QByteArray &edid,
                                              const QDBusMessage &request)
{
	QByteArray edidToUse = edid;

	// get the service, perform the request and attach the result to the dbus reply
	const QSharedPointer<const BleRcuInfraredService> service = m_device->infraredService();
	Future<IrCodeList> results = service->codeIds(edidToUse);

	connectFutureToDBusReply(request, results);
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Utility function for converting a \l{BleRcuInfraredService::SearchResults}
	object to a \l{QList<QVariant>} as would be expected for sending results
	over dbus.
 */
QList<QVariant> BleRcuInfrared1Adaptor::convertSearchResults(const BleRcuInfraredService::SearchResults &results)
{
	return QList<QVariant>( { QVariant::fromValue(results.maxResults),
	                          QVariant::fromValue(results.results) } );
}

// -----------------------------------------------------------------------------
/*!
	DBus get property call for com.sky.blercu.Infrared1.GetManufacturers

 */
void BleRcuInfrared1Adaptor::GetManufacturers(const QString &search,
                                              quint32 flags,
                                              qint64 offset, qint64 limit,
                                              const QDBusMessage &request)
{
	// convert the flags
	BleRcuInfraredService::SearchOptions options = flagsToSearchOptions(flags);

	// get the service, perform the request and attach the result to the dbus reply
	const QSharedPointer<const BleRcuInfraredService> service = m_device->infraredService();
	Future<BleRcuInfraredService::SearchResults> results = service->brands(search, options, offset, limit);

	const std::function<QList<QVariant>(const BleRcuInfraredService::SearchResults&)>
		converter = &BleRcuInfrared1Adaptor::convertSearchResults;

	connectFutureToDBusReply(request, results, converter);
}

// -----------------------------------------------------------------------------
/*!
	DBus get property call for com.sky.blercu.Infrared1.GetModels

 */
void BleRcuInfrared1Adaptor::GetModels(const QString &manufacturer,
                                       const QString &search,
                                       quint32 flags,
                                       qint64 offset, qint64 limit,
                                       const QDBusMessage &request)
{
	// convert the flags
	BleRcuInfraredService::SearchOptions options = flagsToSearchOptions(flags);

	// get the service, perform the request and attach the result to the dbus reply
	const QSharedPointer<const BleRcuInfraredService> service = m_device->infraredService();
	Future<BleRcuInfraredService::SearchResults> results = service->models(manufacturer, search, options, offset, limit);

	const std::function<QList<QVariant>(const BleRcuInfraredService::SearchResults&)>
		converter = &BleRcuInfrared1Adaptor::convertSearchResults;
	
	connectFutureToDBusReply(request, results, converter);
}



