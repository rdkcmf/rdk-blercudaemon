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
//  blercuasservice.cpp
//  SkyBluetoothRcu
//

#include "blercuasservice.h"
#include "blercustatuswebsocket.h"
#include "blercufwupgradeservice.h"
#include "blercu/blercucontroller.h"
#include "blercu/blercudevice.h"
#include "blercu/bleservices/blercuinfraredservice.h"
#include "irdb/irdatabase.h"

#if defined(Q_OS_LINUX)
#  include "dbus/asrequest.h"
#endif

#include "utils/logging.h"

#include <QMap>
#include <QSet>
#include <QFile>
#include <QString>
#include <QDebug>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>
#include <QDBusMetaType>



// -----------------------------------------------------------------------------
/*!
	\internal

	Utility to read all service .json file from the resources and return as a
	string.

 */
static QString configJson()
{
	QFile config(":/btremotes.json");
	if (!config.open(QFile::ReadOnly)) {
		qError("failed to open btremote.json file");
		return QString();
	}

	return config.readAll();
}



BleRcuASService::BleRcuASService(const QDBusConnection &dbusConn,
                                 QObject *parent)
	: ASService(dbusConn, QStringLiteral("com.sky.as.btremotes"), configJson(), parent)
	, m_asVersion(122)
	, m_dbusConn(dbusConn)
	, m_wsStatus(QSharedPointer<BleRcuStatusWebSocket>::create(m_asVersion))
{

	// connect to the signal telling us that the ws has more data
	QObject::connect(m_wsStatus.data(), &BleRcuStatusWebSocket::updateWebSocket,
	                 this, &BleRcuASService::onWebSocketUpdate);

	// set an initial empty f/w upgrade status
	QJsonObject fwStatus;
	fwStatus["remotes"] = QJsonArray();
	BleRcuASService::onFwUpgradeStatusChanged(fwStatus);
}

BleRcuASService::~BleRcuASService()
{
	m_wsStatus.reset();
}

// -----------------------------------------------------------------------------
/*!

 */
void BleRcuASService::setController(const QSharedPointer<BleRcuController> &controller)
{
	if (m_controller) {
		qError("already have a controller object, ignoring");
		return;
	}

	// sanity check the controller object
	if (!controller || !controller->isValid()) {
		qError("invalid controller object");
		return;
	}

	// store the controller object
	m_controller = controller;

	// pass the controller on to the ws status class
	m_wsStatus->setController(controller);

	// can now create the f/w upgrade service (non-prod builds only)
#if (AI_BUILD_TYPE == AI_DEBUG)
	if (!m_fwUpgrade) {
		m_fwUpgrade = QSharedPointer<BleRcuFwUpgradeService>::create(controller);

		QObject::connect(m_fwUpgrade.data(), &BleRcuFwUpgradeService::statusChanged,
		                 this, &BleRcuASService::onFwUpgradeStatusChanged);
	}
#endif
}

// -----------------------------------------------------------------------------
/*!

 */
void BleRcuASService::setIrDatabase(const QSharedPointer<IrDatabase> &irDatabase)
{
	m_irDatabase = irDatabase;
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Called to get the "capacitiveRCUMode" setting.

 */
QString BleRcuASService::getSystemSetting(const QString &name)
{
	if (name.compare("capacitiveRCUMode", Qt::CaseInsensitive) == 0) {
		return QStringLiteral("D-PAD");
	}

	return QString();
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Called to set the "capacitiveRCUMode" setting, this is a NOP on our platform.

 */
void BleRcuASService::setSystemSetting(const QString &name, const QString &value)
{
	qInfo("request to set the '%s' to '%s', ignored",
	      qPrintable(name), qPrintable(value));
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Called when the BleRcuStatusWebSocket object has detected a change that
	requires an update to the status websocket.

 */
void BleRcuASService::onWebSocketUpdate(const QJsonObject &message)
{
	static const QString wsUrl("/as/peripherals/btremotes/status");
	updateWebSocket(wsUrl, message);
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Called when the BleRcuFwUpgradeService object has detected a change in the
	f/w upgrade process for any RCU.

	This is only used on non-prod builds.

 */
void BleRcuASService::onFwUpgradeStatusChanged(const QJsonObject &message)
{
	static const QString wsUrl("/as/test/btremotes/fwupgrade/status");
	updateWebSocket(wsUrl, message);
}

// -----------------------------------------------------------------------------
/*!
    \internal

    Called to handle the AS request.

 */
void BleRcuASService::onRequest(const ASRequest &request)
{
	switch (request.method()) {
		case ASRequest::HttpGet:
			handleGetRequest(request);
			break;
		case ASRequest::HttpPost:
			handlePostRequest(request);
			break;

		default:
			qWarning("unknown request type");
			break;
	}
}

// -----------------------------------------------------------------------------
/*!
    \internal

    Called to handle the AS request.

 */
void BleRcuASService::handleGetRequest(const ASRequest &request)
{
	// check it's a player action request
	static const QString expectedUrlPrefix("/as/peripherals/btremotes/");
	if (!request.path().startsWith(expectedUrlPrefix)) {
		qWarning("url '%s' invalid or not supported", qPrintable(request.path()));
		request.sendErrorReply(ASRequest::InvalidUrlError);
		return;
	}

	// table of actions we support an their handlers
	using HandlerFunc = std::function<void(BleRcuASService*, const ASRequest&)>;
	static const QMap<QString, HandlerFunc> actionHandlers = {
		{ "edidinfo",               std::mem_fn(&BleRcuASService::onRequestEDIDInfo)        },
		{ "edidbasedcodes",         std::mem_fn(&BleRcuASService::onRequestEDIDBasedCodes)  },
		{ "ircodes/manualcodes",    std::mem_fn(&BleRcuASService::onRequestIRCodesManual)   },
		{ "ircodes/manufacturers",  std::mem_fn(&BleRcuASService::onRequestIRCodesManuf)    },
		{ "ircodes/models",         std::mem_fn(&BleRcuASService::onRequestIRCodesModels)   },
	};

	// get the last part of the path that contains the action
	const QString action = request.path().mid(expectedUrlPrefix.length());

	// get the handler for the action (if one)
	const HandlerFunc &handler = actionHandlers.value(action);
	if (handler) {
		// perform the action
		handler(this, request);

	} else {
		qWarning("no handler found for action '%s'", qPrintable(action));

		// unknown / unsupported action
		request.sendErrorReply(ASRequest::NotSupportedError);
	}
}

// -----------------------------------------------------------------------------
/*!
    \internal

    Called to handle the AS request.

 */
void BleRcuASService::handlePostRequest(const ASRequest &request)
{
	// check if a f/w upgrade request
	static const QString fwUpgradeUrlPrefix("/as/test/btremotes/fwupgrade/");
	if (request.path().startsWith(fwUpgradeUrlPrefix)) {
		if (m_fwUpgrade)
			m_fwUpgrade->handleRequest(request);
		else
			request.sendErrorReply(ASRequest::NotSupportedError);
		return;
	}

	// check it's a ircodes action request
	static const QString urlPrefix("/as/peripherals/btremotes/");
	if (!request.path().startsWith(urlPrefix)) {
		qWarning("url '%s' invalid or not supported", qPrintable(request.path()));
		request.sendErrorReply(ASRequest::InvalidUrlError);
		return;
	}

	// table of actions we support an their handlers
	using HandlerFunc = std::function<void(BleRcuASService*, const ASRequest&)>;
	static const QMap<QString, HandlerFunc> actionHandlers = {
		{ "action/startsearching",     std::mem_fn(&BleRcuASService::onRequestStartSearching)      },
		{ "ircodes/action/setcode",    std::mem_fn(&BleRcuASService::onRequestIRCodesSetCode)      },
		{ "ircodes/action/clear",      std::mem_fn(&BleRcuASService::onRequestIRCodesClearCode)    },
	};

	// get the last part of the path that contains the action
	const QString action = request.path().mid(urlPrefix.length());

	// get the handler for the action (if one)
	const HandlerFunc &handler = actionHandlers.value(action);
	if (handler) {
		// perform the action
		handler(this, request);

	} else {
		qWarning("no handler found for action '%s'", qPrintable(action));

		// unknown / unsupported action
		request.sendErrorReply(ASRequest::NotSupportedError);
	}

}

// -----------------------------------------------------------------------------
/*!
	\internal

	Called to process a POST request to the
	'/peripherals/btremotes/action/startsearching' URL.

 */
void BleRcuASService::onRequestStartSearching(const ASRequest &request)
{
	const ASRequest::QueryStringMap params = request.queryParams();
	qInfo() << "startsearching query params:"  << params;

	// get the timeout for the search if specified
	int timeoutMillisecs = -1;
	if (params.contains("timeout")) {

		const QString timeoutStr = params.value("timeout", "-1");

		bool ok = false;
		int timeoutSecs = timeoutStr.toInt(&ok, 10);
		if (!ok || (timeoutSecs <= 0)) {
			request.sendErrorReply(400, 102,
			                       "Invalid Parameters",
			                       "timeout parameter missing or invalid");
			return;
		}

		// ensure the timeout will fit into a millisecond int
		timeoutSecs = qMin(timeoutSecs, INT_MAX / 1000);
		timeoutMillisecs = timeoutSecs * 1000;
	}

	// start the scan and return the result
	if (m_controller->startScanning(timeoutMillisecs)) {
		request.sendReply(200);
	} else {
		request.sendErrorReply(400, 7570,
		                       "invalid state to invoke this action",
		                       "the pairing state must be IDLE,"
		                       " COMPLETE or FAILED in order to start a search");
	}
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Called to process a GET request to the '/peripherals/btremotes/edidinfo'
	URL.

 */
void BleRcuASService::onRequestEDIDInfo(const ASRequest &request)
{
	// TODO: implement once we have a source for the EDID

	request.sendErrorReply(ASRequest::NotSupportedError);
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Called to process a GET request to the '/peripherals/btremotes/edidbasedcodes'
	URL.

 */
void BleRcuASService::onRequestEDIDBasedCodes(const ASRequest &request)
{
	// TODO: implement once we have a source for the EDID

	request.sendErrorReply(ASRequest::NotSupportedError);
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Called to process a GET request to the '/peripherals/btremotes/ircodes/manualcodes'
	URL.

 */
void BleRcuASService::onRequestIRCodesManual(const ASRequest &request)
{
	// sanity check we have a database
	if (!m_irDatabase) {
		request.sendErrorReply(500, 7502, "database not available");
		return;
	}


	const ASRequest::QueryStringMap params = request.queryParams();
	qInfo() << "manualcodes query params:"  << params;

	// get the type of code to get
	IrDatabase::Type type;
	const QString requestedType = params.value("type");
	if (requestedType.compare("TV", Qt::CaseInsensitive) == 0)
		type = IrDatabase::Televisions;
	else if (requestedType.compare("AMP", Qt::CaseInsensitive) == 0)
		type = IrDatabase::AVAmplifiers;
	else {
		request.sendErrorReply(400, 7503, "type invalid (not AMP or TV)");
		return;
	}

	// get the manufacturer param
	const QString manufacturer = params.value("manufacturer");
	if (manufacturer.isEmpty()) {
		request.sendErrorReply(ASRequest::InvalidParametersError,
		                       QStringLiteral("Invalid 'manufacturer' param"));
		return;
	}

	// the model is optional
	QString model = params.value("model");

	// get the code list
	const QList<int> codes = m_irDatabase->codeIds(type, manufacturer, model);

	// form the json response
	QJsonObject root;
	root["type"] = requestedType;
	root["manufacturer"] = manufacturer;
	if (!model.isEmpty())
		root["model"] = model;

	QJsonArray jsonCodes;
	for (int code : codes)
		jsonCodes.append(code);
	root["codes"] = jsonCodes;

	// convert to a json string
	QJsonDocument json(root);
	request.sendReply(200, json.toJson());
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Called to process a GET request to the '/peripherals/btremotes/ircodes/manufacturers'
	URL.

 */
void BleRcuASService::onRequestIRCodesManuf(const ASRequest &request)
{
	// sanity check we have a database
	if (!m_irDatabase) {
		request.sendErrorReply(500, 7502, "database not available");
		return;
	}


	const ASRequest::QueryStringMap params = request.queryParams();
	qInfo() << "manufacturers query params:"  << params;

	// get the type of code to get
	IrDatabase::Type type;
	const QString requestedType = params.value("type");
	if (requestedType.compare("TV", Qt::CaseInsensitive) == 0)
		type = IrDatabase::Televisions;
	else if (requestedType.compare("AMP", Qt::CaseInsensitive) == 0)
		type = IrDatabase::AVAmplifiers;
	else {
		request.sendErrorReply(400, 7503, "type invalid (not AMP or TV)");
		return;
	}

	// get the manufacturer param
	const QString manufacturer = params.value("manufacturer");
	if (manufacturer.isEmpty()) {
		request.sendErrorReply(ASRequest::InvalidParametersError,
		                       QStringLiteral("Invalid 'manufacturer' param"));
		return;
	}

	// get the manufacturers list
	const QStringList manufacturers = m_irDatabase->brands(type, manufacturer);

	// form the json response
	QJsonObject root;
	root["type"] = requestedType;

	QJsonArray jsonManufacturers;
	for (const QString &manuf : manufacturers)
		jsonManufacturers.append(manuf);
	root["manufacturers"] = jsonManufacturers;

	// convert to a json string
	QJsonDocument json(root);
	request.sendReply(200, json.toJson());
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Called to process a GET request to the '/peripherals/btremotes/ircodes/models'
	URL.

 */
void BleRcuASService::onRequestIRCodesModels(const ASRequest &request)
{
	// sanity check we have a database
	if (!m_irDatabase) {
		request.sendErrorReply(500, 7502, "database not available");
		return;
	}

	const ASRequest::QueryStringMap params = request.queryParams();
	qInfo() << "models query params:"  << params;

	// get the type of code to get
	IrDatabase::Type type;
	const QString requestedType = params.value("type");
	if (requestedType.compare("TV", Qt::CaseInsensitive) == 0)
		type = IrDatabase::Televisions;
	else if (requestedType.compare("AMP", Qt::CaseInsensitive) == 0)
		type = IrDatabase::AVAmplifiers;
	else {
		request.sendErrorReply(400, 7503, "type invalid (not AMP or TV)");
		return;
	}

	// get the manufacturer param
	const QString manufacturer = params.value("manufacturer");
	if (manufacturer.isEmpty()) {
		request.sendErrorReply(ASRequest::InvalidParametersError,
		                       QStringLiteral("Invalid 'manufacturer' param"));
		return;
	}

	// the model, must not be empty
	QString search = params.value("model");
	if (search.isEmpty()) {
		request.sendErrorReply(ASRequest::InvalidParametersError,
		                       QStringLiteral("Invalid 'model' param"));
		return;
	}

	// get the model list, limit the number of responses to 100
	const QStringList models = m_irDatabase->models(type, manufacturer, search,
	                                                nullptr, 0, 100);

	// form the json response
	QJsonObject root;
	root["type"] = requestedType;
	root["manufacturer"] = manufacturer;

	QJsonArray jsonModels;
	for (const QString &model : models)
		jsonModels.append(model);
	root["models"] = jsonModels;

	// convert to a json string
	QJsonDocument json(root);
	request.sendReply(200, json.toJson());
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Called to process a GET request to the '/peripherals/btremotes/ircodes/action/setcode'
	URL.

 */
void BleRcuASService::onRequestIRCodesSetCode(const ASRequest &request)
{
	// sanity check we have the controller
	if (!m_controller) {
		request.sendErrorReply(ASRequest::GenericFailureError,
		                       QStringLiteral("Controller not available"));
		return;
	}

	const ASRequest::QueryStringMap params = request.queryParams();
	qInfo() << "setcode query params:"  << params;

	// get the bdaddr and parse it
	const BleAddress bdaddr(params.value("bdaddr"));
	if (bdaddr.isNull()) {
		request.sendErrorReply(ASRequest::InvalidParametersError,
		                       QStringLiteral("Invalid 'bdaddr' parameter"));
		return;
	}

	// get the type of code to get
	IrDatabase::Type type;
	const QString requestedType = params.value("type");
	if (requestedType.compare("TV", Qt::CaseInsensitive) == 0)
		type = IrDatabase::Televisions;
	else if (requestedType.compare("AMP", Qt::CaseInsensitive) == 0)
		type = IrDatabase::AVAmplifiers;
	else {
		request.sendErrorReply(400, 7503, "type invalid (not AMP or TV)");
		return;
	}

	// get the code id
	bool ok;
	int codeId;
	const QString codeStr = params.value("code");
	if (codeStr.isEmpty() || ((codeId = codeStr.toInt(&ok)) <= 0) || !ok) {
		request.sendErrorReply(ASRequest::InvalidParametersError,
		                       QStringLiteral("Invalid 'code' parameter"));
		return;
	}


	// try and find the device with the supplied bdaddr
	QSharedPointer<BleRcuDevice> device = m_controller->managedDevice(bdaddr);
	if (!device) {
		request.sendErrorReply(404, 7505, "bluetooth address is not found");
		return;
	}

	// get the infrared service for the device
	QSharedPointer<BleRcuInfraredService> irService = device->infraredService();
	if (!device->isReady() || !irService) {
		request.sendErrorReply(500, 7504, "remote not connected");
		return;
	}

	// create the set of keys to program
	QSet<Qt::Key> keyCodes = { Qt::Key_VolumeUp, Qt::Key_VolumeDown, Qt::Key_VolumeMute };
	if (type == IrDatabase::Televisions) {
		keyCodes += { Qt::Key_Standby, Qt::Key_Settings };
	}

	// finally program the actual signals
	Future<> result = irService->programIrSignals(codeId, keyCodes);

	// connect up lambdas to send back the success or failure result
	auto successLambda =
		[request]()
		{
			request.sendReply(200);
		};

	auto failureLambda =
		[request](const QString &errorName, const QString &errorMessage)
		{
			request.sendErrorReply(500, 7506, "unable to set code", errorMessage);
		};

	result.connectFinished(successLambda);
	result.connectErrored(failureLambda);
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Called to process a GET request to the '/peripherals/btremotes/ircodes/action/clear'
	URL.

 */
void BleRcuASService::onRequestIRCodesClearCode(const ASRequest &request)
{
	// sanity check we have the controller
	if (!m_controller) {
		request.sendErrorReply(ASRequest::GenericFailureError,
		                       QStringLiteral("Controller not available"));
		return;
	}

	const ASRequest::QueryStringMap params = request.queryParams();
	qInfo() << "clear query params:"  << params;

	// get the bdaddr and parse it
	const BleAddress bdaddr(params.value("bdaddr"));
	if (bdaddr.isNull()) {
		request.sendErrorReply(ASRequest::InvalidParametersError,
		                       QStringLiteral("Invalid 'bdaddr' parameter"));
		return;
	}

	// try and find the device with the supplied bdaddr
	QSharedPointer<BleRcuDevice> device = m_controller->managedDevice(bdaddr);
	if (!device) {
		request.sendErrorReply(404, 7505, "bluetooth address is not found");
		return;
	}

	// get the infrared service for the device
	QSharedPointer<BleRcuInfraredService> irService = device->infraredService();
	if (!device->isReady() || !irService) {
		request.sendErrorReply(500, 7504, "remote not connected");
		return;
	}

	// finally clear the actual signals
	Future<> result = irService->eraseIrSignals();

	// connect up lambdas to send back the success or failure result
	auto successLambda =
		[request]()
		{
			request.sendReply(200);
		};

	auto failureLambda =
		[request](const QString &errorName, const QString &errorMessage)
		{
			request.sendErrorReply(500, 7507, "unable to clear code", errorMessage);
		};

	result.connectFinished(successLambda);
	result.connectErrored(failureLambda);
}

