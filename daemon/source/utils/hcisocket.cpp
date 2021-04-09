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
//  hcisocket.cpp
//  BleRcuDaemon
//

#include "hcisocket_p.h"
#include "logging.h"
#include "linux/containerhelpers.h"

#include <QDebug>
#include <QtEndian>

#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/uio.h>
#include <sys/ioctl.h>
#include <sys/socket.h>


#ifndef SOCK_CLOEXEC
#  define SOCK_CLOEXEC      0
#endif

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
#define HCI_CHANNEL_RAW		0
#define HCI_CHANNEL_MONITOR	2
#define HCI_CHANNEL_CONTROL	3

// HCI data types
#define HCI_COMMAND_PKT     0x01
#define HCI_ACLDATA_PKT     0x02
#define HCI_SCODATA_PKT     0x03
#define HCI_EVENT_PKT       0x04
#define HCI_VENDOR_PKT      0xff

#define HCI_FLT_TYPE_BITS   31
#define HCI_FLT_EVENT_BITS  63

#define HCI_MAX_EVENT_SIZE  260


// HCI ioctls
#define HCIGETDEVLIST	_IOR('H', 210, int)
#define HCIGETDEVINFO	_IOR('H', 211, int)
#define HCIGETCONNLIST	_IOR('H', 212, int)
#define HCIGETCONNINFO	_IOR('H', 213, int)


struct hci_filter {
	quint32 type_mask;
	quint32 event_mask[2];
	quint16 opcode;
};

struct sockaddr_hci {
	sa_family_t hci_family;
	unsigned short hci_dev;
	unsigned short hci_channel;
};

//  HCI Packet structure
#define HCI_TYPE_LEN	1

typedef struct {
	quint16     opcode;		// OCF & OGF
	quint8      plen;
} __attribute__ ((packed))	hci_command_hdr;
#define HCI_COMMAND_HDR_SIZE 	3

struct __attribute__ ((packed)) hci_event_hdr {
	quint8      evt;
	quint8      plen;
};
#define HCI_EVENT_HDR_SIZE 	2


// Bluetooth address
typedef struct {
	quint8 b[6];
} __attribute__((packed)) bdaddr_t;


// HCI connection info
struct hci_conn_info {
	quint16  handle;
	bdaddr_t bdaddr;
	quint8   type;
	quint8	 out;
	quint16  state;
	quint32  link_mode;
};

#define SCO_LINK	0x00
#define ACL_LINK	0x01
#define ESCO_LINK	0x02
#define LE_LINK		0x80
#define AMP_LINK	0x81


struct hci_conn_info_req {
	bdaddr_t bdaddr;
	quint8   type;
	struct hci_conn_info conn_info[0];
};

struct hci_conn_list_req {
    quint16 dev_id;
    quint16 conn_num;
    struct hci_conn_info conn_info[0];
};




#define EVT_DISCONN_COMPLETE    0x05
struct __attribute__ ((packed)) evt_disconn_complete {
	quint8      status;
	quint16     handle;
	quint8      reason;
};
#define EVT_DISCONN_COMPLETE_SIZE 4

// BLE Meta Event
#define EVT_LE_META_EVENT       0x3E
struct __attribute__ ((packed)) evt_le_meta_event {
	quint8      subevent;
	quint8      data[0];
};
#define EVT_LE_META_EVENT_SIZE 1

// BLE Meta Event - connection complete
#define EVT_LE_CONN_COMPLETE	0x01
struct __attribute__ ((packed)) evt_le_connection_complete {
	quint8      status;
	quint16     handle;
	quint8      role;
	quint8      peer_bdaddr_type;
	bdaddr_t    peer_bdaddr;
	quint16     interval;
	quint16     latency;
	quint16     supervision_timeout;
	quint8      master_clock_accuracy;
};
#define EVT_LE_CONN_COMPLETE_SIZE 18

// BLE Meta Event - update complete
#define EVT_LE_CONN_UPDATE_COMPLETE	0x03
struct __attribute__ ((packed)) evt_le_connection_update_complete {
	quint8      status;
	quint16     handle;
	quint16     interval;
	quint16     latency;
	quint16     supervision_timeout;
};
#define EVT_LE_CONN_UPDATE_COMPLETE_SIZE 9


// LE commands
#define OGF_LE_CTL		0x08

#define OCF_LE_CONN_UPDATE			0x0013
struct __attribute__ ((packed)) le_connection_update_cp {
	quint16     handle;
	quint16     min_interval;
	quint16     max_interval;
	quint16     latency;
	quint16     supervision_timeout;
	quint16     min_ce_length;
	quint16     max_ce_length;
};
#define LE_CONN_UPDATE_CP_SIZE 14


//HCI_VS_LE_SET_MORE_DATA_CAP_CMD_CODE
#define OGF_LE_VSC				0x3F
#define OCF_LE_MORE_DATA		0x01B3
#define LE_MORE_DATA_VSC_SIZE	3
//More Data Capability, 0x05~0x0A: 0x05 means 50% capability, 0x0A means 100% 
#define OCF_LE_MORE_DATA_CAPABILITY_LEVEL	0x0A
struct __attribute__ ((packed)) le_vsc_more_data_capability {
	quint16     handle;
	quint8      level;
};

// sanity checks for struct packing
Q_STATIC_ASSERT(sizeof(hci_command_hdr) == HCI_COMMAND_HDR_SIZE);
Q_STATIC_ASSERT(sizeof(hci_event_hdr) == HCI_EVENT_HDR_SIZE);

Q_STATIC_ASSERT(sizeof(evt_disconn_complete) == EVT_DISCONN_COMPLETE_SIZE);
Q_STATIC_ASSERT(sizeof(evt_le_meta_event) == EVT_LE_META_EVENT_SIZE);
Q_STATIC_ASSERT(sizeof(evt_le_connection_complete) == EVT_LE_CONN_COMPLETE_SIZE);
Q_STATIC_ASSERT(sizeof(evt_le_connection_update_complete) == EVT_LE_CONN_UPDATE_COMPLETE_SIZE);

Q_STATIC_ASSERT(sizeof(le_connection_update_cp) == LE_CONN_UPDATE_CP_SIZE);




// -----------------------------------------------------------------------------
/*!
	Debugging function to print the details of a connected device.

 */
QDebug operator<<(QDebug dbg, const HciSocket::ConnectedDeviceInfo &info)
{
	QDebugStateSaver saver(dbg);

	dbg.nospace() << "ConnectedDeviceInfo(" << info.address
	              << ", handle=" << info.handle
	              << ", state=" << info.state
	              << ")";

	return dbg;
}


// -----------------------------------------------------------------------------
/*!
	\class HciSocket
	\brief Wraps a bluetooth HCI socket to provide limited event notifications
	and command executation.

	This object is not intended to be a fully featured interface to the kernel's
	bluetooth HCI driver, rather it is specifically targeted for bluetooth low
	energy devices and then only the basic events and only one command.


	\warning To get all target events from the socket the process needs the
	\c CAP_NET_RAW capability or root privilage.  In addition the hci driver
	in the kernel hasn't been converted over to using user namespaces and hence
	setting \c CAP_NET_RAW will not work in a userns. The code in question is
	the following in the kernel

	\code
		if (!capable(CAP_NET_RAW)) {
			uf.type_mask &= hci_sec_filter.type_mask;
			uf.event_mask[0] &= *((u32 *) hci_sec_filter.event_mask + 0);
			uf.event_mask[1] &= *((u32 *) hci_sec_filter.event_mask + 1);
		}
	\endcode

	The issue is that the \c capable(...) call uses the initial user namespace
	of the container rather than the current. Instead it should be using
	\c capable_ns(...).  See https://github.com/moby/moby/issues/25622 for
	details on the issue.


	\warning The kernel's HCI driver hasn't been updated to work inside a
	container with a network namespace, the following kernel code means that
	attempting to create the socket will always return with \c EAFNOSUPPORT

	\code
		static int bt_sock_create(struct net *net, struct socket *sock, int proto,
					  int kern)
		{
			int err;

			if (net != &init_net)
				return -EAFNOSUPPORT;

			...
	\endcode

 */

// -----------------------------------------------------------------------------
/*!
	\fn HciSocket::connectionCompleted(quint16 handle, const BleAddress &device, const BleConnectionParameters &params);

	This signal is emitted when an BLE Connection Complete Event is received
	from the driver.  The \a handle is the unique handle used to identify the
	new connection and \a device is the BDADDR of the remote device that has
	just been connected to.

	\a params are the connection parameters that are currently being used for
	the connection.  Note that the BleConnectionParameters::minInterval() and
	BleConnectionParameters::maxInterval() of \a params will be the same and
	refer to the current interval used for the connection.

	\see Section 7.7.65.1 LE Connection Complete Event of Volumne 2 part E of
	the Bluetooth Core Spec 4.0
 */


// -----------------------------------------------------------------------------
/*!
	Constructs a new HciSocket object that is open and connected to the HCI
	device.  The socket will be bound to the hci device with the given
	\a deviceId, this should typically be 0 for the \c hci0 device.

	\a netNsFd refers to the network namespace in which to create the socket, if
	it is less than 0 then the current network namespace will be used.

	If a failure occurs an empty shared pointer is returned.
 */
QSharedPointer<HciSocket> HciSocket::create(uint deviceId, int netNsFd, QObject *parent)
{
	qInfo("creating new socket for HciSocket object");

	// create the HCI socket, optionally in the supplied network namespace
	int sockFd = -1;
	if (netNsFd < 0)
		sockFd = socket(AF_BLUETOOTH, (SOCK_RAW | SOCK_CLOEXEC), BTPROTO_HCI);
	else
		sockFd = createSocketInNs(netNsFd, AF_BLUETOOTH, (SOCK_RAW | SOCK_CLOEXEC),
		                          BTPROTO_HCI);

	if (sockFd < 0) {
		qErrnoWarning(errno, "failed to create hci socket");
		return QSharedPointer<HciSocketImpl>();
	}

	//
	QSharedPointer<HciSocketImpl> hciSocket =
		QSharedPointer<HciSocketImpl>::create(sockFd, deviceId, parent);

	if (!hciSocket->isValid())
		hciSocket.reset();

	return hciSocket;
}

// -----------------------------------------------------------------------------
/*!
	Constructs a new HciSocket object wrapping an existing \a socketFd.

	This method takes ownership of \a socketFd, it will be stored within the
	object and closed when the object is destroyed.

	It is expected the socket is opened with the following arguments:
	\code
		socket(AF_BLUETOOTH, (SOCK_RAW | SOCK_CLOEXEC), BTPROTO_HCI);
	\endcode

	This function is provided so that an HCI socket can be passed in from
	the host if running inside a container.

	If a failure occurs an empty shared pointer is returned.
 */
QSharedPointer<HciSocket> HciSocket::createFromSocket(int socketFd,
                                                      uint deviceId,
                                                      QObject *parent)
{
	qInfo("wrapping socket %d with HciSocket object", socketFd);

	QSharedPointer<HciSocketImpl> hciSocket =
		QSharedPointer<HciSocketImpl>::create(socketFd, deviceId, parent);

	if (!hciSocket->isValid())
		hciSocket.reset();

	return hciSocket;
}


HciSocketImpl::HciSocketImpl(int socketFd, uint hciDeviceId, QObject *parent)
	: HciSocket(parent)
	, m_hciDeviceId(hciDeviceId)
	, m_hciSocket(-1)
	, m_notifier(nullptr)
{
	// setup the hci socket
	if (!setSocketFilter(socketFd) || !bindSocket(socketFd, hciDeviceId)) {
		close(socketFd);
		return;
	}

	m_hciSocket = socketFd;

	// install a notifier for events from the socket
	m_notifier = new QSocketNotifier(m_hciSocket, QSocketNotifier::Read, this);
	QObject::connect(m_notifier, &QSocketNotifier::activated,
	                 this, &HciSocketImpl::onSocketActivated);

	m_notifier->setEnabled(true);
}

HciSocketImpl::~HciSocketImpl()
{
	if (m_notifier)
		m_notifier->setEnabled(false);

	if ((m_hciSocket >= 0) && (::close(m_hciSocket) != 0))
		qErrnoWarning(errno, "failed to close hci socket");
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Sets the HCI filter so we get only the events we care about.

 */
bool HciSocketImpl::setSocketFilter(int socketFd) const
{
	const quint32 filterTypeMask = (1UL << HCI_EVENT_PKT);
	const quint32 filterEvenMask[2] = { (1UL << EVT_DISCONN_COMPLETE),
	                                    (1UL << (EVT_LE_META_EVENT - 32)) };


	// get the filter first in case we don't need to change it
	struct hci_filter filter;
	bzero(&filter, sizeof(filter));
	socklen_t filterLen = sizeof(filter);

	if (getsockopt(socketFd, SOL_HCI, HCI_FILTER, &filter, &filterLen) < 0) {
		qErrnoWarning(errno, "failed to set hci socket filter");

	} else if (filterLen != sizeof(filter)) {
		qWarning("returned filter is not the expected size");

	} else if ( ((filter.type_mask & filterTypeMask) == filterTypeMask) &&
	            ((filter.event_mask[0] & filterEvenMask[0]) == filterEvenMask[0]) &&
	            ((filter.event_mask[1] & filterEvenMask[1]) == filterEvenMask[1]) ) {
		qInfo("hci filter already matches, no need to reset");
		return true;
	}

	qDebug("hci filter was [ type=0x%04x events={0x%08x, 0x%08x} ]",
	       filter.type_mask, filter.event_mask[0], filter.event_mask[1]);

	qInfo("setting hci filter to [ type=0x%04x events={0x%08x, 0x%08x} ]",
	      filterTypeMask, filterEvenMask[0], filterEvenMask[1]);

	// setup filter for only receiving BLE meta events
	bzero(&filter, sizeof(filter));
	filter.type_mask = filterTypeMask;
	filter.event_mask[0] = filterEvenMask[0];
	filter.event_mask[1] = filterEvenMask[1];

	if (setsockopt(socketFd, SOL_HCI, HCI_FILTER, &filter, sizeof(filter)) < 0) {
		qErrnoWarning(errno, "failed to set hci socket filter");
		return false;
	}

	return true;
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Binds the HCI socket to the given hci device.

 */
bool HciSocketImpl::bindSocket(int socketFd, uint hciDeviceId) const
{
	// bind socket to the HCI device
	struct sockaddr_hci addr;
	bzero(&addr, sizeof(addr));
	addr.hci_family = AF_BLUETOOTH;
	addr.hci_dev = hciDeviceId;
	addr.hci_channel = HCI_CHANNEL_RAW;

	if (bind(socketFd, (struct sockaddr *) &addr, sizeof(addr)) < 0) {

		// EALREADY is sent if we've already bound the socket, ignore this error
		if (errno != EALREADY) {
			qErrnoWarning(errno, "failed to bind to hci socket");
			return false;
		}
	}

	return true;
}

// -----------------------------------------------------------------------------
/*!
	\fn bool HciSocket::isValid() const

	Returns \c true if the socket was successifully opened.
 */
bool HciSocketImpl::isValid() const
{
	return (m_hciSocket >= 0);
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Sends a command to the HCI device.
 */
bool HciSocketImpl::sendCommand(quint16 ogf, quint16 ocf, void *data, quint8 dataLen)
{
	struct iovec vec[3];
	int vecLen = 0;

	quint8 type = HCI_COMMAND_PKT;
	vec[0].iov_base = &type;
	vec[0].iov_len = 1;
	vecLen++;

	hci_command_hdr hdr;
	hdr.opcode = qToLittleEndian<quint16>((ocf & 0x03ff) | (ogf << 10));
	hdr.plen = dataLen;
	vec[1].iov_base = &hdr;
	vec[1].iov_len = HCI_COMMAND_HDR_SIZE;
	vecLen++;

	if (data && dataLen) {
		vec[2].iov_base = data;
		vec[2].iov_len  = dataLen;
		vecLen++;
	}

	ssize_t wr = TEMP_FAILURE_RETRY(::writev(m_hciSocket, vec, vecLen));
	if (wr < 0) {
		qErrnoWarning(errno, "failed to write command");
		return false;
	}

	return true;
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Checks that the supplied connection parameters are valid, this is borrowed
	from the kernel checker code.
 */
bool HciSocketImpl::checkConnectionParams(quint16 minInterval,
                                          quint16 maxInterval,
                                          quint16 latency,
                                          quint16 supervisionTimeout) const
{
	if ((minInterval > maxInterval) || (minInterval < 6) || (maxInterval > 3200))
		return false;

	if ((supervisionTimeout < 10) || (supervisionTimeout > 3200))
		return false;

	if (maxInterval >= (supervisionTimeout * 8))
		return false;

	quint16 maxLatency = (supervisionTimeout * 8 / maxInterval) - 1;
	if ((latency > 499) || (latency > maxLatency))
		return false;

	return true;
}

// -----------------------------------------------------------------------------
/*!
	\fn bool HciSocket::requestConnectionUpdate(const BleConnectionParameters &params)

	Sends a request to the HCI device to update the connection parameters.

	\sa Volume 2, Part E, Section 7.7.65.1 LE Connection Complete Event
	of the Bluetooth Core Spec version 4.0

 */
bool HciSocketImpl::requestConnectionUpdate(quint16 connHandle,
                                            const BleConnectionParameters &params)
{
	// convert the parameters
	le_connection_update_cp connUpdate;
	bzero(&connUpdate, sizeof(connUpdate));

	connUpdate.min_interval        = quint16(params.minimumInterval() / 1.25f);
	connUpdate.max_interval        = quint16(params.maximumInterval() / 1.25f);
	connUpdate.latency             = quint16(params.latency());
	connUpdate.supervision_timeout = quint16(params.supervisionTimeout() / 10);

	// check the parameters
	if (!checkConnectionParams(connUpdate.min_interval, connUpdate.max_interval,
	                           connUpdate.latency, connUpdate.supervision_timeout)) {
		qWarning("invalid connection parameters, aborting request");
		return false;
	}

	// convert all params to little endian
	connUpdate.handle              = qToLittleEndian(connHandle);
	connUpdate.min_interval        = qToLittleEndian(connUpdate.min_interval);
	connUpdate.max_interval        = qToLittleEndian(connUpdate.max_interval);
	connUpdate.latency             = qToLittleEndian(connUpdate.latency);
	connUpdate.supervision_timeout = qToLittleEndian(connUpdate.supervision_timeout);

	// send the request to the kernel
	return sendCommand(OGF_LE_CTL, OCF_LE_CONN_UPDATE, &connUpdate,
	                   LE_CONN_UPDATE_CP_SIZE);
}

// -----------------------------------------------------------------------------
/*!
	\fn bool HciSocket::sendIncreaseDataCapability()

	Sends a request to the HCI device to send the VSC to increase data 
	capability for bluetooth
 */
bool HciSocketImpl::sendIncreaseDataCapability(quint16 connHandle)
{
	le_vsc_more_data_capability moreData;
	bzero(&moreData, sizeof(moreData));
	moreData.handle = qToLittleEndian(connHandle);
	moreData.level = OCF_LE_MORE_DATA_CAPABILITY_LEVEL;
	bool ret = sendCommand(OGF_LE_VSC, OCF_LE_MORE_DATA, &moreData, LE_MORE_DATA_VSC_SIZE);
	qWarning("Sent VSC MORE_DATA_CAPABILITY (0x%X) to handle: %d, return = %s", moreData.level, connHandle, ret ? "TRUE":"FALSE");
	return ret;
}

// -----------------------------------------------------------------------------
/*!
	\fn QList<HciSocket::ConnectedDeviceInfo> HciSocket::getConnectedDevices() const

	Returns a list of all the connected bluetooth LE devices.  On failure an
	empty list is returned, which is the same for if there are no actual
	attached devices.

 */
QList<HciSocket::ConnectedDeviceInfo> HciSocketImpl::getConnectedDevices() const
{
	QList<ConnectedDeviceInfo> devices;

	const quint16 maxConns = 10;

	// create a buffer to store all the results from the ioctl
	QByteArray data(sizeof(struct hci_conn_list_req) +
	                (maxConns * sizeof(struct hci_conn_info)), 0x00);

	struct hci_conn_list_req *req =
		reinterpret_cast<struct hci_conn_list_req*>(data.data());
	const struct hci_conn_info *info =
		reinterpret_cast<const struct hci_conn_info*>(data.data() + sizeof(struct hci_conn_list_req));

	req->dev_id = m_hciDeviceId;
	req->conn_num = maxConns;

	// request a list of the all the connections
	int ret = TEMP_FAILURE_RETRY(::ioctl(m_hciSocket, HCIGETCONNLIST, data.data()));
	if (ret != 0) {
		qErrnoWarning(errno, "HCIGETCONNLIST ioctl failed");
		return devices;
	}

	// append any devices to the list
	for (quint16 i = 0; i < req->conn_num; i++) {

		// we only care about bluetooth LE connections
		if (info[i].type != LE_LINK)
			continue;

		// get the bdaddr of the device
		BleAddress bdAddr(info[i].bdaddr.b, BleAddress::LSBOrder);
		ConnectedDeviceInfo bdInfo(std::move(bdAddr), info[i].handle,
		                           info[i].state, info[i].link_mode);

		// append the device
		devices.append(bdInfo);
	}

	return devices;
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Returns the string for the given error / status code returned by the HCI
	interface.

	\sa Volume 2, Part D, Section 1.3 'List of Error Codes' of the Bluetooth
	Core Spec version 4.0

 */
const char* HciSocketImpl::hciErrorString(quint8 code) const
{
	switch (code) {
		case 0x00:	return "Success";
		case 0x01:	return "Unknown HCI Command";
		case 0x02:	return "Unknown Connection Identifier";
		case 0x03:	return "Hardware Failure";
		case 0x04:	return "Page Timeout";
		case 0x05:	return "Authentication Failure";
		case 0x06:	return "PIN or Key Missing";
		case 0x07:	return "Memory Capacity Exceeded";
		case 0x08:	return "Connection Timeout";
		case 0x09:	return "Connection Limit Exceeded";
		case 0x0A:	return "Synchronous Connection Limit To A Device Exceeded";
		case 0x0B:	return "ACL Connection Already Exists";
		case 0x0C:	return "Command Disallowed";
		case 0x0D:	return "Connection Rejected due to Limited Resources";
		case 0x0E:	return "Connection Rejected Due To Security Reasons";
		case 0x0F:	return "Connection Rejected due to Unacceptable BD_ADDR";
		case 0x10:	return "Connection Accept Timeout Exceeded";
		case 0x11:	return "Unsupported Feature or Parameter Value";
		case 0x12:	return "Invalid HCI Command Parameters";
		case 0x13:	return "Remote User Terminated Connection";
		case 0x14:	return "Remote Device Terminated Connection due to Low Resources";
		case 0x15:	return "Remote Device Terminated Connection due to Power Off";
		case 0x16:	return "Connection Terminated By Local Host";
		case 0x17:	return "Repeated Attempts";
		case 0x18:	return "Pairing Not Allowed";
		case 0x19:	return "Unknown LMP PDU";
		case 0x1A:	return "Unsupported Remote Feature / Unsupported LMP Feature";
		case 0x1B:	return "SCO Offset Rejected";
		case 0x1C:	return "SCO Interval Rejected";
		case 0x1D:	return "SCO Air Mode Rejected";
		case 0x1E:	return "Invalid LMP Parameters / Invalid LL Parameters";
		case 0x1F:	return "Unspecified Error";
		case 0x20:	return "Unsupported LMP Parameter Value / Unsupported LL Parameter Value";
		case 0x21:	return "Role Change Not Allowed";
		case 0x22:	return "LMP Response Timeout / LL Response Timeout";
		case 0x23:	return "LMP Error Transaction Collision";
		case 0x24:	return "LMP PDU Not Allowed";
		case 0x25:	return "Encryption Mode Not Acceptable";
		case 0x26:	return "Link Key cannot be Changed";
		case 0x27:	return "Requested QoS Not Supported";
		case 0x28:	return "Instant Passed";
		case 0x29:	return "Pairing With Unit Key Not Supported";
		case 0x2A:	return "Different Transaction Collision";
		case 0x2C:	return "QoS Unacceptable Parameter";
		case 0x2D:	return "QoS Rejected";
		case 0x2E:	return "Channel Classification Not Supported";
		case 0x2F:	return "Insufficient Security";
		case 0x30:	return "Parameter Out Of Mandatory Range";
		case 0x32:	return "Role Switch Pending";
		case 0x34:	return "Reserved Slot Violation";
		case 0x35:	return "Role Switch Failed";
		case 0x36:	return "Extended Inquiry Response Too Large";
		case 0x37:	return "Secure Simple Pairing Not Supported By Host";
		case 0x38:	return "Host Busy - Pairing";
		case 0x39:	return "Connection Rejected due to No Suitable Channel Found";
		case 0x3A:	return "Controller Busy";
		case 0x3B:	return "Unacceptable Connection Parameters";
		case 0x3C:	return "Directed Advertising Timeout";
		case 0x3D:	return "Connection Terminated due to MIC Failure";
		case 0x3E:	return "Connection Failed to be Established";
		case 0x3F:	return "MAC Connection Failed";
		case 0x40:	return "Coarse Clock Adjustment Rejected but Will Try to Adjust Using Clock Dragging";
		default:	return "Unknown";
	}
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Called when an \c EVT_LE_META_EVENT event has been received and the sub
	event type is \c EVT_LE_CONN_COMPLETE.

	This event is sent after the HCI device has successifully connected to a
	remote device.

	\sa Volume 2, Part E, Section 7.7.65.1 LE Connection Complete Event
	of the Bluetooth Core Spec version 4.0

 */
void HciSocketImpl::onConnectionCompleteEvent(const evt_le_connection_complete *event)
{
	qDebug("EVT_LE_CONN_COMPLETE - { 0x%02hhx, %02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx, %hu, %hu, %hu, %hu }",
	       event->status,
	       event->peer_bdaddr.b[5], event->peer_bdaddr.b[4],
	       event->peer_bdaddr.b[3], event->peer_bdaddr.b[2],
	       event->peer_bdaddr.b[1], event->peer_bdaddr.b[0],
	       event->handle, event->interval, event->latency,
	       event->supervision_timeout);

	// check the status of the connection
	if (event->status != 0x00) {
		qWarning("connection failed (0x%02hhx - %s)", event->status,
		         hciErrorString(event->status));
		return;
	}

	// extract the remote device address
	BleAddress bdaddr(event->peer_bdaddr.b, BleAddress::LSBOrder);

	// extract the connection values
	const double intervalMs = double(event->interval) * 1.25;
	const int supervisionTimeoutMs = int(event->supervision_timeout) * 10;
	const int latency = event->latency;

	BleConnectionParameters params;
	params.setIntervalRange(intervalMs, intervalMs);
	params.setSupervisionTimeout(supervisionTimeoutMs);
	params.setLatency(latency);

	// finally emit the message
	emit connectionCompleted(qFromLittleEndian<quint16>(event->handle),
	                         bdaddr, params);
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Called when an \c EVT_LE_META_EVENT event has been received and the sub
	event type is \c EVT_LE_CONN_UPDATE_COMPLETE.

	This event is sent after the HCI device has update the connection parameters
	for a given connection.  We extract the fields and create an
	\l{HciConnectionParameters} object from them before emitting the
	\l{connectionUpdated,HciSocketImpl::connectionUpdated} signal.

	\sa Volume 2, Part E, Section 7.7.65.3 LE Connection Update Complete Event
	of the Bluetooth Core Spec version 4.0

 */
void HciSocketImpl::onUpdateCompleteEvent(const evt_le_connection_update_complete *event)
{
	qDebug("EVT_LE_CONN_UPDATE_COMPLETE - { 0x%02hhx, %hu, %hu, %hu, %hu }",
	       event->status, event->handle, event->interval, event->latency,
	       event->supervision_timeout);

	if (event->status != 0x00) {
		qWarning("update connection failed (0x%02hhx - %s)", event->status,
		         hciErrorString(event->status));
		return;
	}

	// extract the connection values
	const double intervalMs = double(event->interval) * 1.25;
	const int supervisionTimeoutMs = int(event->supervision_timeout) * 10;
	const int latency = event->latency;

	BleConnectionParameters params;
	params.setIntervalRange(intervalMs, intervalMs);
	params.setSupervisionTimeout(supervisionTimeoutMs);
	params.setLatency(latency);

	// finally emit the message
	emit connectionUpdated(qFromLittleEndian<quint16>(event->handle), params);
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Called when an \c EVT_DISCONN_COMPLETE event has been received.

	\sa Volume 2, Part E, 7.7.5 Disconnection Complete Event of the Bluetooth
	Core Spec version 4.0

 */
void HciSocketImpl::onDisconnectionCompleteEvent(const evt_disconn_complete *event)
{
	qDebug("EVT_DISCONN_COMPLETE - { 0x%02hhx, %hu, 0x%02hhx }",
	       event->status, event->handle, event->reason);

	if (event->status != 0x00) {
		qWarning("disconnection failed (0x%02hhx - %s)", event->status,
		         hciErrorString(event->status));
		return;
	}

	emit disconnectionComplete(qFromLittleEndian<quint16>(event->handle),
	                           HciStatus(event->reason));
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Called when the HCI socket is readable (i.e. an event is in the queue). The
	event is read and then passed onto one of the event handlers.

 */
void HciSocketImpl::onSocketActivated(int socket)
{
	if (Q_UNLIKELY(socket != m_hciSocket)) {
		qWarning("odd, socket doesn't match in notifier");
		return;
	}

	// read an event from the buffer
	quint8 buf[HCI_MAX_EVENT_SIZE];
	ssize_t len = TEMP_FAILURE_RETRY(recv(socket, buf, HCI_MAX_EVENT_SIZE, MSG_DONTWAIT));
	if (len < 0) {
		if ((errno != EAGAIN) && (errno != EWOULDBLOCK))
			qErrnoWarning(errno, "failed to read from hci socket");
		return;
	}

	if (len == 0) {
		qWarning("read from hci socket returned 0 bytes");
		return;
	}

	// check the message is an HCI event
	if (buf[0] != HCI_EVENT_PKT) {
		qInfo("odd, received non-event message even though it should be filtered out");
		return;
	}
	len -= HCI_TYPE_LEN;

	// check we have enough for the header
	if (len < HCI_EVENT_HDR_SIZE) {
		qWarning("read too short message from hci socket (read %zd bytes)", len);
		return;
	}
	len -= HCI_EVENT_HDR_SIZE;

	// get the message header and sanity check the length
	const hci_event_hdr *hdr = reinterpret_cast<const hci_event_hdr*>(buf + HCI_TYPE_LEN);
	if (hdr->plen != len) {
		qWarning("size of received event doesn't match header value");
		return;
	}


	// check if a disconnect event
	if (hdr->evt == EVT_DISCONN_COMPLETE) {

		if (len < EVT_DISCONN_COMPLETE_SIZE) {
			qWarning("disconnect event EVT_DISCONN_COMPLETE has invalid size "
			         "(expected:%u actual:%zd)", EVT_DISCONN_COMPLETE_SIZE, len);
			return;
		}

		const evt_disconn_complete *disconnEvent =
			reinterpret_cast<const evt_disconn_complete*>(buf + HCI_TYPE_LEN + HCI_EVENT_HDR_SIZE);

		onDisconnectionCompleteEvent(disconnEvent);


	// check if a meta event
	} else if (hdr->evt == EVT_LE_META_EVENT) {

		if (len < EVT_LE_META_EVENT_SIZE) {
			qWarning("le meta event EVT_LE_META_EVENT has invalid size "
			         "(expected:%u actual:%zd)", EVT_LE_META_EVENT_SIZE, len);
			return;
		}
		len -= EVT_LE_META_EVENT_SIZE;

		const evt_le_meta_event *metaEvt =
			reinterpret_cast<const evt_le_meta_event*>(buf + HCI_TYPE_LEN + HCI_EVENT_HDR_SIZE);

		if (metaEvt->subevent == EVT_LE_CONN_COMPLETE) {

			// sanity check the length of the sub event
			if (len < EVT_LE_CONN_COMPLETE_SIZE) {
				qWarning("le meta event EVT_LE_CONN_COMPLETE has invalid size "
				         "(expected:%u actual:%zd)", EVT_LE_CONN_COMPLETE_SIZE, len);
				return;
			}

			// pass the event onto the handler
			const evt_le_connection_complete *leConnComplt =
				reinterpret_cast<const evt_le_connection_complete*>(metaEvt->data);

			onConnectionCompleteEvent(leConnComplt);

		} else if (metaEvt->subevent == EVT_LE_CONN_UPDATE_COMPLETE) {

			// sanity check the length of the sub event
			if (len < EVT_LE_CONN_UPDATE_COMPLETE_SIZE) {
				qWarning("le meta event EVT_LE_CONN_UPDATE_COMPLETE_SIZE has invalid size"
				         "(expected:%u actual:%zd)", EVT_LE_CONN_UPDATE_COMPLETE_SIZE, len);
				return;
			}

			// pass the event onto the handler
			const evt_le_connection_update_complete *leUpdateComplt =
				reinterpret_cast<const evt_le_connection_update_complete*>(metaEvt->data);

			onUpdateCompleteEvent(leUpdateComplt);
		}
	}
}

