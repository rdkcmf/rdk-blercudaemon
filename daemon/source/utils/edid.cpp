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
//  edid.cpp
//  SkyBluetoothRcu
//

#include "edid.h"


// -----------------------------------------------------------------------------
/*!
	Creates an invalid EDID, isValid() will return \c false.

 */
Edid::Edid()
	: m_valid(false)
{
	bzero(m_data, sizeof(m_data));
}

// -----------------------------------------------------------------------------
/*!
	Copy constructor, if \a other is invalid then the constructed EDID will
	also be invalid.

 */
Edid::Edid(const Edid &other)
	: m_valid(other.m_valid)
{
	memcpy(m_data, other.m_data, sizeof(m_data));
}

// -----------------------------------------------------------------------------
/*!
	Copies the EDID data from \a rhs into this EDID, this may invalidate the
	object if \a rhs is invalid.

 */
Edid &Edid::operator=(const Edid &rhs)
{
	m_valid = rhs.m_valid;
	memcpy(m_data, rhs.m_data, sizeof(m_data));

	return *this;
}

// -----------------------------------------------------------------------------
/*!
	Constructs a new EDID object from the \a data supplied.  The \a data must
	be at least 128 bytes in size otherwise an invalid EDID would be created.

	If the supplied \a data is checked to ensure that it has the correct EDID
	header and the checksum is valid.  If these checks fail then an invalid
	EDID object is created and isValid() will return \c false.

 */
Edid::Edid(const QByteArray &data)
	: m_valid(false)
{
	// sanity check we have the enough data for the edid
	if (Q_UNLIKELY(data.length() < (int)sizeof(m_data))) {
		qWarning("data to short for EDID");
		return;
	}

	// check the edid is valid before copying in the data
	m_valid = checkEdid(data);
	if (m_valid)
		memcpy(m_data, data.constData(), sizeof(m_data));
	else
		bzero(m_data, sizeof(m_data));
}

// -----------------------------------------------------------------------------
/*!
	Returns \c true if \a lhs is not equal to \a rhs; otherwise returns \c false.
 */
bool operator!=(const Edid &lhs, const Edid &rhs)
{
	if (!lhs.m_valid || !rhs.m_valid)
		return true;

	return (memcmp(lhs.m_data, rhs.m_data, sizeof(lhs.m_data)) != 0);
}

// -----------------------------------------------------------------------------
/*!
	Returns \c true if \a lhs is equal to \a rhs; otherwise returns \c false.
 */
bool operator==(const Edid &lhs, const Edid &rhs)
{
	if (!lhs.m_valid || !rhs.m_valid)
		return false;

	return (memcmp(lhs.m_data, rhs.m_data, sizeof(lhs.m_data)) == 0);
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Runs basic checks on the \a data to verify it's an EDID.

	\see https://en.wikipedia.org/wiki/Extended_Display_Identification_Data

 */
bool Edid::checkEdid(const QByteArray &data)
{
	const quint8 *data_ = reinterpret_cast<const quint8*>(data.constData());

	// check the header matches
	static const quint8 header[] = { 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00 };
	if (memcmp(data_, header, sizeof(header)) != 0) {
		qWarning("edid has malformed header");
		return false;
	}

	// check bit 15 of the manufacturer id is not set
	if (data_[8] & 0x80) {
		qWarning("edid has malformed manufacturer id field");
		return false;
	}

	// check the checksum
	quint32 checksum = 0;
	for (unsigned i = 0; i < 128; i++)
		checksum += data_[i];

	if ((checksum & 0xff) != 0x00) {
		qWarning("edid has incorrect checksum");
		return false;
	}

	return true;
}

// -----------------------------------------------------------------------------
/*!
	Returns \c true if the edid is valid, i.e. it was constructed with a
	correctly formatted and checksumed data block.

 */
bool Edid::isValid() const
{
	return m_valid;
}

// -----------------------------------------------------------------------------
/*!
	Returns the raw EDID data.

	If not valid then an empty byte array is returned.

	\sa isValid()
 */
QByteArray Edid::data() const
{
	if (!m_valid)
		return QByteArray();

	return QByteArray(reinterpret_cast<const char*>(m_data), sizeof(m_data));
}

// -----------------------------------------------------------------------------
/*!
	Returns the version number of the EDID, the typical value is 1.3.

	If not valid then a null version number is returned.

	\sa isValid()
 */
QVersionNumber Edid::version() const
{
	if (!m_valid)
		return QVersionNumber();

	return QVersionNumber(m_data[18], m_data[19]);
}

// -----------------------------------------------------------------------------
/*!
	Converts the \a pnpId to it's 3 character string representation.

	\sa stringToPnPId()
 */
QString Edid::pnpIdToString(quint16 pnpId)
{
	const char id[3] = {
		char( 'A' - 1 + ((pnpId >> 10) & 0x1f) ),
		char( 'A' - 1 + ((pnpId >>  5) & 0x1f) ),
		char( 'A' - 1 + ((pnpId >>  0) & 0x1f) )
	};

	return QLatin1String(id ,3);
}

// -----------------------------------------------------------------------------
/*!
	Converts the \a str of 3 characters to it's 16-bit PnP representation.

	If \a str is not at least 3 characters and each of the characters is not
	in the ASCII range of \c 'A' to \c 'Z' then \c 0x0000 is returned.

	\sa pnpIdToString()
 */
quint16 Edid::stringToPnPId(const QString &str)
{
	const QByteArray ascii = str.toLatin1();

	if (ascii.length() < 3)
		return 0;

	if ( ((ascii[0] < 'A') || (ascii[0] > 'Z')) ||
	     ((ascii[1] < 'A') || (ascii[1] > 'Z')) ||
	     ((ascii[2] < 'A') || (ascii[2] > 'Z')) ) {
		return 0;
	}

	return ((quint16(ascii[0]) - 'A' + 1) << 10) |
	       ((quint16(ascii[1]) - 'A' + 1) << 5) |
	       ((quint16(ascii[2]) - 'A' + 1) << 0);
}

// -----------------------------------------------------------------------------
/*!
	Returns the manufacturer id field of the EDID.

	If not valid then an empty string is returned.

	\sa pnpId(), isValid()
 */
QString Edid::manufacturerId() const
{
	if (!m_valid)
		return QString();

	const quint16 id = quint16(m_data[8]) << 8 |
	                   quint16(m_data[9]) << 0;

	return pnpIdToString(id);
}

// -----------------------------------------------------------------------------
/*!
	Returns the manufacturer PnP id field of the EDID.

	If not valid then \c 0x0000 is returned

	\sa manufacturerId(), isValid()
 */
quint16 Edid::pnpId() const
{
	if (!m_valid)
		return 0;

	return quint16(m_data[8]) << 8 |
	       quint16(m_data[9]) << 0;
}

// -----------------------------------------------------------------------------
/*!
	Returns the 16-bit product code of the EDID.

	If not valid then \c 0x0000 is returned

	\sa isValid()
 */
quint16 Edid::productCode() const
{
	if (!m_valid)
		return 0;

	return quint16(m_data[10]) << 0 |
	       quint16(m_data[11]) << 8;
}

// -----------------------------------------------------------------------------
/*!
	Returns the 32-bit serial number of the EDID.

	If not valid then \c 0 is returned

	\sa isValid()
 */
quint32 Edid::serialNumber() const
{
	if (!m_valid)
		return 0;

	return quint32(m_data[12]) << 0  |
	       quint32(m_data[13]) << 8  |
	       quint32(m_data[14]) << 16 |
	       quint32(m_data[15]) << 24;
}
