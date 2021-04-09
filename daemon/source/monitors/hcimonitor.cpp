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
//  hcimonitor.cpp
//  BleRcuDaemon
//

#include "hcimonitor.h"
#include "hcimonitor_p.h"
#include "utils/linux/containerhelpers.h"
#include "utils/logging.h"

#include <QtEndian>
#include <QMutexLocker>

#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/uio.h>
#include <sys/poll.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

#if !defined(QT_NO_EVENTFD)
#include <sys/eventfd.h>
#endif


#if defined(__APPLE__)
#  define pipe2(a, b)  pipe(a)
#endif

#ifndef SOCK_CLOEXEC
#  define SOCK_CLOEXEC      0
#endif

#ifndef SOCK_NONBLOCK
#  define SOCK_NONBLOCK    0
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

//
#ifndef HCI_MAX_FRAME_SIZE
#  define HCI_MAX_FRAME_SIZE (1024 + 4)
#endif

// HCI Packet types
#define HCI_COMMAND_PKT     0x01
#define HCI_ACLDATA_PKT     0x02
#define HCI_SCODATA_PKT		0x03
#define HCI_EVENT_PKT		0x04
#define HCI_VENDOR_PKT		0xff


struct Q_PACKED btsnoop_hdr {
	quint8  id[8];          // identification pattern
	quint32 version;        // version number = 1
	quint32 type;           // datalink type
};
Q_STATIC_ASSERT(sizeof(btsnoop_hdr) == 16);
#define BTSNOOP_FILE_HDR_SIZE (sizeof(struct btsnoop_hdr))

struct Q_PACKED btsnoop_pkt {
	quint32 size;           // original length
	quint32 len;            // included length
	quint32 flags;          // packet flags
	quint32 drops;          // cumulative drops
	quint64 ts;             // timestamp microseconds
	quint8  data[0];        // packet aata
};
Q_STATIC_ASSERT(sizeof(btsnoop_pkt) == 24);
#define BTSNOOP_PKT_SIZE (sizeof(struct btsnoop_pkt))

static const quint8 btsnoop_id[] = { 0x62, 0x74, 0x73, 0x6e, 0x6f, 0x6f, 0x70, 0x00 };

struct Q_PACKED pktlog_hdr {
	quint32 len;
	quint64 ts;
	quint8  type;
};
Q_STATIC_ASSERT(sizeof(pktlog_hdr) == 13);
#define PKTLOG_HDR_SIZE (sizeof(struct pktlog_hdr))

struct hci_filter {
	quint32 type_mask;
	quint32 event_mask[2];
	quint16 opcode;
};

struct sockaddr_hci {
	sa_family_t hci_family;
	qint16 hci_dev;
	qint16 hci_channel;
};

// HCI CMSG flags
#define HCI_CMSG_DIR	0x0001
#define HCI_CMSG_TSTAMP	0x0002



// -----------------------------------------------------------------------------
/*!
	\class HciMonitor
	\brief Object that stores all the HCI packets sent over an hci device.

	The data is stored in an internal ram buffer that wraps and overwrites the
	oldest records when full.  Some of this functionality is duplicated in
	the HciSocket class, however I didn't want to pollute that class with
	what are essentially debug methods used by this class.

	The buffer can be cleared and dumped using the HciMonitor::dumpBuffer()
	method.

	The hci packets are stored in the BTSnoop record format and when dumped
	to a buffer they are prefixed with BTSnoop file header (although this can
	be omitted with functions args).  The BTSnoop file format is similar to
	RFC1761 and is reasonably well documented here:
	http://www.fte.com/webhelp/bpa600/Content/Technical_Information/BT_Snoop_File_Format.htm


 */



// -----------------------------------------------------------------------------
/*!
	Constructs the object which will open the HCI socket and create the ring
	buffer.  Use isValid() to determine if there was an error constructing the
	object.

	The \a deviceId is the number of the HCI device to open, this should be 0
	for Sky STBs as we only have one HCI device.  The \a netNsFd is the
	file descriptor to the root network namespace, if valid (>= 0) then the
	HCI monitor socket will be created in that namespace.

	The \a bufferSize must be at least 4K in size so that it can contain at
	least 2 HCI packets of the maximum size.

 */
HciMonitor::HciMonitor(uint deviceId, int netNsFd, size_t bufferSize)
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

	int opt = 1;
	if (setsockopt(sockFd, SOL_HCI, HCI_DATA_DIR, &opt, sizeof(opt)) < 0) {
		qErrnoWarning(errno, "failed to enable data direction info");
		close(sockFd);
		return;
	}

	opt = 1;
	if (setsockopt(sockFd, SOL_HCI, HCI_TIME_STAMP, &opt, sizeof(opt)) < 0) {
		qErrnoWarning(errno, "failed to enable time stamping");
		close(sockFd);
		return;
	}

	// setup the hci filter
	struct hci_filter filter;
	bzero(&filter, sizeof(struct hci_filter));

	// enable all events (for now?)
	filter.type_mask = 0xffffffff;
	filter.event_mask[0] = 0xffffffff;
	filter.event_mask[1] = 0xffffffff;

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
	if (bind(sockFd, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
		qErrnoWarning(errno, "failed to bind to hci%u", deviceId);
		close(sockFd);
		return;
	}


	// finally create the private object which takes ownership of the socket
	_d = new HciMonitorPrivate(sockFd, bufferSize);

	// and then start it
	_d->start();
}

// -----------------------------------------------------------------------------
/*!
	Constructor intended to be used only for unit testing, rather than open
	and setup an HCI socket it simply dup's the \a hciSocketFd socket.

 */
HciMonitor::HciMonitor(int hciSocketFd, size_t bufferSize)
	: _d(nullptr)
{
	// dup the socket
	int sockFd = fcntl(hciSocketFd, F_DUPFD_CLOEXEC, 3);
	if (sockFd < 0) {
		qErrnoWarning(errno, "failed to dup hci socket");
		return;
	}

	//  make it non-blocking
	int flags = fcntl(sockFd, F_GETFL, 0);
	fcntl(sockFd, F_SETFL, flags | O_NONBLOCK);


	// finally create the private object which takes ownership of the socket
	_d = new HciMonitorPrivate(sockFd, bufferSize);

	// start the monitor thread
	_d->start();
}

// -----------------------------------------------------------------------------
/*!
	Destructor, closes the internal hci socket and frees the ring buffer.

 */
HciMonitor::~HciMonitor()
{
	if (_d != nullptr)
		delete _d;
}

// -----------------------------------------------------------------------------
/*!
	Returns \c true if the buffer has been created and the hci monitor socket
	was created.

 */
bool HciMonitor::isValid() const
{
	return (_d != nullptr);
}

// -----------------------------------------------------------------------------
/*!
	Returns the snap length to use for captured data.

 */
int HciMonitor::snapLength() const
{
	if (Q_UNLIKELY(_d == nullptr))
		return -1;
	else
		return _d->snapLength();
}

// -----------------------------------------------------------------------------
/*!
	Sets the snap length to \a length.  The snap length is clamped between 0 and
	\c HCI_MAX_FRAME_SIZE bytes.

 */
void HciMonitor::setSnapLength(int length)
{
	if (Q_LIKELY(_d != nullptr))
		_d->setSnapLength(length);
}

// -----------------------------------------------------------------------------
/*!
	Clears the monitor buffer of all data.

 */
void HciMonitor::clear()
{
	if (Q_LIKELY(_d != nullptr))
		_d->clear();
}

// -----------------------------------------------------------------------------
/*!
	Dumps the buffer to the \a output file or buffer. Returns the number of
	bytes written, or -1 if an error occurred.

	If \a includeHeader is \c true (the default) then a BTSnoop file header will
	be written to \a output before the actual records.

	If \a clearBuffer is \c true the buffer will be cleared after the data is
	written to the ouput device.

 */
qint64 HciMonitor::dumpBuffer(QIODevice *output, bool includeHeader,
                              bool clearBuffer)
{
	if (Q_UNLIKELY(_d == nullptr))
		return -1;
	else
		return _d->dumpBuffer(output, includeHeader, clearBuffer);
}




HciMonitorPrivate::HciMonitorPrivate(int hciSocketFd, size_t bufferSize,
                                     QObject *parent)
	: QThread(parent)
	, m_hciSocketFd(hciSocketFd)
	, m_snapLength(HCI_MAX_FRAME_SIZE)
	, m_buffer(bufferSize)
	, m_deathFd(-1)
{

	// give this object a name, which in turn means the thread spawned will have
	// the same name
	setObjectName(QStringLiteral("HciMonitor"));

#if !defined(QT_NO_EVENTFD)
	// create the fd used to terminate the thread poll loop
	m_deathFd = eventfd(0, EFD_CLOEXEC);
	if (m_deathFd < 0) {
		qErrnoWarning(errno, "failed to create eventfd for thread notification");
		return;
	}
#endif

}

// -----------------------------------------------------------------------------
/*!
	Terminates the thread if running.  This may block for 5 seconds waiting for
	the thread to terminate.

 */
HciMonitorPrivate::~HciMonitorPrivate()
{
	if (isRunning()) {

		// signal the eventfd to terminate the thread
		if (m_deathFd >= 0) {
			uint64_t value = 1;
			if (TEMP_FAILURE_RETRY(write(m_deathFd, &value, sizeof(value))) != sizeof(value))
				qErrnoWarning(errno, "failed to write eventfd to wake thread");
		}

		// wait for the thread to finish
		if (wait(5000) == false) {
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
	if ((m_hciSocketFd >= 0) && (close(m_hciSocketFd) != 0))
		qErrnoWarning(errno, "failed to close hci socket");

	m_deathFd = m_hciSocketFd = -1;
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Returns the snap length to use for captured data.
 */
int HciMonitorPrivate::snapLength() const
{
	QMutexLocker locker(&m_lock);
	return m_snapLength;
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Sets the snap length to \a length.  The snap length is clamped between 0 and
	\c HCI_MAX_FRAME_SIZE bytes.
 */
void HciMonitorPrivate::setSnapLength(int length)
{
	QMutexLocker locker(&m_lock);
	m_snapLength = qBound<int>(0, length, HCI_MAX_FRAME_SIZE);
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Clears the monitor buffer of all data.
 */
void HciMonitorPrivate::clear()
{
	QMutexLocker locker(&m_lock);
	m_buffer.clear();
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Dumps the buffer to the \a output file or buffer. Returns the number of
	bytes written, or -1 if an error occurred.

	If \a includeHeader is \c true then a BTSnoop file header will be written
	to \a output before the actual records.

	If \a clearBuffer is \c true (the default) the buffer will be cleared after
	the data is written to the ouput device.

 */
qint64 HciMonitorPrivate::dumpBuffer(QIODevice *output, bool includeHeader,
                                     bool clearBuffer)
{
	qint64 total = 0;

	// write out the BTSnoop header
	if (includeHeader) {

		struct btsnoop_hdr header;
		bzero(&header, sizeof(header));

		memcpy(header.id, btsnoop_id, sizeof(btsnoop_id));
		header.version = qToBigEndian<quint32>(1);
		header.type = qToBigEndian<quint32>(1002);

		qint64 wr = output->write(reinterpret_cast<const char*>(&header),
		                          BTSNOOP_FILE_HDR_SIZE);
		if (wr != BTSNOOP_FILE_HDR_SIZE) {
			qError("failed to write btsnoop header");
			return -1;
		}

		total += wr;
	}

	// take the lock while reading the buffer
	QMutexLocker locker(&m_lock);

	// write out the records in the buffer
	if (!m_buffer.isEmpty()) {

		const char *data = m_buffer.tail<const char>();
		qint64 dataLen = m_buffer.size();

		qDebug("%lld bytes in hci monitor buffer", dataLen);

		while (dataLen > 0) {

			qint64 wr = output->write(data, dataLen);
			if (wr <= 0) {
				qWarning("failed to write hci data to output file");
				return -1;;
			}

			total += wr;

			data += wr;
			dataLen -= wr;
		}

	}

	// clear the buffer if asked to
	if (clearBuffer)
		m_buffer.clear();

	return total;
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Reserves space in the buffer for \a amount number of bytes, if there is no
	free space then packets from the tail of the ring buffer are discarded.

 */
quint8* HciMonitorPrivate::reserveBufferSpace(size_t amount)
{
	while (m_buffer.space() < amount) {

		// get the last record from the buffer
		const struct btsnoop_pkt *record = m_buffer.tail<const struct btsnoop_pkt>();
		size_t recLen = qFromBigEndian<quint32>(record->len) + BTSNOOP_PKT_SIZE;

		// move to the next record
		m_buffer.advanceTail(recLen);
	}

	return m_buffer.head<quint8>();
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Reads a single HCI message from the socket, returns \c true if a packet
	was read.

	Returns \c false if there was an error reading the socket, otherwise
	\c true is returned (an empty read is not considered an error).

 */
bool HciMonitorPrivate::readHciPacket()
{
	// take the lock while writing to the buffer
	QMutexLocker locker(&m_lock);

	// reserve some space in the buffer for the max packet size plus the
	// BTSnoop record header
	quint8* bufferPtr = reserveBufferSpace(HCI_MAX_FRAME_SIZE + BTSNOOP_PKT_SIZE);


	// read a packet
	struct iovec iv;
	iv.iov_base = bufferPtr + BTSNOOP_PKT_SIZE;
	iv.iov_len  = HCI_MAX_FRAME_SIZE;

	struct msghdr msg;
	msg.msg_iov = &iv;
	msg.msg_iovlen = 1;
	msg.msg_control = m_controlBuffer;
	msg.msg_controllen = sizeof(m_controlBuffer);

	ssize_t len = TEMP_FAILURE_RETRY(recvmsg(m_hciSocketFd, &msg, MSG_DONTWAIT));
	if (Q_UNLIKELY(len == 0)) {
		qWarning("read an empty packet from the hci monitor socket");
		return true;

	} else if (len < 0) {
		if (errno != EAGAIN)
			qErrnoWarning(errno, "failed to receive hci message");

		return (errno == EAGAIN);
	}


	// populate the packet record header
	struct btsnoop_pkt *record = reinterpret_cast<struct btsnoop_pkt*>(bufferPtr);
	bzero(record, BTSNOOP_PKT_SIZE);

	record->size = qToBigEndian<quint32>(quint32(len));
	size_t actualLen = qMin<size_t>(len, m_snapLength);
	record->len = qToBigEndian<quint32>(quint32(actualLen));

	if ((record->data[0] == HCI_COMMAND_PKT) ||
	    (record->data[0] == HCI_EVENT_PKT)) {
		record->flags |= qToBigEndian<quint32>(0x02);
	}


	// process control message
	struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
	while (cmsg) {

		switch (cmsg->cmsg_type) {
			case HCI_CMSG_DIR:
			{
				int dir;
				memcpy(&dir, CMSG_DATA(cmsg), sizeof(int));

				if ((dir & 0xff) != 0x00)
					record->flags |= qToBigEndian<quint32>(1);
				break;
			}

			case HCI_CMSG_TSTAMP:
			{
				struct timeval tv;
				memcpy(&tv, CMSG_DATA(cmsg), sizeof(struct timeval));

				quint64 ts = (tv.tv_sec - 946684800ll) * 1000000ll + tv.tv_usec;
				record->ts = qToBigEndian<quint64>(ts + 0x00E03AB44A676000ll);
				break;
			}
		}

		cmsg = CMSG_NXTHDR(&msg, cmsg);
	}

	// move the head pointer to the first spot after the record header + data
	m_buffer.advanceHead(BTSNOOP_PKT_SIZE + actualLen);

	return true;
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Thread function, this runs the poll loop and only exits when someone
	triggers the eventfd.

 */
void HciMonitorPrivate::run()
{
	qInfo("entering hci monitor thread");

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

	fds[1].fd = m_hciSocketFd;
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
			if (readHciPacket() == false) {
				qWarning("hci socket read failed, quitting monitor event loop");
				break;
			}
		}

	}

	qInfo("exiting hci monitor thread");
}

