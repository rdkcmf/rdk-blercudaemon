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
//  main.cpp
//  SkyBluetoothRcu
//

#include <QCoreApplication>
#include <QtGlobal>
#include <QSharedPointer>
#include <QDebug>

#include "cmdlineoptions.h"
#include "configsettings/configsettings.h"
#include "utils/logging.h"
#include "utils/bleaddress.h"
#include "utils/unixsignalnotifier.h"
#include "utils/inputdevicemanager.h"
#include "utils/hidrawdevicemanager.h"
#include "utils/linux/linuxdevicenotifier.h"

#include "monitors/lescanmonitor.h"

#include "blercu/blercucontroller_p.h"
#include "blercu/bleservices/blercuservicesfactory.h"
#include "blercu/bluez/blercuadapter_p.h"
#include "blercu/btrmgradapter.h"

#if defined(ENABLE_BLERCU_CONN_PARAM_CHANGER)
#  include "bleconnparamchanger.h"
#endif

#include "irdb/irdatabase.h"

#include "services/servicemanager.h"

#if defined(ENABLE_IRPAIRING)
#  include "irpairing/irpairing.h"
#endif

#if QT_VERSION >= QT_VERSION_CHECK(5, 11, 0)
#if defined(ENABLE_PAIRING_SERVER) && (AI_BUILD_TYPE == AI_DEBUG)
#  include "debug/httpserver.h"
#endif
#endif

#include <signal.h>


#if !defined(AI_BUILD_TYPE) || !defined(AI_DEBUG) || !defined(AI_RELEASE)
#  error "AI logging macros not defined!"
#elif (AI_BUILD_TYPE != AI_DEBUG) && (AI_BUILD_TYPE != AI_RELEASE)
#  error "AI build type not set!"
#endif



// -----------------------------------------------------------------------------
/*!
	\internal

	Disables the SIGPIPE signal, the most annoying signal in the world.

 */
static void disableSigPipe()
{
	signal(SIGPIPE, SIG_IGN);
}

#if defined(ENABLE_BLERCU_CONN_PARAM_CHANGER)

// -----------------------------------------------------------------------------
/*!
	\internal

	Creates the \l{BleConnParamChanger} object and starts it with the set
	of desired connection parameters.

 */
static QSharedPointer<BleConnParamChanger> setupConnParamChanger(const QSharedPointer<CmdLineOptions> &options,
                                                                 const QSharedPointer<ConfigSettings> &config)
{
	// if a socket was supplied on the command line use that one in preference
	// to a newly created one
	QSharedPointer<HciSocket> hciSocket;
	int socketFd = options->takeHciSocket();
	if (socketFd >= 0)
		hciSocket = HciSocket::createFromSocket(socketFd, options->hciDeviceId());
	else
		hciSocket = HciSocket::create(options->hciDeviceId(), options->networkNamespace());

	if (!hciSocket || !hciSocket->isValid()) {
		qError("failed to setup hci socket to hci%u", options->hciDeviceId());
		return QSharedPointer<BleConnParamChanger>();
	}

	// create the connection parameters changer object
	QSharedPointer<BleConnParamChanger> connParamChanger =
		QSharedPointer<BleConnParamChanger>::create(hciSocket);
	if (!connParamChanger) {
		qError("failed to setup the BLE connection parameter changer");
		return QSharedPointer<BleConnParamChanger>();
	}

	// load the desired parameters based on device OUI
	const QList<ConfigModelSettings> modelSettings = config->modelSettings();
	for (const ConfigModelSettings &settings : modelSettings) {
		if (settings.hasBleConnParams()) {
			connParamChanger->setConnectionParamsFor(settings.oui(),
			                                         settings.bleConnParams());
		}
	}

	connParamChanger->start();

	return connParamChanger;
}

#endif // defined(ENABLE_BLERCU_CONN_PARAM_CHANGER)

// -----------------------------------------------------------------------------
/*!
	\internal

	Attempts to connect to the dbus with the given params.

 */
static QSharedPointer<QDBusConnection> setupDBus(CmdLineOptions::DBusType dbusType,
                                                 const QString &dbusAddress,
                                                 const QString &dbusServiceName)
{
	QSharedPointer<QDBusConnection> dbusConn;

	if (dbusType == CmdLineOptions::SystemBus)
		dbusConn = QSharedPointer<QDBusConnection>::create(QDBusConnection::systemBus());
	else if (dbusType == CmdLineOptions::SessionBus)
		dbusConn = QSharedPointer<QDBusConnection>::create(QDBusConnection::sessionBus());
	else if (dbusType == CmdLineOptions::CustomBus)
		dbusConn = QSharedPointer<QDBusConnection>::create(QDBusConnection::connectToBus(dbusAddress, dbusServiceName));
	else
		return QSharedPointer<QDBusConnection>();

	// check we managed to connect
	if (!dbusConn || !dbusConn->isConnected()) {
		const QDBusError error = dbusConn->lastError();
		qCritical("failed to connect to dbus, due to '%s'", qPrintable(error.name()));
		return QSharedPointer<QDBusConnection>();
	}

	// register ourselves on the bus
	if (!dbusConn->registerService(dbusServiceName)) {
		const QDBusError error = dbusConn->lastError();
		qCritical("failed to register service due to '%s'", qPrintable(error.name()));
		return QSharedPointer<QDBusConnection>();
	}

	return dbusConn;
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Creates the main \l{BleRcuController} object and returns a shared pointer
	to it.  This is just a wrapper around the boilerplate needed to create
	the various class objects needed by the controller.

 */
static QSharedPointer<BleRcuController> setupBleRcuController(const QSharedPointer<CmdLineOptions> &options,
                                                              const QSharedPointer<ConfigSettings> &config,
                                                              const QSharedPointer<IrDatabase> &irDatabase,
                                                              const QSharedPointer<QDBusConnection> &clientDBusConn,
                                                              const QSharedPointer<QDBusConnection> &debugDBusConn)
{
	// setup the linux device notifier (udev wrapper)
	QSharedPointer<LinuxDeviceNotifier> devNotifier =
		LinuxDeviceNotifier::create(LinuxDeviceNotifier::UDev, options->networkNamespace());
	if (!devNotifier) {
		qFatal("failed to setup the udev monitor");
	}

	// enable the device manager filters for both hidraw and (optionally) the
	// input device sub-systems
	devNotifier->addSubsystemMatchFilter(LinuxDevice::HidRawSubSystem);

	// create a 'hidraw' device manager to detect hidraw devices
	QSharedPointer<HidRawDeviceManager> hidrawDevManager =
		HidRawDeviceManager::create(devNotifier);
	if (!hidrawDevManager) {
		qFatal("failed to setup the hidraw device manager");
	}

	// create the factory for creating the BleRcu services for each device
	QSharedPointer<BleRcuServicesFactory> servicesFactory =
		QSharedPointer<BleRcuServicesFactory>::create(config,
		                                              irDatabase);
	if (!servicesFactory) {
		qFatal("failed to setup the BLE services factory");
	}

	// create the bluetooth adapter proxy
	QSharedPointer<BleRcuAdapter> adapter =
		QSharedPointer<BleRcuAdapterBluez>::create(config,
		                                           servicesFactory,
		                                           QDBusConnection::systemBus());
	if (!adapter || !adapter->isValid()) {
		qFatal("failed to setup the BLE manager");
	}

	// create the controller object
	QSharedPointer<BleRcuController> controller =
		QSharedPointer<BleRcuControllerImpl>::create(config, adapter);
	if (!controller || !controller->isValid()) {
		controller.reset();
		qFatal("failed to setup the BLE RCU controller");
	}

	return controller;
}

// -----------------------------------------------------------------------------
/*!


 */
int main(int argc, char *argv[])
{
	QCoreApplication app(argc, argv);
	QCoreApplication::setApplicationName("BleRcuDaemon");
	QCoreApplication::setApplicationVersion(BLUETOOTHRCU_VERSION);

	// disable SIGPIPE early
	disableSigPipe();


	// setup the logging very early (before command line parsing). On debug
	// builds we enable the console and EthanLog, plus warnings, errors, fatals
	// and milestones.  On release builds only enable EthanLog and only prodlog
	// messages by default, can be overridden by command line options.
#if (AI_BUILD_TYPE == AI_DEBUG)
	setupLogging(LoggingTarget::Console | LoggingTarget::EthanLog,
	             LoggingLevel::Fatal    | LoggingLevel::Error |
	             LoggingLevel::Warning  | LoggingLevel::Milestone);
#elif (AI_BUILD_TYPE == AI_RELEASE)
	setupLogging(LoggingTarget::EthanLog, 0);
#else
#	error "Unknown AI_BUILD_TYPE, expected AI_DEBUG or AI_RELEASE"
#endif


	// parse the command line options
	QSharedPointer<CmdLineOptions> options = QSharedPointer<CmdLineOptions>::create();
	options->process(app);


	// create the config options
	QSharedPointer<ConfigSettings> config = ConfigSettings::defaults();


	// connect to the bus used for exposing our services
	QSharedPointer<QDBusConnection> dbusConn = setupDBus(options->dbusType(),
	                                                     options->dbusAddress(),
	                                                     options->dbusServiceName());
	if (!dbusConn)
		return EXIT_FAILURE;

	// connect to the debug dbus
	QSharedPointer<QDBusConnection> debugDBusConn;
#if (AI_BUILD_TYPE == AI_DEBUG)
	if (options->debugDBusType() != CmdLineOptions::NoBus) {
		debugDBusConn = setupDBus(options->debugDBusType(),
		                          options->debugDBusAddress(),
		                          options->dbusServiceName());
	} else if (qEnvironmentVariableIsSet("DBUS_DEBUG_BUS_ADDRESS")) {
		debugDBusConn = setupDBus(CmdLineOptions::CustomBus,
		                          qgetenv("DBUS_DEBUG_BUS_ADDRESS"),
		                          QStringLiteral("com.sky.blercu.debug"));
	}
#endif // (AI_BUILD_TYPE == AI_DEBUG)


	// setup and start the BLE scan monitor, this is used for some limited prod
	// logging and we want to up and running before powering the BT interface
	QSharedPointer<LEScanMonitor> leScanMonitor;
	if (options->enableScanMonitor()) {
		leScanMonitor = QSharedPointer<LEScanMonitor>::create(options->hciDeviceId());
	}


#if defined(ENABLE_BLERCU_CONN_PARAM_CHANGER)
	// setup and start the connection parameters changer object, failure here
	// is not fatal for the daemon
	QSharedPointer<BleConnParamChanger> connParamChanger =
		setupConnParamChanger(options, config);
#endif // defined(ENABLE_BLERCU_CONN_PARAM_CHANGER)



	// first thing we do is start the service manager, it has no controller
	// so won't be useful, but means other clients can connect to it (the
	// controller is added later)
	QSharedPointer<ServiceManager> serviceManager =
		QSharedPointer<ServiceManager>::create(*dbusConn);

	serviceManager->registerAllServices();


	// create the IR database
	QSharedPointer<IrDatabase> irDatabase =
		IrDatabase::create(QStringLiteral(":/irdb.sqlite"));

	// initialize BTRMGR API before it is used in BleRcuController
	const auto btrMgrInitializer = BtrMgrAdapter::ApiInitializer{};

	// create the controller that manages the adapter and paired devices
	QSharedPointer<BleRcuController> controller =
		setupBleRcuController(options, config, irDatabase, dbusConn, debugDBusConn);


	// give the controller to the Android service, the service is now useful
	serviceManager->setController(controller);
	serviceManager->setIrDatabase(irDatabase);


#if defined(ENABLE_IRPAIRING)
	// enable the IR pairing object which will monitor the IR input device and
	// auto-enable pairing if the right IR signal is received
	QSharedPointer<IrPairing> irPairing =
		QSharedPointer<IrPairing>::create(controller);
#endif


    // (non-fusion builds) create and start the debug http/ws server
#if QT_VERSION >= QT_VERSION_CHECK(5, 11, 0)
#if defined(ENABLE_PAIRING_SERVER) && (AI_BUILD_TYPE == AI_DEBUG)

	QSharedPointer<HttpServer> pairingServer;
	if (options->enablePairingWebServer()) {
		pairingServer = QSharedPointer<HttpServer>::create(controller);
		pairingServer->listen();
	}

#endif // defined(ENABLE_PAIRING_SERVER) && (AI_BUILD_TYPE == AI_DEBUG)
#endif


	// destruct the command line options object (important as it closes any
	// file descriptors passed on the commandline).
	options.clear();


	// create a unix signal handler to capture the SIGTERM signal and do an
	// ordered clean up
	UnixSignalNotifier unixSignalNotifier(SIGTERM);
	QObject::connect(&unixSignalNotifier, &UnixSignalNotifier::activated,
	                 &QCoreApplication::quit);

#if (AI_BUILD_TYPE == AI_DEBUG)
	// create a ctrl-c handler for clean shutdown on debug builds, the handler
	// for this disables the signal handler then posts the quit - so that
	// a second ctrl-c will force a terminate
	UnixSignalNotifier ctrlcSignalNotifier(SIGINT);
	QObject::connect(&ctrlcSignalNotifier, &UnixSignalNotifier::activated,
	                 [&]() {
	                     qInfo("ctrl-c signal received, shutting down");
	                     ctrlcSignalNotifier.setEnabled(false);
	                     QCoreApplication::quit();
	});
#endif

	qMilestone("BleRcuDaemon started");

	// finally start the Qt event loop
	int exitCode = QCoreApplication::exec();

	qMilestone("BleRcuDaemon shutting down");

	return exitCode;
}

