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
//  fwimagefile.cpp
//  SkyBluetoothRcu
//

#include "fwimagefile.h"

#include "logging.h"
#include "crc32.h"

#include <QtEndian>
#include <QBuffer>
#include <QFile>

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>



// -----------------------------------------------------------------------------
/*!
	\class FwImageFile
	\brief Wraps a QIODevice object that contains a f/w image file.

	Basic utility object to abstract away some of the details of a f/w image
	file and to perform the integrity checks on said files.

 */



/// The format of the expect firmware file header
struct Q_PACKED FwFileHeader {
	quint32 hwIdent;
	quint32 fwImageLength;
	quint32 fwImageVersion;
	quint32 fwImageCrc32;
};

/// The maximum number of data bytes in a DATA packet
#define FIRMWARE_PACKET_MTU           18


// -----------------------------------------------------------------------------
/*!
	Constructs a FwImageFile object by wrapping the supplied \a data.

	Use isValid() to determine if the data is a valid f/w image file.
 */
FwImageFile::FwImageFile(const QByteArray &data)
	: m_valid(false)
{
	// create and copy the data into the buffer
	QBuffer *buffer = new QBuffer();
	buffer->setData(data);
	buffer->open(QIODevice::ReadOnly);

	// store the buffer object
	m_file.reset(buffer);

	// check the file header / contents
	m_valid = checkFile();
	if (!m_valid)
		m_file.reset();
}

// -----------------------------------------------------------------------------
/*!
	Constructs a FwImageFile object by attempting to open the file at the given
	path.  Use isValid() to determine if the file could be opened and is a
	valid f/w image file.


 */
FwImageFile::FwImageFile(const QString &filePath)
	: m_valid(false)
{
	QFile *file = new QFile(filePath);

	// try and open the file
	if (!file->open(QFile::ReadOnly)) {
		qWarning("failed to open fw file @ '%s'", qPrintable(filePath));
		m_error = file->errorString();
		delete file;
		return;
	}

	// store the file object
	m_file.reset(file);

	// sanity check the file is not a pipe or something we can't seek on
	if (m_file->isSequential()) {
		m_error = QStringLiteral("Invalid f/w file");
		m_file.reset();
		return;
	}

	// check the file header / contents
	m_valid = checkFile();
	if (!m_valid)
		m_file.reset();
}

// -----------------------------------------------------------------------------
/*!
	Constructs a FwImageFile object wrapping the file descriptor, internally the
	file descriptor is dup'd so it should be closed by the caller after the
	object is created.

	To check whether the file is valid use isValid().

 */
FwImageFile::FwImageFile(int fd)
	: m_valid(false)
{
	// check the file descriptor by duping it
	int fd_ = fcntl(fd, F_DUPFD_CLOEXEC, 3);
	if (fd_ < 0) {
		qErrnoWarning(errno, "failed to dup supplied fd");
		m_error = QStringLiteral("Failed to access f/w file");
		return;
	}

	// check the fd corresponds to a real file, not a fifo or something else
	struct stat stat;
	if ((fstat(fd_, &stat) != 0) || !S_ISREG(stat.st_mode)) {
		qWarning("supplied fd doesn't correspond to a regular file");
		m_error = QStringLiteral("Invalid f/w file");

		if (::close(fd_) != 0)
			qErrnoWarning(errno, "failed to close fd");

		return;
	}

	QFile *file = new QFile;

	// wrap the file descriptor in a QFile, the QFile will take ownership of
	// the file descriptor
	if (!file->open(fd_, QFile::ReadOnly, QFile::AutoCloseHandle)) {
		qWarning("failed to open f/w file due to %s",
		         qPrintable(file->errorString()));
		delete file;

		m_error = QStringLiteral("Failed to open f/w file");

		if (::close(fd_) != 0)
			qErrnoWarning(errno, "failed to close fd");

		return;
	}

	// take ownership of the file
	m_file.reset(file);

	// check the file header / contents
	m_valid = checkFile();
	if (!m_valid)
		m_file.reset();
}

FwImageFile::~FwImageFile()
{
}

// -----------------------------------------------------------------------------
/*!
	\internal

	Checks the open file has the correct header and the crc32 checksum matches.

 */
bool FwImageFile::checkFile()
{
	if (Q_UNLIKELY(!m_file->isReadable())) {
		m_error = QStringLiteral("Failed to read f/w file");
		return false;
	}


	// get the size of the file, we may potential have a problem if the size
	// of the file would mean that the block ids would wrap, for now we
	// ignore that
	const qint64 fileSize = m_file->size();
	if (fileSize > (0x3fff * FIRMWARE_PACKET_MTU)) {
		m_error = QStringLiteral("Firmware file is too large");
		return false;
	}
	if (fileSize <= qint64(sizeof(FwFileHeader))) {
		m_error = QStringLiteral("Firmware file is empty");
		return false;
	}


	// read the file header and verify the crc matches
	FwFileHeader header;
	qint64 read = m_file->read(reinterpret_cast<char*>(&header),
	                           sizeof(FwFileHeader));
	if (read != qint64(sizeof(FwFileHeader))) {
		m_error = QStringLiteral("Firmware file header error");
		return false;
	}

	// convert from little endian just in case we ever run on a big endain
	m_hardwareVersion = qFromLittleEndian(header.hwIdent);
	m_firmwareVersion = qFromLittleEndian(header.fwImageVersion);
	m_firmwareSize = qFromLittleEndian(header.fwImageLength);
	m_firmwareCrc = qFromLittleEndian(header.fwImageCrc32);

	// check the length in the header matches
	if (m_firmwareSize != (fileSize - sizeof(FwFileHeader))) {
		m_error = QStringLiteral("Firmware file header length error");
		return false;
	}

	// calculate the crc over the rest of the file after the header
	Crc32 fileCrc;
	fileCrc.addData(m_file.data());

	if (m_firmwareCrc != fileCrc.result()) {
		m_error = QStringLiteral("Firmware file header crc error");
		return false;
	}


	// now need to rewind the file to the point right after the header
	return m_file->seek(sizeof(FwFileHeader));
}

// -----------------------------------------------------------------------------
/*!
	Returns \c true if the file is valid and the f/w header checks out with
	the contents of the file; otherwise \c false.

 */
bool FwImageFile::isValid() const
{
	return m_valid;
}

// -----------------------------------------------------------------------------
/*!
	Returns a human-readable description of the last device error that occurred.

 */
QString FwImageFile::errorString() const
{
	return m_error;
}

// -----------------------------------------------------------------------------
/*!
	Returns the manufacturer id byte this firmware image file is targeting.

	If the firmware file is not valid this method returns an undefined value.
 */
quint8 FwImageFile::manufacturerId() const
{
	if (Q_UNLIKELY(!m_valid))
		return 0x00;

	return quint8((m_hardwareVersion >> 24) & 0xff);
}

// -----------------------------------------------------------------------------
/*!
	Returns the hardware version this firmware image file is targeting.

	If the firmware file is not valid then a null version is returned.
 */
QVersionNumber FwImageFile::hwVersion() const
{
	if (Q_UNLIKELY(!m_valid))
		return QVersionNumber();

	return QVersionNumber(int((m_hardwareVersion >> 16) & 0xff),
	                      int((m_hardwareVersion >> 8) & 0xff),
	                      int((m_hardwareVersion >> 0) & 0xff));
}

// -----------------------------------------------------------------------------
/*!
	Returns the version of the firmware in the image file.

	If the firmware file is not valid then a null version is returned.
 */
QVersionNumber FwImageFile::version() const
{
	if (Q_UNLIKELY(!m_valid))
		return QVersionNumber();

	return QVersionNumber(int((m_firmwareVersion >> 16) & 0xffff),
	                      int((m_firmwareVersion >> 8) & 0xff),
	                      int((m_firmwareVersion >> 0) & 0xff));
}

// -----------------------------------------------------------------------------
/*!
	Returns the CRC32 checksum of the f/w data.

	If the firmware file is not valid then the return value is undefined.
 */
quint32 FwImageFile::crc32() const
{
	if (Q_UNLIKELY(!m_valid))
		return 0;

	return m_firmwareCrc;
}

// -----------------------------------------------------------------------------
/*!
	Returns the current position within the f/w file.  This is excluding the
	header, i.e. a position of 0 means we're at the start of the firmware image
	data.

 */
qint64 FwImageFile::pos() const
{
	if (Q_UNLIKELY(!m_valid))
		return -1;

	return m_file->pos() - sizeof(FwFileHeader);
}

// -----------------------------------------------------------------------------
/*!
	Returns \c true if the end of the file has been reached; otherwise returns
	\c false.

 */
bool FwImageFile::atEnd() const
{
	if (Q_UNLIKELY(!m_valid))
		return true;

	return m_file->atEnd();
}

// -----------------------------------------------------------------------------
/*!
	Seeks to a position within the image data segment, ie. seeking to postion
	0 will be at the first byte in the f/w data image, not the first byte in
	the file.

 */
bool FwImageFile::seek(qint64 pos)
{
	if (Q_UNLIKELY(!m_valid))
		return false;
	if (Q_UNLIKELY(pos < 0))
		return false;

	return m_file->seek(sizeof(FwFileHeader) + pos);
}

// -----------------------------------------------------------------------------
/*!
	Returns the size of the f/w image data, not the size of the file.  I.e. this
	is the size of the image to transfer to the RCU.

 */
qint64 FwImageFile::size() const
{
	if (Q_UNLIKELY(!m_valid))
		return -1;

	return m_firmwareSize;
}

// -----------------------------------------------------------------------------
/*!
	Reads data from the firmware data segment starting at the current position.

 */
qint64 FwImageFile::read(void *data, qint64 len)
{
	if (Q_UNLIKELY(!m_valid))
		return -1;

	return m_file->read(reinterpret_cast<char*>(data), len);
}

