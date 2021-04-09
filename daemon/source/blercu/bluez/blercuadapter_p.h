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
//  blercuadapter_p.h
//  SkyBluetoothRcu
//

#ifndef BLUEZ_BLERCUADAPTER_P_H
#define BLUEZ_BLERCUADAPTER_P_H

#include "../blercuadapter.h"
#include "utils/statemachine.h"
#include "utils/hcisocket.h"
#include "dbus/dbusobjectmanager.h"
#include "configsettings/configmodelsettings.h"

#include <QSet>
#include <QVector>

#include <QtDBus>


class BleRcuNotifier;
class BleRcuDeviceBluez;
class BleRcuServicesFactory;
class ConfigSettings;

class DBusObjectManagerInterface;
class BluezAdapterInterface;


class BleRcuAdapterBluez : public BleRcuAdapter
{
	Q_OBJECT

public:
	BleRcuAdapterBluez(const QSharedPointer<const ConfigSettings> &config,
	                   const QSharedPointer<BleRcuServicesFactory> &servicesFactory,
	                   const QDBusConnection &bluezBusConn,
	                   QObject *parent = nullptr);
	~BleRcuAdapterBluez();

private:
	void initStateMachine();

	QDBusObjectPath findAdapter(int timeout = 2000);
	bool attachAdapter(const QDBusObjectPath &adapterPath);
	bool setAdapterDiscoveryFilter();

	void getRegisteredDevices();

public:
	bool isValid() const override;
	bool isAvailable() const override;
	bool isPowered() const override;

	bool isDiscovering() const override;
	bool startDiscovery(int pairingCode) override;
	bool stopDiscovery() override;

	bool isPairable() const override;
	bool enablePairable(int timeout) override;
	bool disablePairable() override;

	QSet<BleAddress> pairedDevices() const override;

	QMap<BleAddress, QString> deviceNames() const override;

	QSharedPointer<BleRcuDevice> getDevice(const BleAddress &address) const override;

	bool isDevicePaired(const BleAddress &address) const override;

	bool addDevice(const BleAddress &address) override;
	bool removeDevice(const BleAddress &address) override;

	void dump(Dumper out) const override;

private slots:
	void onBluezServiceRegistered(const QString &serviceName);
	void onBluezServiceUnregistered(const QString &serviceName);

	void onAdapterPowerChanged(bool powered);
	void onAdapterDiscoveringChanged(bool discovering);
	void onAdapterPairableChanged(bool pairable);

	void onBluezInterfacesAdded(const QDBusObjectPath &objectPath,
	                            const DBusInterfaceList &interfacesAndProperties);
	void onBluezInterfacesRemoved(const QDBusObjectPath &objectPath,
	                              const QStringList &interfaces);

	void onDeviceNameChanged(const BleAddress &address, const QString &name);
	void onDevicePairedChanged(const BleAddress &address, bool paired);
	void onDeviceReadyChanged(const BleAddress &address, bool ready);

	void onStartDiscoveryReply(QDBusPendingCallWatcher *call);
	void onStopDiscoveryReply(QDBusPendingCallWatcher *call);
	void onRemoveDeviceReply(QDBusPendingCallWatcher *call);
	void onPowerOnReply(QDBusPendingCallWatcher *call);

	void onDiscoveryWatchdog();

	void onStateEntry(int state);
	void onStateExit(int state);

private:
	void onDeviceAdded(const QDBusObjectPath &objectPath,
	                   const QVariantMap &properties);
	void onDeviceRemoved(const QDBusObjectPath &objectPath);

private slots:
	void onPowerCycleAdapter();
	void onDisconnectReconnectDevice(const BleAddress &device);

private:
	const QSharedPointer<BleRcuServicesFactory> m_servicesFactory;
	const QDBusConnection m_bluezDBusConn;

	const QString m_bluezService;
	QSharedPointer<QDBusServiceWatcher> m_bluezServiceWatcher;
	QSharedPointer<DBusObjectManagerInterface> m_bluezObjectMgr;

	BleAddress m_address;
	QDBusObjectPath m_adapterObjectPath;
	QSharedPointer<BluezAdapterInterface> m_adapterProxy;

	QMap<BleAddress, QSharedPointer<BleRcuDeviceBluez>> m_devices;

	QSharedPointer<HciSocket> m_hciSocket;

private:
	bool m_discovering;
	bool m_pairable;

	int m_discoveryRequests;
	enum { StartDiscovery, StopDiscovery } m_discoveryRequested;
	QTimer m_discoveryWatchdog;

private:
	static QSet<quint32> getSupportedOuis(const QList<ConfigModelSettings> &details);
	static QVector<QRegExp> getSupportedPairingNames(const QList<ConfigModelSettings> &details);

	const QSet<quint32> m_supportedOuis;
	const QVector<QRegExp> m_supportedPairingNames;

private:
	enum State {
		ServiceUnavailableState,
		ServiceAvailableSuperState,
			AdapterUnavailableState,
			AdapterAvailableSuperState,
				AdapterPoweredOffState,
				AdapterPoweredOnState,
		ShutdownState
	};

	StateMachine m_stateMachine;

	qint64 m_retryEventId;

	static const QEvent::Type ServiceRetryEvent         = QEvent::Type(QEvent::User + 1);
	static const QEvent::Type ServiceAvailableEvent     = QEvent::Type(QEvent::User + 2);
	static const QEvent::Type ServiceUnavailableEvent   = QEvent::Type(QEvent::User + 3);

	static const QEvent::Type AdapterRetryAttachEvent   = QEvent::Type(QEvent::User + 4);
	static const QEvent::Type AdapterAvailableEvent     = QEvent::Type(QEvent::User + 5);
	static const QEvent::Type AdapterUnavailableEvent   = QEvent::Type(QEvent::User + 6);

	static const QEvent::Type AdapterRetryPowerOnEvent  = QEvent::Type(QEvent::User + 7);
	static const QEvent::Type AdapterPoweredOnEvent     = QEvent::Type(QEvent::User + 8);
	static const QEvent::Type AdapterPoweredOffEvent    = QEvent::Type(QEvent::User + 9);

	static const QEvent::Type ShutdownEvent             = QEvent::Type(QEvent::User + 10);

	void onEnteredServiceUnavailableState();
	void onExitedServiceAvailableSuperState();
	void onEnteredAdapterUnavailableState();
	void onExitedAdapterAvailableSuperState();
	void onEnteredAdapterPoweredOffState();
	void onEnteredAdapterPoweredOnState();
	void onExitedAdapterPoweredOnState();

};

#endif // !defined(BLUEZ_BLERCUADAPTER_P_H)
