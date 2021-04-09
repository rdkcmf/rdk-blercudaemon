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
//  hidmonitor.cpp
//  BleRcuDaemon
//

#include "hidmonitor.h"
#include "utils/hidrawdevicemanager.h"
#include "utils/hidrawdevice.h"
#include "utils/logging.h"

#include <functional>


#define HID_REPORT          0
#define HID_DEVICE_ADDED    1
#define HID_DEVICE_REMOVED  2


struct hidsnoop_pkt {
	quint8  id;             // the id of the hid device (hidraw minor number)
	quint8  type;           // the type of event
	quint8  size;           // original length
	quint8  len;            // included length
	quint64 ts;             // timestamp microseconds
	quint8  data[0];        // hid report aata
} __attribute__ ((packed));
#define HIDSNOOP_PKT_SIZE (sizeof(struct hidsnoop_pkt))




// -----------------------------------------------------------------------------
/*!
	\class HidMonitor
	\brief Object that stores all the HID packets sent by all hidraw devices
	on the system.

	This class monitors the hidraw devices and records when they're added or
	removed from the system and the reports that they send.  Everything is
	stored in a circular buffer that when full overwrites the oldest record(s).
 
	The buffer can be cleared and dumped using the HidMonitor::dumpBuffer()
	method.
 
	\warning This class is not currently thread-safe, it's only designed to run
	in the context of the main event loop.

 */



// -----------------------------------------------------------------------------
/*!
	Constructs a monitor object with the given \a bufferSize.


 */
HidMonitor::HidMonitor(const QSharedPointer<HidRawDeviceManager> &hidRawManager,
                       size_t bufferSize, QObject *parent)
	: QObject(parent)
	, m_hidRawManager(hidRawManager)
	, m_snapLength(68)
	, m_buffer(bufferSize)
{

	// give up if no hidraw manager
	if (!m_hidRawManager)
		return;

	// observe the signals for hidraw devices comming / going
	QObject::connect(m_hidRawManager.data(), &HidRawDeviceManager::deviceAdded,
					 this, &HidMonitor::onDeviceAdded);
	QObject::connect(m_hidRawManager.data(), &HidRawDeviceManager::deviceRemoved,
					 this, &HidMonitor::onDeviceRemoved);

	// get the current list of devices
	const QSet<QByteArray> devices = m_hidRawManager->physicalAddresses();
	for (const QByteArray &phyAddress : devices)
		onDeviceAdded(phyAddress);

}

// -----------------------------------------------------------------------------
/*!
	Destructor


 */
HidMonitor::~HidMonitor()
{
	m_hidRawDevices.clear();
}

// -----------------------------------------------------------------------------
/*!
	Returns \c true if the buffer has been created and the hid manager is valid.

 */
bool HidMonitor::isValid() const
{
	return m_buffer.isValid() && !m_hidRawManager.isNull();
}

// -----------------------------------------------------------------------------
/*!
	Returns the snap length to use for captured data.

 */
int HidMonitor::snapLength() const
{
	return m_snapLength;
}

// -----------------------------------------------------------------------------
/*!
	Sets the snap length to \a length.  The snap length is clamped between 0 and
	254 bytes.

 */
void HidMonitor::setSnapLength(int length)
{
	m_snapLength = qBound<int>(0, length, 254);
}

// -----------------------------------------------------------------------------
/*!
	\internal
 
	Called when a new hidraw device is added to the system, the
	\a physicalAddress of the added device is supplied.

 */
void HidMonitor::onDeviceAdded(const QByteArray &physicalAddress)
{
	// try and open the hidraw device
	QSharedPointer<HidRawDevice> device = m_hidRawManager->open(physicalAddress);
	if (!device || !device->isValid())
		return;

	// set a fully permissive filter to capture all hid reports
	for (uint i = 0; i < 32; i++)
		device->enableReport(i);

	// create a functor for handling the reports, needed because we want to
	// bind in the device minor number
	const std::function<void(uint, const QByteArray&)> reportFn =
		std::bind(&HidMonitor::onReport, this, device->minorNumber(),
		          std::placeholders::_1, std::placeholders::_2);

	// connect to the report events from the device
	QObject::connect(device.data(), &HidRawDevice::report,
					 this, reportFn);

	// inject a device added event
	injectEvent(quint8(device->minorNumber()), HID_DEVICE_ADDED, physicalAddress);

	// add to internal list
	m_hidRawDevices.append(device);
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Called when a new hidraw device is removed from the system, the
	\a physicalAddress of the removed device is supplied.

 */
void HidMonitor::onDeviceRemoved(const QByteArray &physicalAddress)
{
	// find the device to remove from our internal map
	QList<QSharedPointer<HidRawDevice>>::iterator it = m_hidRawDevices.begin();
	while (it != m_hidRawDevices.end()) {

		const QSharedPointer<HidRawDevice> &device = *it;
		if (physicalAddress == device->physicalAddress()) {

			// inject a 'device removed' event
			injectEvent(quint8(device->minorNumber()), HID_DEVICE_REMOVED,
			            physicalAddress);

			// remove the device from the list
			it = m_hidRawDevices.erase(it);
		} else {
			++it;
		}
	}
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Called when a report is received on any of the hidraw devices that are
	managed.
 
	The \a minorNumber is the dev nodes minor number and used to identify which
	events are for which device.

 */
void HidMonitor::onReport(int minorNumber, uint reportId, const QByteArray &data)
{
	// trim the max report size to 64 bytes plus the report id
	const quint8 dataLen = static_cast<quint8>(qMin(data.length() + 1, m_snapLength));

	// allocate space in the event buffer
	quint8 *eventPtr = addEvent(quint8(minorNumber), HID_REPORT, dataLen);
	if (Q_UNLIKELY(eventPtr == nullptr))
		return;

	// populate the event with the report data
	eventPtr[0] = quint8(reportId);
	memcpy(eventPtr + 1, data.constData(), (dataLen - 1));
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Injects an event into the buffer with the given \a data.  If \a data is
	greater than 254 bytes it will be trimmed.

 */
void HidMonitor::injectEvent(quint8 minorNumber, quint8 type, const QByteArray &data)
{
	// trim the max report size to 254 bytes plus the report id
	const quint8 dataLen = static_cast<quint8>(qMin(data.length(), 254));

	// allocate space in the event buffer
	quint8 *eventPtr = addEvent(minorNumber, type, dataLen);
	if (Q_UNLIKELY(eventPtr == nullptr))
		return;

	// populate the data
	memcpy(eventPtr, data.constData(), dataLen);
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Reserves space in the buffer for \a amount number of bytes, if there is no
	free space then packets from the tail of the ring buffer are discarded.

 */
quint8* HidMonitor::reserveBufferSpace(size_t amount)
{
	while (m_buffer.space() < amount) {

		// get the last record from the buffer
		const struct hidsnoop_pkt *rec = m_buffer.tail<const struct hidsnoop_pkt>();
		size_t recLen = rec->len + HIDSNOOP_PKT_SIZE;

		// move to the next record
		m_buffer.advanceTail(recLen);
	}

	return m_buffer.head<quint8>();
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Adds an event header and reserves space in the buffer for \a size number
	of bytes.  A pointer to store the data is returned.

 */
quint8* HidMonitor::addEvent(quint8 minorNumber, quint8 type, quint8 size)
{
	if (Q_UNLIKELY(!m_buffer.isValid()))
		return nullptr;

	// reserve space in the buffer to write the event into
	quint8* data = reserveBufferSpace(HIDSNOOP_PKT_SIZE + size);

	// set the record header
	struct hidsnoop_pkt *record = reinterpret_cast<struct hidsnoop_pkt*>(data);
	bzero(record, HIDSNOOP_PKT_SIZE);

	record->id = minorNumber;
	record->type = type;
	record->size = size;
	record->len = size;

	struct timespec tv;
	clock_gettime(CLOCK_REALTIME, &tv);

	quint64 ts = (tv.tv_sec - 946684800ll) * 1000000ll + (tv.tv_nsec / 1000ll);
	record->ts = ts + 0x00E03AB44A676000ll;

	return data + HIDSNOOP_PKT_SIZE;
}

// -----------------------------------------------------------------------------
/*!
	Dumps the buffer to the \a output file or buffer. Returns the number of
	bytes written, or -1 if an error occurred.
 
	If \a clearBuffer is \c true (the default) the buffer will be cleared after
	the data is written to the ouput device.

 */
qint64 HidMonitor::dumpBuffer(QIODevice *output, bool clearBuffer)
{
	qint64 count = output->write(m_buffer.tail<const char>(), m_buffer.size());
	if (count != qint64(m_buffer.size()))
		count = -1;

	if (clearBuffer)
		m_buffer.clear();

	return count;
}
