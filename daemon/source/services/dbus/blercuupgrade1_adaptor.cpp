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
//  blercuupgrade1_adaptor.cpp
//  SkyBluetoothRcu
//

#include "blercuupgrade1_adaptor.h"

#include "blercu/blercudevice.h"
#include "blercu/blercuerror.h"
#include "blercu/bleservices/blercuupgradeservice.h"

#include "utils/logging.h"
#include "utils/filedescriptor.h"


BleRcuUpgrade1Adaptor::BleRcuUpgrade1Adaptor(const QSharedPointer<BleRcuDevice> &device,
                                             const QDBusObjectPath &objPath,
                                             QObject *parent)
	: DBusAbstractAdaptor(parent)
	, m_device(device)
	, m_dbusObjPath(objPath)
{
	// we use manual signals for property change notifications only
	setAutoRelaySignals(false);

	// get the upgrade service and if valid connect to the progress change and
	// upgrade status notifications
	const QSharedPointer<const BleRcuUpgradeService> upgradeService = m_device->upgradeService();
	if (upgradeService) {
		QObject::connect(upgradeService.data(), &BleRcuUpgradeService::upgradingChanged,
		                 this, &BleRcuUpgrade1Adaptor::onUpgradingChanged);
		QObject::connect(upgradeService.data(), &BleRcuUpgradeService::progressChanged,
		                 this, &BleRcuUpgrade1Adaptor::onProgressChanged);

		QObject::connect(upgradeService.data(), &BleRcuUpgradeService::error,
		                 this, &BleRcuUpgrade1Adaptor::UpgradeError);
	}
}

BleRcuUpgrade1Adaptor::~BleRcuUpgrade1Adaptor()
{
	// destructor
}

// -----------------------------------------------------------------------------
/*!
	DBus get method call for com.sky.blercu.Upgrade1.StartUpgrade

 */
void BleRcuUpgrade1Adaptor::StartUpgrade(const QDBusUnixFileDescriptor &file,
                                         const QDBusMessage &request)
{
	// get the upgrade service, if doesn't exist then f/w upgrade is not
	// supported on this device
	const QSharedPointer<BleRcuUpgradeService> service = m_device->upgradeService();
	if (!service) {
		sendErrorReply(request, BleRcuError::errorString(BleRcuError::General),
		               QStringLiteral("Upgrade not supported on this device"));
		return;
	}

	// get / check the supplied file
	int fd;
	if (!file.isValid() || ((fd = file.fileDescriptor()) < 0)) {
		sendErrorReply(request, BleRcuError::errorString(BleRcuError::FileNotFound),
		               QStringLiteral("Invalid file descriptor supplied"));
		return;
	}

	// ask the service to start the upgrade
	Future<> results = service->startUpgrade(QSharedPointer<FwImageFile>::create(fd));
	connectFutureToDBusReply(request, results);
}

// -----------------------------------------------------------------------------
/*!
	DBus get method call for com.sky.blercu.Upgrade1.CancelUpgrade

 */
void BleRcuUpgrade1Adaptor::CancelUpgrade(const QDBusMessage &request)
{
	// get the upgrade service, if doesn't exist then f/w upgrade is not
	// supported on this device
	const QSharedPointer<BleRcuUpgradeService> service = m_device->upgradeService();
	if (!service) {
		sendErrorReply(request, BleRcuError::errorString(BleRcuError::General),
		               QStringLiteral("Upgrade not support on this device"));
		return;
	}

	// ask the service to start the upgrade
	Future<> results = service->cancelUpgrade();
	connectFutureToDBusReply(request, results);
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Slot called when the upgrading service for this device signals that the
	upgrading status has changed.

 */
void BleRcuUpgrade1Adaptor::onUpgradingChanged(bool upgrading)
{
	sendPropertyChangeNotification<bool>(m_dbusObjPath.path(),
	                                     QStringLiteral("Upgrading"), upgrading);
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Slot called when the upgrading service for this device signals that the
	progress of the upgrade has changed.

 */
void BleRcuUpgrade1Adaptor::onProgressChanged(int progress)
{
	sendPropertyChangeNotification<qint32>(m_dbusObjPath.path(),
	                                       QStringLiteral("Progress"), progress);
}

// -----------------------------------------------------------------------------
/*!
	DBus property getter for com.sky.blercu.Upgrade1.Upgrading

 */
bool BleRcuUpgrade1Adaptor::upgrading() const
{
	const QSharedPointer<const BleRcuUpgradeService> service = m_device->upgradeService();
	return !service.isNull() && service->upgrading();
}

// -----------------------------------------------------------------------------
/*!
	DBus property getter for com.sky.blercu.Upgrade1.Progress

 */
qint32 BleRcuUpgrade1Adaptor::progress() const
{
	const QSharedPointer<const BleRcuUpgradeService> service = m_device->upgradeService();
	return service.isNull() ? -1 : service->progress();
}

