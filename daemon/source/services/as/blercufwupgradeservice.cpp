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
//  blercufwupgradeservice.cpp
//  SkyBluetoothRcu
//

#include "blercufwupgradeservice.h"
#include "blercu/blercucontroller.h"
#include "blercu/blercudevice.h"
#include "blercu/bleservices/blercuupgradeservice.h"
#include "utils/logging.h"
#include "utils/fwimagefile.h"

#if defined(Q_OS_LINUX)
#  include "dbus/asrequest.h"
#endif

#include <QFile>
#include <QJsonDocument>

#include <unistd.h>

#if defined(Q_OS_ANDROID) || defined(RDK)
#  include <syscall.h>
#  if !defined(SYS_memfd_create)
#    if defined(__NR_memfd_create)
#      define SYS_memfd_create  __NR_memfd_create
#    elif defined(__arm__)
#      define SYS_memfd_create  385
#    endif
#  endif
#  if defined(Q_OS_ANDROID)
#    include <linux/memfd.h>
#  else
#    if !defined(MFD_CLOEXEC)
#      define MFD_CLOEXEC         0x0001U
#    endif
#    if !defined(MFD_ALLOW_SEALING)
#      define MFD_ALLOW_SEALING   0x0002U
#    endif
#  endif
#endif



// -----------------------------------------------------------------------------
/*!
	\class BleRcuFwUpgradeMonitor
	\brief Monitor object used to listener to f/w upgrade events for a single
	device.


 */
class BleRcuFwUpgradeMonitor : public QObject
{
	Q_OBJECT

public:
	explicit BleRcuFwUpgradeMonitor(const BleAddress &bdaddr,
	                                QObject *parent = nullptr)
		: m_bdaddr(bdaddr.toString())
		, m_state(Idle)
		, m_upgrading(false)
		, m_progress(0)
		, m_connected(true)
	{
	}

	// -------------------------------------------------------------------------
	/*!
		Returns the json object to return in the websocket
	 */
	QJsonObject details() const
	{
		static const QMap<State, QString> stateNames = {
			{ Idle,      "IDLE"       },
			{ Upgrading, "UPGRADING"  },
			{ Complete,  "COMPLETE"   },
			{ Failed,    "FAILED"     },
		};

		QJsonObject jsonObject;
		jsonObject["bdaddr"] = m_bdaddr.toString();
		jsonObject["state"] = stateNames.value(m_state);
		if (m_state == Upgrading)
			jsonObject["progress"] = m_progress;
		if (m_state == Failed)
			jsonObject["error"] = m_error;

		return jsonObject;
	}

signals:
	void updated();

public slots:
	// -------------------------------------------------------------------------
	/*!
		Signal handler for the upgrading state change from the f/w upgrade
		service.
	 */
	void onUpgradeChanged(bool upgrading)
	{
		if (m_upgrading == upgrading)
			return;

		qInfo("f/w upgrade %s", upgrading ? "started" : "stopped");

		m_upgrading = upgrading;
		switch (m_state) {
			case Idle:
			case Failed:
			case Complete:
				if (upgrading) {
					m_error.clear();
					m_state = Upgrading;
					m_progress = 0;
				}
				break;
			case Upgrading:
				if (!upgrading) {
					m_state = Complete;
					m_progress = 100;
				}
				break;
		}

		emit updated();
	};

	// -------------------------------------------------------------------------
	/*!
		Signal handler for the progress update from the f/w upgrade service.
	 */
	void onProgressChanged(int progress)
	{
		if (m_progress == progress)
			return;

		qDebug("f/w upgrade progress %d%%", progress);

		m_progress = progress;

		emit updated();
	};

	// -------------------------------------------------------------------------
	/*!
		Signal handler for the error event from the f/w upgrade service.
	 */
	void onError(const QString &message)
	{
		// workaround for UEI RCUs (EC102 and EC202), where they don't ack the
		// last upgrade packet and therefore the upgrade code reports a
		// failure (I should have pushed UEI to fix this)
		if (!m_connected && (m_progress >= 98) &&
		    ((m_bdaddr.oui() == 0x7091F3) || (m_bdaddr.oui() == 0xE80FC8))) {

			qWarning("ignoring f/w upgrade error '%s' on UEI RCU as reached "
			         "%d%% progress and then disconnected - assuming success",
			         qPrintable(message), m_progress);
			return;
		}

		m_error = message;
		m_state = Failed;

		qInfo("f/w upgrade error - %s", qPrintable(message));

		emit updated();
	}

	// -------------------------------------------------------------------------
	/*!
		Signal handler to listen for when the device has disconnected from the
		STB.  This is used to workaround an issue with the UEI RCUs that
		don't send the last ACK on the firmware upgrade, instead they just
		disconnect and start the upgrade.

	 */
	void onConnectedChanged(bool connected)
	{
		m_connected = connected;
	}

private:
	const BleAddress m_bdaddr;

	enum State { Idle, Upgrading, Complete, Failed } m_state;
	bool m_upgrading;
	int m_progress;
	QString m_error;
	bool m_connected;
};

#include "blercufwupgradeservice.moc"



// -----------------------------------------------------------------------------
/*!
	\class BleRcuFwUpgradeService
	\brief Implements the debug AS interface to upgrade the firmware on an RCU.

	This handles all the AS POST calls to the /as/test/btremotes/fwupgrade/xxx
	set of URL endpoints.  It also signals f/w upgrade status change
	notifications, which should be sent out the
	/as/test/btremotes/fwupgrade/status websocket.


 */


BleRcuFwUpgradeService::BleRcuFwUpgradeService(const QSharedPointer<BleRcuController> &controller,
	                                           QObject *parent)
	: QObject(parent)
	, m_controller(controller)
{
}

BleRcuFwUpgradeService::~BleRcuFwUpgradeService()
{
	for (auto &fwFile : m_uploadedFiles) {
		if ((fwFile.memFd >= 0) && (close(fwFile.memFd) != 0))
			qErrnoWarning(errno, "failed to close f/w memory file");
	}

	m_uploadedFiles.clear();

}

QJsonObject BleRcuFwUpgradeService::status() const
{
	QJsonObject json;
	json["remotes"] = m_remotes;
	return json;
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Called when a client has performed a POST request to one of the
	/as/test/btremotes/fwupgrade/xxx URL endpoints.

 */
void BleRcuFwUpgradeService::handleRequest(const ASRequest &request)
{
	qInfo("handling AS request '%s'", qPrintable(request.path()));

	// check it's a fwupgrade action request
	static const QString expectedUrlPrefix("/as/test/btremotes/fwupgrade/");
	if (!request.path().startsWith(expectedUrlPrefix)) {
		qWarning("url '%s' invalid or not supported", qPrintable(request.path()));
		request.sendErrorReply(ASRequest::InvalidUrlError);
		return;
	}

	// check it's a POST request
	if (request.method() != ASRequest::HttpPost) {
		qWarning("non-POST methods are not supported");
		request.sendErrorReply(ASRequest::NotSupportedError);
		return;
	}

	// table of actions we support an their handlers
	using HandlerFunc = std::function<void(BleRcuFwUpgradeService*, const ASRequest&)>;
	static const QMap<QString, HandlerFunc> actionHandlers = {
		{ "uploadfile/start",           std::mem_fn(&BleRcuFwUpgradeService::onUploadFileStart)     },
		{ "action/start",               std::mem_fn(&BleRcuFwUpgradeService::onStartFwUpgrade)      },
		{ "action/abort",               std::mem_fn(&BleRcuFwUpgradeService::onAbortFwUpgrade)      },
	};

	// get the last part of the path that contains the action
	const QString action = request.path().mid(expectedUrlPrefix.length());

	// get the handler for the action (if one)
	const HandlerFunc &handler = actionHandlers.value(action);
	if (handler) {

		// perform the action
		handler(this, request);
		return;
	}

	// check if one of the URLs that contains a UUID in the path
	const QStringList elements = action.split('/');
	if ((elements.count() == 3) && (elements[0] == "uploadfile")) {

		const QUuid uuid = QUuid::fromString(elements[1]);
		if (!uuid.isNull()) {

			if (elements[2] == "data") {
				onUploadFileData(uuid, request);
				return;
			}

			if (elements[2] == "delete") {
				onUploadFileDelete(uuid, request);
				return;
			}
		}
	}

	qWarning("no handler found for action '%s'", qPrintable(request.path()));

	// unknown / unsupported action
	request.sendErrorReply(ASRequest::NotSupportedError);
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Called when a client has POSTed to
	/as/test/btremotes/fwupgrade/uploadfile/start

 */
void BleRcuFwUpgradeService::onUploadFileStart(const ASRequest &request)
{
	// check if we have too many files and therefore which one to cull
	if (m_uploadedFiles.count() > 3) {
		qint64 oldestElapsed = 0;
		QUuid oldestUuid;

		QMap<QUuid, FwMemoryFile>::iterator it = m_uploadedFiles.begin();
		for (; it != m_uploadedFiles.end(); ++it) {
			if (it.value().created.elapsed() > oldestElapsed) {
				oldestElapsed = it.value().created.elapsed();
				oldestUuid = it.key();
			}
		}

		FwMemoryFile &oldest = m_uploadedFiles[oldestUuid];
		if ((oldest.memFd >= 0) && (close(oldest.memFd) != 0))
			qErrnoWarning(errno, "failed to close f/w memfd file");

		m_uploadedFiles.remove(oldestUuid);
	}

	// generate a new UUID and add a new f/w memory file to the map
	QUuid uuid = QUuid::createUuid();

	// create a new memfd for the file
	FwMemoryFile fwFile;

	char memName[64];
	snprintf(memName, sizeof(memName), "/fwfile-%s",
	         qPrintable(uuid.toString(QUuid::WithoutBraces)));

#if defined(Q_OS_ANDROID) || defined(RDK)
	fwFile.memFd = syscall(SYS_memfd_create, memName, MFD_CLOEXEC);
#else
	memFd = shm_open(memName, O_CLOEXEC | O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
	if ((memFd >= 0) && (shm_unlink(memName) != 0))
		qErrnoWarning(errno, "failed to unlink shm");
#endif

	if (fwFile.memFd < 0) {
		qErrnoWarning(errno, "failed to create memfd");
		request.sendErrorReply(ASRequest::GenericFailureError);
		return;
	}

	// store the new memfd
	m_uploadedFiles[uuid] = fwFile;

	// return the uuid in an json object to the caller
	QString jsonReply =
		QString("{ \"uuid\": \"%1\" }")
			.arg(uuid.toString(QUuid::WithoutBraces));

	request.sendReply(200, jsonReply);
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Called when a client has POSTed to
	/as/test/btremotes/fwupgrade/uploadfile/<uuid>/data

 */
void BleRcuFwUpgradeService::onUploadFileData(const QUuid &uuid,
	                                          const ASRequest &request)
{
	// check the uuid is valid
	if (!m_uploadedFiles.contains(uuid)) {
		request.sendErrorReply(ASRequest::InvalidUrlError,
		                       "Unknown uuid");
		return;
	}

	FwMemoryFile &fwFile = m_uploadedFiles[uuid];
	if (fwFile.memFd < 0) {
		request.sendErrorReply(ASRequest::GenericFailureError,
		                       "Internal error - invalid f/w memory file");
		return;
	}


	// sanity check the request body size
	const QString body = request.body();
	if (body.size() > (4 * 1024 * 1024)) {
		request.sendErrorReply(ASRequest::GenericFailureError,
		                       "POST body to large");
		return;
	}

	// parse the json body
	const QJsonDocument jsonBody = QJsonDocument::fromJson(request.body().toUtf8());
	if (!jsonBody.isObject()) {
		request.sendErrorReply(ASRequest::GenericFailureError,
		                       "Invalid JSON POST body");
		return;
	}

	const QJsonValueRef jsonFileChunkRef = jsonBody.object()["chunk"];
	if (!jsonFileChunkRef.isObject()) {
		request.sendErrorReply(ASRequest::GenericFailureError,
		                       "Missing JSON 'chunk' object");
		return;
	}

	QJsonObject jsonFileChunk = jsonFileChunkRef.toObject();
	const QJsonValueRef offset = jsonFileChunk["offset"];
	const QJsonValueRef data = jsonFileChunk["data"];
	if (!offset.isDouble() || !data.isString()) {
		request.sendErrorReply(ASRequest::GenericFailureError,
		                       "Missing JSON 'chunk' fields");
		return;
	}

	const int fileOffset = offset.toInt();
	const QByteArray fileData = QByteArray::fromBase64(data.toString().toUtf8());

	const size_t endPos = (fileOffset + fileData.size());

	// sanity check we're not trying to write at a giant offset
	if (endPos > (32 * 1024 * 1024)) {
		request.sendErrorReply(ASRequest::GenericFailureError,
		                       "File offset and / or data size to large");
		return;
	}


	// resize the file to include the new chunk if too small
	if (fwFile.size < endPos) {

		if (ftruncate(fwFile.memFd, endPos) != 0) {
			qErrnoWarning(errno, "failed to resize memfd to %zu", fwFile.size);
			request.sendErrorReply(ASRequest::GenericFailureError,
			                       "Internal error resizing memory f/w file");
			return;
		}

		fwFile.size = endPos;
	}

	// seek to the correct offset
	if (lseek(fwFile.memFd, fileOffset, SEEK_SET) != fileOffset) {
		qErrnoWarning(errno, "failed to seek to correct file position");
		request.sendErrorReply(ASRequest::GenericFailureError,
		                       "Internal error seeking within memory f/w file");
		return;
	}

	// write the data to the file
	if (TEMP_FAILURE_RETRY(write(fwFile.memFd, fileData.data(), fileData.size())) != fileData.size()) {
		qErrnoWarning(errno, "failed to write to temp f/w file");
		request.sendErrorReply(ASRequest::GenericFailureError,
		                       "Internal error writing to the memory f/w file");
		return;
	}

	qDebug("stored f/w file chunk at offset %d with size %zu", fileOffset, fileData.size());

	// send a success reply
	request.sendReply(200);
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Called when a client has POSTed to
	/as/test/btremotes/fwupgrade/uploadfile/<uuid>/delete

 */
void BleRcuFwUpgradeService::onUploadFileDelete(const QUuid &uuid,
	                                            const ASRequest &request)
{
	qInfo("received request to delete f/w file with uuid %s",
	      qPrintable(uuid.toString()));

	// check the uuid is valid
	auto it = m_uploadedFiles.find(uuid);
	if (it == m_uploadedFiles.end()) {
		request.sendErrorReply(ASRequest::InvalidUrlError,
		                       "Unknown uuid");
		return;
	}

	// close the memdfd
	FwMemoryFile &fwFile = it.value();
	if ((fwFile.memFd >= 0) && (close(fwFile.memFd) != 0))
		qErrnoWarning(errno, "failed to close memory f/w file");


	// remove from the map
	m_uploadedFiles.erase(it);

	qDebug("deleted temp f/w file with uuid '%s'", qPrintable(uuid.toString()));

	// send a success reply
	request.sendReply(200);
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Copies all the data from the supplied memfd into a new FwImageFile object,
	which will check it's integrity (CRC check) and if ok return a shared
	pointer to the object which can be used by the upgrade state machine.

 */
QSharedPointer<FwImageFile> BleRcuFwUpgradeService::copyFwMemoryFile(int memFd) const
{
	// sanity check
	if (memFd < 0) {
		qWarning("invalid memory f/w file");
		return nullptr;
	}

	// seek back to beginning of the source file
	if (lseek(memFd, 0, SEEK_SET) != 0) {
		qErrnoWarning(errno, "failed to seek?");
		return nullptr;
	}

	// wrap the fd in a QFile
	QFile fileWrapper;
	if (!fileWrapper.open(memFd, QFile::ReadOnly, QFile::DontCloseHandle)) {
		qWarning("failed to wrap memfd");
		return nullptr;
	}

	// read the entire memfd file
	const QByteArray fwFileData = fileWrapper.readAll();
	qInfo("copied f/w file of size %d", fwFileData.size());

	// write the data to f/w image file object which performs the integrity tests
	QSharedPointer<FwImageFile> fwImageFile = QSharedPointer<FwImageFile>::create(fwFileData);
	if (!fwImageFile || !fwImageFile->isValid()) {
		qWarning("invalid f/w file");
		fwImageFile.reset();
	}

	return fwImageFile;
}

// -------------------------------------------------------------------------
/*!
	\internal

	Installs lambda listeners on the \a result Future object, so that the
	when it completes a reply (either success or error) is sent back over the
	\a request object.

 */
void BleRcuFwUpgradeService::connectFutureToServiceReply(const ASRequest &request,
                                                         const Future<void> &result) const
{
	// connect up lambdas to send back the success or failure result
	auto successLambda =
		[request]()
		{
			request.sendReply(200);
		};

	auto failureLambda =
		[request](const QString &errorName, const QString &errorMessage)
		{
			request.sendErrorReply(ASRequest::GenericFailureError, errorMessage);
		};


	// check if the result is already available and if so just send a
	// reply, don't bother with attaching to the result signals
	if (result.isFinished()) {

		if (result.isError())
			failureLambda(result.errorName(), result.errorMessage());
		else
			successLambda();

	} else {

		// connect the lambda's to the finished and error signals
		result.connectFinished(this, successLambda);
		result.connectErrored(this, failureLambda);
	}
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Called when a client has POSTed to /as/test/btremotes/fwupgrade/action/start

 */
void BleRcuFwUpgradeService::onStartFwUpgrade(const ASRequest &request)
{
	// there are two mandatory query params, one to tell us which device to
	// program, and the second which f/w file uuid to use
	const ASRequest::QueryStringMap params = request.queryParams();
	qInfo() << "start f/w upgrade query params:"  << params;


	// get the f/w file uuid
	const QUuid fwFileUuid = QUuid::fromString(params.value("fwfileuuid"));
	if (fwFileUuid.isNull()) {
		request.sendErrorReply(ASRequest::InvalidParametersError,
		                       "Invalid uuid parameter");
		return;
	}

	if (!m_uploadedFiles.contains(fwFileUuid)) {
		request.sendErrorReply(ASRequest::InvalidParametersError,
		                       "No uploaded file with given UUID");
		return;
	}

	// load and check the f/w file (performs crc check)
	QSharedPointer<FwImageFile> fwImage = copyFwMemoryFile(m_uploadedFiles[fwFileUuid].memFd);
	if (!fwImage) {
		request.sendErrorReply(ASRequest::GenericFailureError,
		                       "Uploaded file is incomplete and/or corrupt");
		return;
	}


	// get the rcu bdaddr
	const BleAddress bdaddr(params.value("bdaddr"));
	if (bdaddr.isNull()) {
		request.sendErrorReply(ASRequest::InvalidParametersError,
		                       "Invalid bdaddr parameter");
		return;
	}

	// get the device
	QSharedPointer<BleRcuDevice> device = m_controller->managedDevice(bdaddr);
	if (!device) {
		request.sendErrorReply(ASRequest::GenericFailureError,
		                       "Unknown device with given bdaddr");
		return;
	}

	// get the upgrade service, if doesn't exist then f/w upgrade is not
	// supported on this device
	const QSharedPointer<BleRcuUpgradeService> service = device->upgradeService();
	if (!service) {
		request.sendErrorReply(ASRequest::NotSupportedError,
		                       "Upgrade not supported on this device");
		return;
	}

	// sanity check an upgrade is not already in progress
	if (service->upgrading()) {
		request.sendErrorReply(ASRequest::GenericFailureError,
		                       "Upgrade already in progress");
		return;
	}


	// add an upgrade monitor if we don't already have one
	if (!m_deviceMonitors.contains(bdaddr)) {
		QSharedPointer<BleRcuFwUpgradeMonitor> monitor =
			QSharedPointer<BleRcuFwUpgradeMonitor>::create(bdaddr);

		QObject::connect(monitor.data(), &BleRcuFwUpgradeMonitor::updated,
		                 this, &BleRcuFwUpgradeService::updateWebSocket,
		                 Qt::QueuedConnection);

		m_deviceMonitors[bdaddr] = monitor;
	}

	// install listeners on the upgrade service for the websocket status
	QSharedPointer<BleRcuFwUpgradeMonitor> monitor = m_deviceMonitors[bdaddr];

	QObject::connect(device.data(), &BleRcuDevice::connectedChanged,
	                 monitor.data(), &BleRcuFwUpgradeMonitor::onConnectedChanged,
	                 Qt::UniqueConnection);

	QObject::connect(service.data(), &BleRcuUpgradeService::upgradingChanged,
	                 monitor.data(), &BleRcuFwUpgradeMonitor::onUpgradeChanged,
	                 Qt::UniqueConnection);
	QObject::connect(service.data(), &BleRcuUpgradeService::progressChanged,
	                 monitor.data(), &BleRcuFwUpgradeMonitor::onProgressChanged,
	                 Qt::UniqueConnection);
	QObject::connect(service.data(), &BleRcuUpgradeService::error,
	                 monitor.data(), &BleRcuFwUpgradeMonitor::onError,
	                 Qt::UniqueConnection);




	// ask the service to start the upgrade and connect the future result to
	// the service request
	Future<> results = service->startUpgrade(fwImage);
	connectFutureToServiceReply(request, results);
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Called when a client has POSTed to /as/test/btremotes/fwupgrade/action/start

 */
void BleRcuFwUpgradeService::onAbortFwUpgrade(const ASRequest &request)
{
	const ASRequest::QueryStringMap params = request.queryParams();
	qInfo() << "abort f/w upgrade query params:"  << params;

	// get the rcu bdaddr
	const BleAddress bdaddr(params.value("bdaddr"));
	if (bdaddr.isNull()) {
		request.sendErrorReply(ASRequest::InvalidParametersError,
		                       "Invalid bdaddr value");
		return;
	}

	// get the device
	QSharedPointer<BleRcuDevice> device = m_controller->managedDevice(bdaddr);
	if (!device) {
		request.sendErrorReply(ASRequest::GenericFailureError,
		                       "Unknown device with given bdaddr");
		return;
	}

	// get the upgrade service, if doesn't exist then f/w upgrade is not
	// supported on this device
	const QSharedPointer<BleRcuUpgradeService> service = device->upgradeService();
	if (!service) {
		request.sendErrorReply(ASRequest::NotSupportedError,
		                       "Upgrade not supported on this device");
		return;
	}

	// ask the service to start the upgrade and connect the future result to
	// the service request
	Future<> results = service->cancelUpgrade();
	connectFutureToServiceReply(request, results);
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Internal queued event callback that indicates th state or progress of a
	running download has changed.

 */
void BleRcuFwUpgradeService::updateWebSocket()
{
	QJsonArray remotes;
	for (const auto &deviceMonitor : m_deviceMonitors)
		remotes.append(deviceMonitor->details());

	QJsonObject status;
	status["remotes"] = remotes;

	emit statusChanged(status);
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Called when a BLE RCU device has been removed.  Which happens when another
	RCU has been paired and we've forcefully un-paired a device.

 */
void BleRcuFwUpgradeService::onDeviceRemoved(const BleAddress &address)
{
	// remove the monitor
	m_deviceMonitors.remove(address);

	// schedule a ws update
	QMetaObject::invokeMethod(this, &BleRcuFwUpgradeService::updateWebSocket,
	                          Qt::QueuedConnection);
}

