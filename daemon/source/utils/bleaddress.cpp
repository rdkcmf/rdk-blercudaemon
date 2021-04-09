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
//  bleaddress.cpp
//  SkyBluetoothRcu
//

#include "bleaddress.h"

#include <QDebug>

// -----------------------------------------------------------------------------
/*!
	\class BleAddress
	\brief Implementation modelled on QBluetoothAddress

	\ingroup utils

	Provides an object for parsing and storing bluetooth MAC addresses (BDADDR).
	The object can be used as a key in a QMap or as an object in QSet, in
	addition it provides toString() implementations and debug operators.

	A default constructed BleAddress object will be invalid and return \c true
	if isNull() is called.  If constructed with a QString users should check
	that the string was successifully parsed by running a isNull() check after
	construction.

	\note Our current version of Qt (5.6) doesn't have the bluetooth libraries,
	hence why this is needed.

 */



#define INVALID_ADDRESS  0xffffffffffffffffULL


void BleAddress::registerType()
{
	static bool initDone = false;
	if (Q_UNLIKELY(!initDone)) {
		qRegisterMetaType<BleAddress>();
		initDone = true;
	}
}



BleAddress::BleAddress()
	: m_address(INVALID_ADDRESS)
{
	registerType();
}

BleAddress::BleAddress(const quint8 address[6], AddressOrder order)
	: m_address(INVALID_ADDRESS)
{
	if (order == MSBOrder) {
		m_address = (quint64(address[0]) << 40) |
		            (quint64(address[1]) << 32) |
		            (quint64(address[2]) << 24) |
		            (quint64(address[3]) << 16) |
		            (quint64(address[4]) << 8) |
		            (quint64(address[5]) << 0);
	} else {
		m_address = (quint64(address[5]) << 40) |
		            (quint64(address[4]) << 32) |
		            (quint64(address[3]) << 24) |
		            (quint64(address[2]) << 16) |
		            (quint64(address[1]) << 8) |
		            (quint64(address[0]) << 0);
	}

	registerType();
}

BleAddress::BleAddress(quint64 address)
	: m_address(address)
{
	if ((m_address >> 48) != 0)
		m_address = INVALID_ADDRESS;
	else if (m_address == 0)
		m_address = INVALID_ADDRESS;
	else if (m_address == 0xffffffffffffULL)
		m_address = INVALID_ADDRESS;

	registerType();
}

BleAddress::BleAddress(const QString &address)
{
	m_address = _fromString(qPrintable(address), address.length());

	registerType();
}

BleAddress::BleAddress(const char *address)
{
	m_address = _fromString(address, qstrlen(address));

	registerType();
}

BleAddress::BleAddress(QLatin1String address)
{
	m_address = _fromString(address.data(), address.size());

	registerType();
}

BleAddress::BleAddress(const BleAddress &other)
	: m_address(other.m_address)
{
	registerType();
}

BleAddress::~BleAddress()
{
}

BleAddress & BleAddress::operator=(const BleAddress &rhs)
{
	m_address = rhs.m_address;
	return *this;
}

BleAddress & BleAddress::operator=(const QLatin1String &rhs)
{
	m_address = _fromString(rhs.data(), rhs.size());
	return *this;
}

BleAddress & BleAddress::operator=(const QString &rhs)
{
	m_address = _fromString(qPrintable(rhs), rhs.length());
	return *this;
}

void BleAddress::clear()
{
	m_address = INVALID_ADDRESS;
}

bool BleAddress::isNull() const
{
	return (m_address == INVALID_ADDRESS);
}

quint64 BleAddress::_fromString(const char *address, int length) const
{
	// convert to a ASCII string and then sanity check it
	if (length != 17)
		return INVALID_ADDRESS;

	for (int i = 0; i < length; ++i) {

		if (((i + 1) % 3) == 0) {
			// every 3rd char must be ':'
			if (address[i] != ':')
				return INVALID_ADDRESS;
		} else {
			// all other chars must be a hex digit
			if (!isxdigit(address[i]))
				return INVALID_ADDRESS;
		}
	}

	// extract the fields of the string
	quint8 bytes[6];
	sscanf(address, "%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx",
	       &bytes[0], &bytes[1], &bytes[2], &bytes[3], &bytes[4], &bytes[5]);

	return (quint64(bytes[0]) << 40) |
	       (quint64(bytes[1]) << 32) |
	       (quint64(bytes[2]) << 24) |
	       (quint64(bytes[3]) << 16) |
	       (quint64(bytes[4]) << 8) |
	       (quint64(bytes[5]) << 0);
}

const char* BleAddress::_toString(char buf[32]) const
{
	if (m_address == INVALID_ADDRESS)
		return nullptr;

	sprintf(buf, "%02hhX:%02hhX:%02hhX:%02hhX:%02hhX:%02hhX",
	        quint8((m_address >> 40) & 0xff),
	        quint8((m_address >> 32) & 0xff),
	        quint8((m_address >> 24) & 0xff),
	        quint8((m_address >> 16) & 0xff),
	        quint8((m_address >> 8)  & 0xff),
	        quint8((m_address >> 0)  & 0xff));
	return buf;
}

// -----------------------------------------------------------------------------
/*!
	Returns the MAC address as a string in the standard format.

	If the address is not valid then an empty string is returned.
 */
QString BleAddress::toString() const
{
	if (m_address == INVALID_ADDRESS)
		return QString();
	else {
		char buf[32];
		return QString(_toString(buf));
	}
}

// -----------------------------------------------------------------------------
/*!
	Returns the MAC address in the lower 48-bits of the returned value.

	If the address is not valid then 0 is returned.
 */
quint64 BleAddress::toUInt64() const
{
	if (m_address == INVALID_ADDRESS)
		return 0;
	else
		return m_address;
}

// -----------------------------------------------------------------------------
/*!
	Returns the MAC address as an array of 6 byte values.  The most significant
	byte will be an index 0 and the least significant at index 5.

	If the address is not valid then an empty array is returned.
 */
QVarLengthArray<quint8, 6> BleAddress::toArray() const
{
	if (Q_UNLIKELY(m_address == INVALID_ADDRESS))
		return QVarLengthArray<quint8, 6>(0);

	QVarLengthArray<quint8, 6> bdaddr(6);

	bdaddr[0] = quint8((m_address >> 40) & 0xff);
	bdaddr[1] = quint8((m_address >> 32) & 0xff);
	bdaddr[2] = quint8((m_address >> 24) & 0xff);
	bdaddr[3] = quint8((m_address >> 16) & 0xff);
	bdaddr[4] = quint8((m_address >> 8)  & 0xff);
	bdaddr[5] = quint8((m_address >> 0)  & 0xff);

	return bdaddr;
}

// -----------------------------------------------------------------------------
/*!
	Accesses the individual bytes in the address.  Index \c 0 is the most
	significant byte, and \a index 5 is the least significant.

	\a index values less than 0 or greater than 5 are invalid and will always
	return 0.

 */
quint8 BleAddress::operator[](int index) const
{
	Q_ASSERT(index >= 0 && index < 6);

	if (Q_UNLIKELY(m_address == INVALID_ADDRESS))
		return 0x00;

	switch (index) {
		case 0:
			return quint8((m_address >> 40) & 0xff);
		case 1:
			return quint8((m_address >> 32) & 0xff);
		case 2:
			return quint8((m_address >> 24) & 0xff);
		case 3:
			return quint8((m_address >> 16) & 0xff);
		case 4:
			return quint8((m_address >> 8) & 0xff);
		case 5:
			return quint8((m_address >> 0) & 0xff);
		default:
			return 0x00;
	}
}

// -----------------------------------------------------------------------------
/*!
	Returns the 24-bit OUI (Organizationally Unique Identifier) part of the
	address.  The OUI is the most significant 3 bytes of the address and is
	equivalent to doing the following:

	\code
		BleRcuAddress bdaddr("11:22:33:44:55:66");
		quint64 oui = (bdaddr.toUInt64() >> 24) & 0xffffff;
	\endcode

	If the address is not valid then 0 is returned.
 */
quint32 BleAddress::oui() const
{
	if (m_address == INVALID_ADDRESS)
		return 0;

	return quint32((m_address >> 24) & 0xffffff);
}

QDebug operator<<(QDebug dbg, const BleAddress &address)
{
	if (address.isNull()) {
		dbg << "00:00:00:00:00:00";
	} else {
		char buf[32];
		dbg << address._toString(buf);
	}

	return dbg;
}
