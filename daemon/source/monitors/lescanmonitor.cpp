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
//  lescanmonitor.cpp
//  BleRcuDaemon
//

#include "lescanmonitor.h"
#include "lescanmonitor_p.h"

#include "utils/logging.h"
#include "utils/linux/containerhelpers.h"

#include <QtEndian>

#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/uio.h>
#include <sys/poll.h>
#include <sys/eventfd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>


#ifndef AF_BLUETOOTH
#  define AF_BLUETOOTH      31
#endif

#define BTPROTO_L2CAP       0
#define BTPROTO_HCI         1
#define BTPROTO_RFCOMM      3

#define SOL_HCI             0
#define SOL_L2CAP           6
#define SOL_RFCOMM          18
#ifndef SOL_BLUETOOTH
#  define SOL_BLUETOOTH     274
#endif

// HCI sockopts
#define HCI_DATA_DIR        1
#define HCI_FILTER          2
#define HCI_TIME_STAMP      3

// HCI channels
#define HCI_CHANNEL_RAW     0
#define HCI_CHANNEL_MONITOR 2
#define HCI_CHANNEL_CONTROL 3

// HCI Packet types
#define HCI_COMMAND_PKT     0x01
#define HCI_ACLDATA_PKT     0x02
#define HCI_SCODATA_PKT     0x03
#define HCI_EVENT_PKT       0x04
#define HCI_VENDOR_PKT      0xff


struct sockaddr_hci {
	sa_family_t     hci_family;
	quint16         hci_dev;
	quint16         hci_channel;
};
#define HCI_DEV_NONE        0xffff

#define HCI_CHANNEL_RAW     0
#define HCI_CHANNEL_USER    1
#define HCI_CHANNEL_MONITOR 2
#define HCI_CHANNEL_CONTROL 3
#define HCI_CHANNEL_LOGGING 4

struct Q_PACKED hci_filter {
	quint32 type_mask;
	quint32 event_mask[2];
	quint16 opcode;
};

// HCI events
#define EVT_INQUIRY_COMPLETE                0x01
#define EVT_INQUIRY_RESULT                  0x02
#define EVT_CONN_COMPLETE                   0x03
#define EVT_CONN_REQUEST                    0x04
#define EVT_DISCONN_COMPLETE                0x05
#define EVT_AUTH_COMPLETE                   0x06
#define EVT_REMOTE_NAME_REQ_COMPLETE        0x07
#define EVT_ENCRYPT_CHANGE                  0x08
#define EVT_CHANGE_CONN_LINK_KEY_COMPLETE   0x09
#define EVT_MASTER_LINK_KEY_COMPLETE        0x0A
#define EVT_READ_REMOTE_FEATURES_COMPLETE   0x0B
#define EVT_READ_REMOTE_VERSION_COMPLETE    0x0C
#define EVT_QOS_SETUP_COMPLETE              0x0D

#define EVT_CMD_COMPLETE                    0x0E
struct Q_PACKED evt_cmd_complete{
	quint8      ncmd;
	quint16     opcode;
};

#define EVT_CMD_STATUS                      0x0F
struct Q_PACKED evt_cmd_status {
	quint8      status;
	quint8      ncmd;
	quint16     opcode;
};


// LE commands
#define OGF_LE_CTL                          0x08

#define OCF_LE_SET_ADVERTISE_ENABLE         0x000A

#define OCF_LE_SET_SCAN_PARAMETERS          0x000B
struct Q_PACKED le_set_scan_parameters {
	quint8  type;
	quint16 interval;
	quint16 window;
	quint8  own_bdaddr_type;
	quint8  filter;
};

#define OCF_LE_SET_SCAN_ENABLE              0x000C
struct Q_PACKED le_set_scan_enable {
	quint8  enable;
	quint8  filter_dup;
};



#define HCI_OPCODE(ogf, ocf) \
	quint16((quint16(ogf) << 10U) | (quint16(ocf) & 0x03ff))

#define HCI_OPCODE_OGF(op) \
	quint16(op >> 10)
#define HCI_OPCODE_OCF(op) \
	quint16(op & 0x03ff)


struct Q_PACKED hci_command_hdr {
	quint16 opcode;
	quint8  plen;
};
static_assert(sizeof(hci_command_hdr) == 3, "invalid hci_command_hdr packing");

struct Q_PACKED hci_event_hdr {
	quint8  evt;
	quint8  plen;
};
static_assert(sizeof(hci_event_hdr) == 2, "invalid hci_event_hdr packing");




// -----------------------------------------------------------------------------
/*!
	\class LEScanMonitor
	\brief Object that runs a monitor socket on the HCI interface to the HW.

	Unlike the other monitors in this directory, this monitor is expected to
	be running on a production build.  It is used to log significant events
	such as scan starting / stopping.

	The actual log messages are rate limited to avoid flooding the production
	logs with events if things start to get out of control.

 */



// -----------------------------------------------------------------------------
/*!
	Constructs the object which will open the HCI socket.  Use isValid() to
	determine if there was an error constructing the object.

	The \a deviceId is the number of the HCI device to open, this should be 0
	for Sky STBs as we only have one HCI device.  The \a netNsFd is the
	file descriptor to the root network namespace, if valid (>= 0) then the
	HCI monitor socket will be created in that namespace.

 */
LEScanMonitor::LEScanMonitor(uint deviceId, int netNsFd)
	: _d(nullptr)
{
	const int sockFlags = SOCK_RAW | SOCK_CLOEXEC | SOCK_NONBLOCK;

	// create the HCI socket, optionally in the supplied network namespace
	int sockFd = -1;
	if (netNsFd < 0)
		sockFd = socket(AF_BLUETOOTH, sockFlags, BTPROTO_HCI);
	else
		sockFd = createSocketInNs(netNsFd, AF_BLUETOOTH, sockFlags, BTPROTO_HCI);

	if (sockFd < 0) {
		qErrnoWarning(errno, "failed to create raw hci socket");
		return;
	}

	/*
	int opt = 1;
	if (setsockopt(sockFd, SOL_HCI, HCI_DATA_DIR, &opt, sizeof(opt)) < 0) {
		qErrnoWarning(errno, "failed to enable data direction info");
		close(sockFd);
		return;
	}

	opt = 1;
	if (setsockopt(sockFd, SOL_SOCKET, SO_PASSCRED, &opt, sizeof(opt)) < 0) {
		qErrnoWarning(errno, "failed to enable time stamping");
		close(sockFd);
		return;
	}
	*/

	// setup the hci filter so we only capture scan enable / disable commands
	// and the status of the command
	struct hci_filter filter;
	bzero(&filter, sizeof(struct hci_filter));

	filter.type_mask = (1UL << HCI_COMMAND_PKT) | (1UL << HCI_EVENT_PKT);
	filter.event_mask[0] = (1UL << EVT_CMD_COMPLETE) | (1UL << EVT_CMD_STATUS);
	filter.event_mask[1] = 0;
	filter.opcode = HCI_OPCODE(OGF_LE_CTL, OCF_LE_SET_SCAN_ENABLE);

	if (setsockopt(sockFd, SOL_HCI, HCI_FILTER, &filter, sizeof(filter)) < 0) {
		qErrnoWarning(errno, "failed to set hci filter");
		close(sockFd);
		return;
	}

	// bind socket to the HCI device
	struct sockaddr_hci addr;
	bzero(&addr, sizeof(struct sockaddr_hci));

	addr.hci_family = AF_BLUETOOTH;
	addr.hci_dev = deviceId;
	addr.hci_channel = HCI_CHANNEL_RAW;
	if (bind(sockFd, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
		qErrnoWarning(errno, "failed to bind to hci%u", deviceId);
		close(sockFd);
		return;
	}


	// finally create the private object which takes ownership of the socket
	_d = new LEScanMonitorPrivate(sockFd);

	// and then start it
	_d->start();
}

// -----------------------------------------------------------------------------
/*!
	Destructor, closes the internal hci socket and terminates the thread.

 */
LEScanMonitor::~LEScanMonitor()
{
	delete _d;
}

// -----------------------------------------------------------------------------
/*!
	Returns \c true if the monitor was created.

 */
bool LEScanMonitor::isValid() const
{
	return (_d != nullptr);
}




LEScanMonitorPrivate::LEScanMonitorPrivate(int btSocketFd, QObject *parent)
	: QThread(parent)
	, m_btSocketFd(btSocketFd)
	, m_deathFd(-1)
	, m_dataBuffer{ }
	, m_controlBuffer{ }
{

	// give this object a name, which in turn means the thread spawned will have
	// the same name
	setObjectName(QStringLiteral("LEScanMonitor"));

	// create the fd used to terminate the thread poll loop
	m_deathFd = eventfd(0, EFD_CLOEXEC);
	if (m_deathFd < 0) {
		qErrnoWarning(errno, "failed to create eventfd for thread notification");
		return;
	}

}

// -----------------------------------------------------------------------------
/*!
	Terminates the thread if running.  This may block for 5 seconds waiting for
	the thread to terminate.

 */
LEScanMonitorPrivate::~LEScanMonitorPrivate()
{
	if (isRunning()) {

		// signal the eventfd to terminate the thread
		if (m_deathFd >= 0) {
			uint64_t value = 1;
			if (TEMP_FAILURE_RETRY(write(m_deathFd, &value, sizeof(value))) != sizeof(value))
				qErrnoWarning(errno, "failed to write eventfd to wake thread");
		}

		// wait for the thread to finish
		if (!wait(5000)) {
			qError("timed-out waiting for monitor thread to finish");

			// forceful terminate, if we hit this something has probably gone
			// very wrong
			terminate();
			wait(100);
		}
	}


	// close the event fd used for killing the thread
	if ((m_deathFd >= 0) && (close(m_deathFd) != 0))
		qErrnoWarning(errno, "failed to close eventfd");

	// close the hci monitor socket
	if ((m_btSocketFd >= 0) && (close(m_btSocketFd) != 0))
		qErrnoWarning(errno, "failed to close hci socket");

	m_deathFd = m_btSocketFd = -1;
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Processes an HCI command packet.  Returns \c true if the packet was well
	formed, otherwise \c false.

 */
bool LEScanMonitorPrivate::processCommandPacket(const quint8 *data, ssize_t len) const
{
	static char scanParamsStr[128] = "?";

	auto *hdr = reinterpret_cast<const hci_command_hdr*>(data);
	data += sizeof(hci_command_hdr);
	len -= sizeof(hci_command_hdr);

	qInfo("received command msg opcode 0x%04x (ogf:0x%02x ocf:0x%04x)",
	      hdr->opcode, HCI_OPCODE_OGF(hdr->opcode), HCI_OPCODE_OCF(hdr->opcode));

	switch (hdr->opcode) {

		case HCI_OPCODE(OGF_LE_CTL, OCF_LE_SET_SCAN_PARAMETERS):
		{
			if (Q_UNLIKELY(len != sizeof(le_set_scan_parameters))) {
				qWarning("invalid LE scan params command packet");
				return false;
			}

			auto scanParams = reinterpret_cast<const le_set_scan_parameters *>(data);
			snprintf(scanParamsStr, sizeof(scanParamsStr),
			         "type:0x%02x time:0x%04x:0x%04x bdtype:0x%02x flt:0x%02x",
			         scanParams->type,
			         qFromLittleEndian(scanParams->interval),
			         qFromLittleEndian(scanParams->window),
			         scanParams->own_bdaddr_type,
			         scanParams->filter);

			return true;
		}

		case HCI_OPCODE(OGF_LE_CTL, OCF_LE_SET_SCAN_ENABLE):
		{
			if (Q_UNLIKELY(len != sizeof(le_set_scan_enable))) {
				qWarning("invalid LE scan enable command packet");
				return false;
			}

			auto scanEnable = reinterpret_cast<const le_set_scan_enable *>(data);
			qLimitedProdLog("HCI scan %sable request sent (en:0x%02x dup:0x%02x params={ %s })",
			                (scanEnable->enable == 0x00) ? "dis" : "en",
			                scanEnable->enable, scanEnable->filter_dup,
			                scanParamsStr);
			return true;
		}

		default:
			// ignore all other HCI commands
			return true;
	}
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Processes an HCI event packet.  Returns \c true if the packet was well
	formed, otherwise \c false.

 */
bool LEScanMonitorPrivate::processEventPacket(const quint8 *data, ssize_t len) const
{
	auto *hdr = reinterpret_cast<const hci_event_hdr*>(data);
	data += sizeof(hci_event_hdr);
	len -= sizeof(hci_event_hdr);

	if (hdr->evt == EVT_CMD_COMPLETE) {
		if (len < sizeof(evt_cmd_complete)) {
			qWarning("invalid size of EVT_CMD_COMPLETE packet");
			return false;
		}

		auto *evt = reinterpret_cast<const evt_cmd_complete*>(data);
		data += sizeof(evt_cmd_complete);
		len -= sizeof(evt_cmd_complete);

		qInfo("received command complete event for opcode 0x%04x (ogf:0x%02x ocf:0x%04x)",
		      evt->opcode, HCI_OPCODE_OGF(evt->opcode), HCI_OPCODE_OCF(evt->opcode));

		if (evt->opcode != HCI_OPCODE(OGF_LE_CTL, OCF_LE_SET_SCAN_ENABLE)) {
			qInfo("ignoring opcode 0x%04x", evt->opcode);
			return true;
		}
		if (len < 1) {
			qWarning("invalid size of EVT_CMD_COMPLETE, missing status byte");
			return false;
		}

		const uint8_t status = *data;
		if (status != 0x00) {
			qLimitedProdLog("HCI scan request failed (error 0x%02x)", status);
		}

		return true;

	} else if (hdr->evt == EVT_CMD_STATUS) {
		if (len < sizeof(evt_cmd_status)) {
			qWarning("invalid size of EVT_CMD_STATUS packet");
			return false;
		}

		auto *evt = reinterpret_cast<const evt_cmd_status*>(data);

		qInfo("received command status event for opcode 0x%04x (ogf:0x%02x ocf:0x%04x)",
		      evt->opcode, HCI_OPCODE_OGF(evt->opcode), HCI_OPCODE_OCF(evt->opcode));

		if (evt->opcode != HCI_OPCODE(OGF_LE_CTL, OCF_LE_SET_SCAN_ENABLE)) {
			qInfo("ignoring opcode 0x%04x", evt->opcode);
			return true;
		}

		qLimitedProdLog("HCI scan request status 0x%02x (%serror)",
		                evt->status, (evt->status == 0x00) ? "no " : "");

		return true;

	} else {
		qWarning("unexpected event type 0x%02x", hdr->evt);
		return false;
	}
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Reads a single HCI message from the socket, returns \c true if a packet
	was read.

	Returns \c false if there was an error reading the socket, otherwise
	\c true is returned (an empty read is not considered an error).

 */
bool LEScanMonitorPrivate::readHciPacket()
{
	static int unexpectedErrorCount = 0;

	// read a packet
	struct iovec iv;
	iv.iov_base = m_dataBuffer;
	iv.iov_len  = sizeof(m_dataBuffer);

	struct msghdr msg;
	msg.msg_iov = &iv;
	msg.msg_iovlen = 1;
	msg.msg_control = m_controlBuffer;
	msg.msg_controllen = sizeof(m_controlBuffer);

	ssize_t len = TEMP_FAILURE_RETRY(recvmsg(m_btSocketFd, &msg, MSG_DONTWAIT));
	if (Q_UNLIKELY(len == 0)) {
		qWarning("read an empty packet from the hci monitor socket");
		return (++unexpectedErrorCount < 10);

	} else if (len < 0) {
		if (errno != EAGAIN)
			qErrnoWarning(errno, "failed to receive hci message");

		return (errno == EAGAIN);
	}

	if (Q_UNLIKELY(len < 5)) {
		qWarning("received a too small hci packet (%zd bytes)", len);
		return (++unexpectedErrorCount < 10);
	}

	switch (m_dataBuffer[0]) {
		case HCI_COMMAND_PKT:
			return processCommandPacket(m_dataBuffer + 1, len - 1);

		case HCI_EVENT_PKT:
			return processEventPacket(m_dataBuffer + 1, len - 1);

		default:
			qWarning("received packet of unexpected type (0x%02x)", m_dataBuffer[0]);
			return (++unexpectedErrorCount < 10);
	}
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Thread function, this runs the poll loop and only exits when someone
	triggers the eventfd.

 */
void LEScanMonitorPrivate::run()
{
	qInfo("entering le scan monitor thread");

	if (Q_UNLIKELY(m_deathFd < 0))
		qWarning("missing death eventfd");


	// on linux we lower the priority of the thread to the minimum value
	int policy;
	struct sched_param param;
	if (pthread_getschedparam(pthread_self(), &policy, &param) == 0) {

		// check if an RT priority and if so drop this threads priority
		if ((policy == SCHED_RR) && (param.sched_priority > 1)) {
			param.sched_priority = 1;
			int ret = pthread_setschedparam(pthread_self(), policy, &param);
			if (ret != 0)
				qErrnoWarning(ret, "failed to set thread priority");
		}

	}


	// poll on both the death fd and the hci socket
	struct pollfd fds[2];

	fds[0].fd = m_deathFd;
	fds[0].events = POLLIN;
	fds[0].revents = 0;

	fds[1].fd = m_btSocketFd;
	fds[1].events = POLLIN;
	fds[1].revents = 0;

	while (true) {

		// wait for a message or death
		if (TEMP_FAILURE_RETRY(poll(fds, 2, -1)) < 0) {
			qErrnoWarning(errno, "odd, poll failed?");
			break;
		}

		// check for if the hci adaptor was removed
		if (Q_UNLIKELY(fds[1].revents & (POLLHUP | POLLERR | POLLNVAL))) {
			qWarning("hci adaptor disconnected the monitor socket");
			break;
		}

		// check for death eventfd
		if (Q_UNLIKELY(fds[0].revents & (POLLIN | POLLHUP | POLLERR | POLLNVAL))) {
			// in theory we should read the eventfd to clear it, but we know
			// it's only going to be closed once we return
			qInfo("quitting monitor event loop");
			break;
		}

		// check for an hci message
		if (fds[1].revents & POLLIN) {

			// read an HCI packet, if it fails exit the loop
			if (!readHciPacket()) {
				qWarning("hci socket read failed, quitting monitor event loop");
				break;
			}
		}

	}

	qInfo("exiting le scan monitor thread");
}
